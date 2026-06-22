#include "xbox_splitscreen.h"

#include "xbox_debug.h"
#include "Cog/sithCog.h"
#include "Devices/sithControl.h"
#include "Dss/sithMulti.h"
#include "Gameplay/jkSaber.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithPlayer.h"
#include "General/stdPalEffects.h"
#include "Main/Main.h"
#include "Main/jkDev.h"
#include "Main/jkHud.h"
#include "Main/jkHudCameraView.h"
#include "Main/jkHudInv.h"
#include "Main/jkHudScope.h"
#include "Platform/stdControl.h"
#include "Platform/std3D.h"
#include "Win95/Video.h"
#include "Win95/stdDisplay.h"
#include "World/jkPlayer.h"
#include "World/sithModel.h"
#include "World/sithSoundClass.h"
#include "World/sithTemplate.h"
#include "World/sithThing.h"
#include "World/sithWeapon.h"
#include "World/sithWorld.h"
#include "Win95/stdComm.h"
#include "Engine/rdroid.h"
#include "Engine/rdColormap.h"
#include "Engine/sithCamera.h"
#include "Engine/sithRender.h"
#include "Main/sithMain.h"
#include "stdPlatform.h"
#include "jk.h"

#ifdef __cplusplus
extern "C" {
#endif
void std3D_XboxSetViewport(int x, int y, int w, int h);
void std3D_XboxResetViewport(void);
void std3D_XboxBeginViewportUI(int x, int y, int w, int h);
void std3D_XboxEndViewportUI(void);
#ifdef __cplusplus
}
#endif

static int g_xboxSplitScreenEnabled = 0;
static int g_xboxSplitScreenRequested = 0;
static int g_xboxSplitScreenLocalCount = 1;
static int g_xboxSplitScreenRequestedLocalCount = XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS;
static int g_xboxSplitScreenControllerForSlot[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {0, 1, 2, 3};
static wchar_t g_xboxSplitScreenPendingMpcNames[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS][32] = {{0}};
static int g_xboxSplitScreenPendingMpcSet[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {0};
static wchar_t g_xboxSplitScreenLocalNames[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS][32] = {{0}};
static int g_xboxSplitScreenLoggedViewports = 0;
static int g_xboxSplitScreenLoggedSlots = 0;
static unsigned int g_xboxSplitScreenFrameCount = 0;
static unsigned int g_xboxSplitScreenRespawnAt[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {0};
static int g_xboxSplitScreenCameraIdx[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {0};
static int g_xboxSplitScreenCurrentSlot = 0;
static int g_xboxSplitScreenInControlTick = 0;

static int xboxSplitScreen_ClampSlot(int slot)
{
    if (slot < 0)
        slot = 0;
    if (slot >= g_xboxSplitScreenLocalCount)
        slot = g_xboxSplitScreenLocalCount - 1;
    if (slot < 0)
        slot = 0;
    if (slot >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        slot = XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS - 1;
    return slot;
}

static void xboxSplitScreen_ApplyCameraForSlot(int slot)
{
    int camIdx;

    slot = xboxSplitScreen_ClampSlot(slot);
    camIdx = g_xboxSplitScreenCameraIdx[slot] ? 1 : 0;
    sithCamera_curCameraIdx = camIdx;
    sithCamera_SetCurrentCamera(&sithCamera_cameras[camIdx ? 1 : 0]);
}

static void xboxSplitScreen_SaveCameraForSlot(int slot)
{
    slot = xboxSplitScreen_ClampSlot(slot);
    g_xboxSplitScreenCameraIdx[slot] = (sithCamera_curCameraIdx != 0) ? 1 : 0;
}

static void xboxSplitScreen_SendJoinForSlot(int slot)
{
    sithThing *player;

    slot = xboxSplitScreen_ClampSlot(slot);
    player = jkPlayer_playerInfos[slot].playerThing;
    if (!sithNet_isServer || !player)
        return;

    XDBGF("SplitScreenJoin: slot=%d thing=%p thingIdx=%d netId=%d team=%d flags=0x%X\n",
          slot,
          (void*)player,
          player->thingIdx,
          jkPlayer_playerInfos[slot].net_id,
          jkPlayer_playerInfos[slot].teamNum,
          jkPlayer_playerInfos[slot].flags);
    sithCog_SendSimpleMessageToAll(SITH_MESSAGE_JOIN, 3, player->thingIdx, 0, slot);
}

static void xboxSplitScreen_SetInventoryBinQuiet(sithThing *player, int binIdx, flex_t amount, int stateFlags)
{
    sithPlayerInfo *info;
    sithItemDescriptor *desc;
    sithItemInfo *item;

    if (!player || binIdx < 0 || binIdx >= SITHBIN_NUMBINS)
        return;
    info = player->actorParams.playerinfo;
    if (!info || info == (sithPlayerInfo*)-136)
        return;
    desc = &sithInventory_aDescriptors[binIdx];
    if ((desc->flags & ITEMINFO_VALID) == 0)
        return;

    if (amount < desc->ammoMin)
        amount = desc->ammoMin;
    if (amount > desc->ammoMax)
        amount = desc->ammoMax;

    item = &info->iteminfo[binIdx];
    item->ammoAmt = amount;
    item->state |= stateFlags;
}

static void xboxSplitScreen_SeedPlayerWeapon(sithThing *player)
{
    sithPlayerInfo *info;
    int curWeapon;

    if (!player || player->type != SITH_THING_PLAYER || !player->actorParams.playerinfo)
        return;

    info = player->actorParams.playerinfo;
    xbox_debug_Printf("MPLoadTrace: SplitScreen seed weapon enter player=%p info=%p cur=%d pov=%p\n",
                      (void*)player,
                      (void*)info,
                      info ? info->curWeapon : -1,
                      player->playerInfo ? (void*)player->playerInfo->povModel.model3 : 0);

    xboxSplitScreen_SetInventoryBinQuiet(player, SITHBIN_FISTS, 1.0f, ITEMSTATE_AVAILABLE | ITEMSTATE_CARRIES);
    xboxSplitScreen_SetInventoryBinQuiet(player, SITHBIN_BRYARPISTOL, 1.0f, ITEMSTATE_AVAILABLE | ITEMSTATE_CARRIES);
    xboxSplitScreen_SetInventoryBinQuiet(player, SITHBIN_ENERGY, 50.0f, 0);

    curWeapon = info->curWeapon;
    if (curWeapon == SITHBIN_NONE ||
        curWeapon < 0 ||
        curWeapon >= SITHBIN_NUMBINS ||
        (sithInventory_aDescriptors[curWeapon].flags & ITEMINFO_WEAPON) == 0)
    {
        info->curWeapon = SITHBIN_BRYARPISTOL;
    }

    if (sithInventory_GetCurWeapon(player) != SITHBIN_BRYARPISTOL ||
        !player->playerInfo ||
        !player->playerInfo->povModel.model3)
    {
        sithWeapon_StartupEntry();
        sithInventory_SetCurWeapon(player, SITHBIN_FISTS);
        sithWeapon_SelectWeapon(player, SITHBIN_BRYARPISTOL, 0);
        sithWeapon_handle_inv_msgs(player);
    }

    xbox_debug_Printf("MPLoadTrace: SplitScreen seed weapon done cur=%d fistsState=0x%X bryarState=0x%X\n",
                      info->curWeapon,
                      info->iteminfo[SITHBIN_FISTS].state,
                      info->iteminfo[SITHBIN_BRYARPISTOL].state);
}

static void xboxSplitScreen_ClearLocalInvulnerability(void)
{
    int i;
    for (i = 0; i < g_xboxSplitScreenLocalCount; i++)
    {
        if (jkPlayer_playerInfos[i].playerThing)
            jkPlayer_playerInfos[i].playerThing->thingflags &= ~SITH_TF_INVULN;
    }
}

static void xboxSplitScreen_RestoreLocalName(int slot)
{
    if (slot < 0 || slot >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        return;
    if (!g_xboxSplitScreenLocalNames[slot][0])
        return;

    _wcsncpy(jkPlayer_playerInfos[slot].player_name, g_xboxSplitScreenLocalNames[slot], 31);
    jkPlayer_playerInfos[slot].player_name[31] = 0;
    _wcsncpy(jkPlayer_playerInfos[slot].multi_name, g_xboxSplitScreenLocalNames[slot], 31);
    jkPlayer_playerInfos[slot].multi_name[31] = 0;
}

int xboxSplitScreen_IsEnabled(void)
{
    return g_xboxSplitScreenEnabled;
}

int xboxSplitScreen_IsRequested(void)
{
    return g_xboxSplitScreenRequested;
}

int xboxSplitScreen_GetLocalPlayerCount(void)
{
    return g_xboxSplitScreenLocalCount;
}

static void xboxSplitScreen_ApplyMpcInfo(sithThing *player, jkPlayerMpcInfo *info)
{
    rdModel3 *model;
    sithSoundClass *soundclass;
    sithThing *saberSparks;
    sithThing *bloodSparks;
    sithThing *wallSparks;

    if (!player || !info)
        return;

    model = sithModel_LoadEntry(info->model, 1);
    if (model)
        sithThing_SetNewModel(player, model);

    soundclass = sithSoundClass_LoadFile(info->soundClass);
    if (soundclass)
        sithSoundClass_SetThingSoundClass(player, soundclass);

    if (player->playerInfo)
    {
        saberSparks = sithTemplate_GetEntryByName("+ssparks_saber");
        bloodSparks = sithTemplate_GetEntryByName("+ssparks_blood");
        wallSparks = sithTemplate_GetEntryByName("+ssparks_wall");
        jkSaber_InitializeSaberInfo(player, info->sideMat, info->tipMat, 0.0032f, 0.0018f, 0.12f, wallSparks, bloodSparks, saberSparks);
    }
}

int xboxSplitScreen_GetRequestedLocalPlayerCount(void)
{
    int count = g_xboxSplitScreenRequestedLocalCount;

    if (count < 1)
        count = 1;
    if (count > XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        count = XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS;
    return count;
}

void xboxSplitScreen_SetRequestedLocalPlayerCount(int count)
{
    if (count < 1)
        count = 1;
    if (count > XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        count = XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS;
    g_xboxSplitScreenRequestedLocalCount = count;
}

void xboxSplitScreen_SetPendingController(int slot, int controllerPort)
{
    if (slot < 0 || slot >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        return;
    if (controllerPort < 0 || controllerPort >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        controllerPort = slot;
    g_xboxSplitScreenControllerForSlot[slot] = controllerPort;
}

void xboxSplitScreen_SetPendingMpc(int slot, const wchar_t *name)
{
    if (slot < 0 || slot >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        return;

    g_xboxSplitScreenPendingMpcSet[slot] = 0;
    g_xboxSplitScreenPendingMpcNames[slot][0] = 0;
    if (!name || !name[0])
        return;

    _wcsncpy(g_xboxSplitScreenPendingMpcNames[slot], name, 31);
    g_xboxSplitScreenPendingMpcNames[slot][31] = 0;
    g_xboxSplitScreenPendingMpcSet[slot] = 1;
}

void xboxSplitScreen_Enable(void)
{
    g_xboxSplitScreenRequested = 1;
}

void xboxSplitScreen_Disable(void)
{
    int i;

    if (g_xboxSplitScreenEnabled || g_xboxSplitScreenLocalCount != 1)
        XDBG("SplitScreen: disabled\n");

    g_xboxSplitScreenRequested = 0;
    g_xboxSplitScreenEnabled = 0;
    g_xboxSplitScreenLocalCount = 1;
    g_xboxSplitScreenRequestedLocalCount = XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS;
    g_xboxSplitScreenLoggedViewports = 0;
    g_xboxSplitScreenLoggedSlots = 0;
    g_xboxSplitScreenFrameCount = 0;
    g_xboxSplitScreenCurrentSlot = 0;
    g_xboxSplitScreenInControlTick = 0;
    for (i = 0; i < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; i++)
    {
        g_xboxSplitScreenCameraIdx[i] = 0;
        g_xboxSplitScreenRespawnAt[i] = 0;
        g_xboxSplitScreenLocalNames[i][0] = 0;
        g_xboxSplitScreenControllerForSlot[i] = i;
        g_xboxSplitScreenPendingMpcSet[i] = 0;
        g_xboxSplitScreenPendingMpcNames[i][0] = 0;
    }

    stdControl_XboxSetActiveController(0);
    std3D_XboxResetViewport();
}

void xboxSplitScreen_OnMultiplayerServerStarted(void)
{
    int i;
    int count = xboxSplitScreen_GetRequestedLocalPlayerCount();

    xbox_debug_Printf("MPLoadTrace: SplitScreen server-start enter requested=%d reqCount=%d maxPlayers=%d\n",
                      g_xboxSplitScreenRequested,
                      count,
                      jkPlayer_maxPlayers);

    if (!g_xboxSplitScreenRequested)
    {
        g_xboxSplitScreenEnabled = 0;
        g_xboxSplitScreenLocalCount = 1;
        g_xboxSplitScreenLoggedViewports = 0;
        g_xboxSplitScreenLoggedSlots = 0;
        stdControl_XboxSetActiveController(0);
        std3D_XboxResetViewport();
        XDBG("SplitScreen: multiplayer server started without local split request\n");
        return;
    }

    if (count > jkPlayer_maxPlayers)
        count = jkPlayer_maxPlayers;
    if (count < 1)
        count = 1;

    g_xboxSplitScreenLocalCount = count;
    g_xboxSplitScreenEnabled = 0;
    g_xboxSplitScreenCurrentSlot = 0;
    g_xboxSplitScreenInControlTick = 0;

    for (i = 0; i < count; i++)
    {
        int haveMpc = 0;
        jkPlayerMpcInfo mpcInfo;
        g_xboxSplitScreenCameraIdx[i] = 0;

        xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d begin thing=%p sector=%p pendingMpc=%d\n",
                          i,
                          (void*)jkPlayer_playerInfos[i].playerThing,
                          jkPlayer_playerInfos[i].playerThing ? (void*)jkPlayer_playerInfos[i].playerThing->sector : 0,
                          g_xboxSplitScreenPendingMpcSet[i]);

        if (!jkPlayer_playerInfos[i].playerThing || !jkPlayer_playerInfos[i].playerThing->sector)
        {
            xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d skipped missing player thing/sector\n", i);
            continue;
        }

        xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d before net activate\n", i);
        sithPlayer_sub_4C87C0(i, i + 1);
        jkPlayer_playerInfos[i].teamNum = 0;
        xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d before context\n", i);
        xboxSplitScreen_SetContextForLocalSlot(i);
        xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d after context\n", i);
        if (g_xboxSplitScreenPendingMpcSet[i])
        {
            xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d before MPC parse\n", i);
            haveMpc = jkPlayer_MPCParse(&mpcInfo, &jkPlayer_playerInfos[i], jkPlayer_playerShortName, g_xboxSplitScreenPendingMpcNames[i], 1);
            xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d after MPC parse result=%d\n", i, haveMpc);
        }

        if (haveMpc)
        {
            _wcsncpy(jkPlayer_playerInfos[i].player_name, mpcInfo.name, 31);
            jkPlayer_playerInfos[i].player_name[31] = 0;
            _wcsncpy(jkPlayer_playerInfos[i].multi_name, mpcInfo.name, 31);
            jkPlayer_playerInfos[i].multi_name[31] = 0;
            _wcsncpy(g_xboxSplitScreenLocalNames[i], mpcInfo.name, 31);
            g_xboxSplitScreenLocalNames[i][31] = 0;
            xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d before MPC apply\n", i);
            xboxSplitScreen_ApplyMpcInfo(jkPlayer_playerInfos[i].playerThing, &mpcInfo);
            xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d after MPC apply\n", i);
        }
        else
        {
            jk_snwprintf(jkPlayer_playerInfos[i].player_name, 32, L"Xbox P%d", i + 1);
            jk_snwprintf(jkPlayer_playerInfos[i].multi_name, 32, L"Xbox P%d", i + 1);
            jk_snwprintf(g_xboxSplitScreenLocalNames[i], 32, L"Xbox P%d", i + 1);
        }

        xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d before seed weapon\n", i);
        xboxSplitScreen_SeedPlayerWeapon(jkPlayer_playerInfos[i].playerThing);
        xbox_debug_Printf("MPLoadTrace: SplitScreen slot %d after seed weapon\n", i);
        XDBGF("SplitScreenInit: slot=%d controller=%d thing=%p mpc=%d sector=%p curW=%d pov=%p flags=0x%X tf=0x%X\n",
              i,
              g_xboxSplitScreenControllerForSlot[i],
              (void*)jkPlayer_playerInfos[i].playerThing,
              haveMpc,
              jkPlayer_playerInfos[i].playerThing ? (void*)jkPlayer_playerInfos[i].playerThing->sector : 0,
              jkPlayer_playerInfos[i].playerThing ? sithInventory_GetCurWeapon(jkPlayer_playerInfos[i].playerThing) : -1,
              jkPlayer_playerInfos[i].playerThing && jkPlayer_playerInfos[i].playerThing->playerInfo
                  ? (void*)jkPlayer_playerInfos[i].playerThing->playerInfo->povModel.model3 : 0,
              jkPlayer_playerInfos[i].flags,
              jkPlayer_playerInfos[i].playerThing ? jkPlayer_playerInfos[i].playerThing->thingflags : 0);
    }

    xbox_debug_Printf("MPLoadTrace: SplitScreen before restore slot 0\n");
    xboxSplitScreen_SetContextForLocalSlot(0);
    xbox_debug_Printf("MPLoadTrace: SplitScreen before reset pal effects\n");
    sithPlayer_ResetPalEffects();
    xbox_debug_Printf("MPLoadTrace: SplitScreen before clear invuln\n");
    xboxSplitScreen_ClearLocalInvulnerability();

    xbox_debug_Printf("MPLoadTrace: SplitScreen server-start done locals=%d enabled=%d maxPlayers=%d\n",
                      g_xboxSplitScreenLocalCount,
                      g_xboxSplitScreenEnabled,
                      jkPlayer_maxPlayers);

    XDBGF("SplitScreen: local players=%d enabled=%d maxPlayers=%d awaiting post-load init\n",
          g_xboxSplitScreenLocalCount, g_xboxSplitScreenEnabled, jkPlayer_maxPlayers);
}

void xboxSplitScreen_SetContextForLocalSlot(int slot)
{
    slot = xboxSplitScreen_ClampSlot(slot);
    g_xboxSplitScreenCurrentSlot = slot;

    if (jkPlayer_playerInfos[slot].playerThing && jkPlayer_playerInfos[slot].playerThing->sector)
    {
        sithPlayer_idk(slot);
        xboxSplitScreen_RestoreLocalName(slot);
        sithCamera_SetsFocus();
        xboxSplitScreen_ApplyCameraForSlot(slot);
        xboxSplitScreen_ClearLocalInvulnerability();
    }
}

void xboxSplitScreen_RestoreContext(void)
{
    xboxSplitScreen_SetContextForLocalSlot(0);
}

void xboxSplitScreen_SetContextForControllerPort(int controllerPort)
{
    int i;

    if (!g_xboxSplitScreenEnabled)
        return;

    for (i = 0; i < g_xboxSplitScreenLocalCount; i++)
    {
        if (g_xboxSplitScreenControllerForSlot[i] == controllerPort)
        {
            xboxSplitScreen_SetContextForLocalSlot(i);
            return;
        }
    }
}

int xboxSplitScreen_GetCurrentControllerPort(void)
{
    int slot = xboxSplitScreen_ClampSlot(g_xboxSplitScreenCurrentSlot);
    int controllerPort = g_xboxSplitScreenControllerForSlot[slot];

    if (controllerPort < 0 || controllerPort >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        controllerPort = slot;
    return controllerPort;
}

int xboxSplitScreen_IsInControlTick(void)
{
    return g_xboxSplitScreenInControlTick;
}

void xboxSplitScreen_PostLoadInitializeLocals(void)
{
    int i;

    if (!g_xboxSplitScreenRequested)
        return;

    if (g_xboxSplitScreenLocalCount < 1)
        g_xboxSplitScreenLocalCount = 1;
    if (g_xboxSplitScreenLocalCount > jkPlayer_maxPlayers)
        g_xboxSplitScreenLocalCount = jkPlayer_maxPlayers;
    if (g_xboxSplitScreenLocalCount < 1)
        g_xboxSplitScreenLocalCount = 1;

    XDBGF("SplitScreenPostLoad: begin locals=%d maxPlayers=%d\n",
          g_xboxSplitScreenLocalCount,
          jkPlayer_maxPlayers);

    for (i = 0; i < g_xboxSplitScreenLocalCount; i++)
    {
        sithThing *player = jkPlayer_playerInfos[i].playerThing;

        if (!player || !player->sector)
        {
            XDBGF("SplitScreenPostLoad: skip slot=%d thing=%p sector=%p\n",
                  i,
                  (void*)player,
                  player ? (void*)player->sector : 0);
            continue;
        }

        sithPlayer_sub_4C87C0(i, i + 1);
        xboxSplitScreen_SetContextForLocalSlot(i);
        sithInventory_ClearInventory(player);
        jkPlayer_MpcInitBins(&jkPlayer_playerInfos[i]);
        sithCamera_SetsFocus();
        sithPlayer_debug_ToNextCheckpoint(player);
        xboxSplitScreen_SeedPlayerWeapon(player);
        sithMulti_SendWelcome(stdComm_dplayIdSelf, i, -1);
        sithMulti_SendWelcome(stdComm_dplayIdSelf, i, -1);
        if (i > 0)
            xboxSplitScreen_SendJoinForSlot(i);
        xboxSplitScreen_SaveCameraForSlot(i);
        g_xboxSplitScreenRespawnAt[i] = 0;

        XDBGF("SplitScreenPostLoad: slot=%d thing=%p sector=%p curW=%d pov=%p flags=0x%X tf=0x%X\n",
              i,
              (void*)player,
              (void*)player->sector,
              sithInventory_GetCurWeapon(player),
              player->playerInfo ? (void*)player->playerInfo->povModel.model3 : 0,
              jkPlayer_playerInfos[i].flags,
              player->thingflags);
    }

    xboxSplitScreen_SetContextForLocalSlot(0);
    sithPlayer_ResetPalEffects();
    xboxSplitScreen_ClearLocalInvulnerability();
    xboxSplitScreen_ResetViewport();
    g_xboxSplitScreenLoggedViewports = 0;
    g_xboxSplitScreenLoggedSlots = 0;
    g_xboxSplitScreenFrameCount = 0;
    g_xboxSplitScreenEnabled = (g_xboxSplitScreenLocalCount > 1);

    XDBGF("SplitScreenPostLoad: armed enabled=%d locals=%d\n",
          g_xboxSplitScreenEnabled,
          g_xboxSplitScreenLocalCount);
}

void xboxSplitScreen_BeginControlFrame(void)
{
    stdControl_ReadControls();
}

void xboxSplitScreen_TickControls(float deltaSecs, int deltaMs)
{
    int i;
    int connectedMask = stdControl_XboxGetConnectedMask();

    for (i = 0; i < g_xboxSplitScreenLocalCount; i++)
    {
        if (!jkPlayer_playerInfos[i].playerThing)
            continue;

        if (jkPlayer_playerInfos[i].playerThing->thingflags & SITH_TF_DEAD)
        {
            if (!g_xboxSplitScreenRespawnAt[i])
            {
                g_xboxSplitScreenRespawnAt[i] = sithTime_curMs + 3000;
                XDBGF("SplitScreenRespawn: slot=%d queued at=%u now=%u flags=0x%X\n",
                      i,
                      g_xboxSplitScreenRespawnAt[i],
                      sithTime_curMs,
                      (unsigned)jkPlayer_playerInfos[i].playerThing->thingflags);
            }
            else if (sithTime_curMs >= g_xboxSplitScreenRespawnAt[i])
            {
                xboxSplitScreen_SetContextForLocalSlot(i);
                sithPlayer_debug_ToNextCheckpoint(jkPlayer_playerInfos[i].playerThing);
                xboxSplitScreen_SeedPlayerWeapon(jkPlayer_playerInfos[i].playerThing);
                XDBGF("SplitScreenRespawn: slot=%d respawned thing=%p flags=0x%X sector=%p\n",
                      i,
                      (void*)jkPlayer_playerInfos[i].playerThing,
                      (unsigned)jkPlayer_playerInfos[i].playerThing->thingflags,
                      (void*)jkPlayer_playerInfos[i].playerThing->sector);
                g_xboxSplitScreenRespawnAt[i] = 0;
            }
        }
        else
        {
            g_xboxSplitScreenRespawnAt[i] = 0;
        }

        {
            int controllerPort = g_xboxSplitScreenControllerForSlot[i];
            if (controllerPort < 0 || controllerPort >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
                controllerPort = i;

            if ((connectedMask & (1 << controllerPort)) == 0)
            {
                static int s_loggedSkippedPads = 0;
                if (s_loggedSkippedPads < 8)
                {
                    XDBGF("SplitScreenCtl: skip slot=%d controller=%d no controller mask=0x%X\n", i, controllerPort, connectedMask);
                    s_loggedSkippedPads++;
                }
                continue;
            }

            stdControl_XboxSetActiveController(controllerPort);
            xboxSplitScreen_SetContextForLocalSlot(i);
            g_xboxSplitScreenInControlTick = 1;
            sithControl_Tick(deltaSecs, deltaMs);
            g_xboxSplitScreenInControlTick = 0;
            xboxSplitScreen_SaveCameraForSlot(i);
        }
    }

    g_xboxSplitScreenInControlTick = 0;
    stdControl_XboxSetActiveController(0);
    xboxSplitScreen_RestoreContext();
}

void xboxSplitScreen_EndControlFrame(void)
{
    stdControl_FinishRead();
}

void xboxSplitScreen_GetViewport(int slot, int *x, int *y, int *w, int *h)
{
    int vx = 0, vy = 0, vw = 640, vh = 480;

    if (g_xboxSplitScreenLocalCount == 2)
    {
        vw = 640;
        vh = 240;
        vy = (slot == 0) ? 240 : 0;
    }
    else if (g_xboxSplitScreenLocalCount >= 3)
    {
        vw = 320;
        vh = 240;
        vx = (slot & 1) ? 320 : 0;
        vy = (slot & 2) ? 0 : 240;
    }

    if (x) *x = vx;
    if (y) *y = vy;
    if (w) *w = vw;
    if (h) *h = vh;

    if (!g_xboxSplitScreenLoggedViewports && g_xboxSplitScreenEnabled)
    {
        int i;
        int lx, ly, lw, lh;
        g_xboxSplitScreenLoggedViewports = 1;
        for (i = 0; i < g_xboxSplitScreenLocalCount; i++)
        {
            lx = 0; ly = 0; lw = 640; lh = 480;
            if (g_xboxSplitScreenLocalCount == 2)
            {
                lw = 640; lh = 240; ly = (i == 0) ? 240 : 0;
            }
            else if (g_xboxSplitScreenLocalCount >= 3)
            {
                lw = 320; lh = 240; lx = (i & 1) ? 320 : 0; ly = (i & 2) ? 0 : 240;
            }
            XDBGF("SplitScreenViewport: slot=%d gl=(%d,%d %dx%d)\n", i, lx, ly, lw, lh);
        }
    }
}

void xboxSplitScreen_ApplyViewport(int slot)
{
    int x, y, w, h;
    xboxSplitScreen_GetViewport(slot, &x, &y, &w, &h);
    std3D_XboxSetViewport(x, y, w, h);
}

void xboxSplitScreen_ResetViewport(void)
{
    std3D_XboxResetViewport();
}

static void xboxSplitScreen_DrawHudForCurrentPlayer(void)
{
    if (!Main_bMotsCompat)
    {
        if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_NOHUD) == 0)
            jkHud_Draw();
        return;
    }

    if (playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_SCOPEHUD)
        jkHudScope_Draw();

    if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_80000000) == 0)
    {
        if ((playerThings[playerThingIdx].actorThing->actorParams.typeflags & SITH_AF_NOHUD) == 0)
            jkHud_Draw();
    }
    else
    {
        jkHudCameraView_Draw();
    }
}

static void xboxSplitScreen_ApplyColorEffects(void)
{
    stdPalEffects_UpdatePalette(stdDisplay_GetPalette());

    if (stdPalEffects_state.bEnabled)
    {
        rdSetColorEffects(&stdPalEffects_state.effect);
    }
    else
    {
        stdPalEffect neutral;
        stdPalEffects_ResetEffect(&neutral);
        rdSetColorEffects(&neutral);
    }
}

int xboxSplitScreen_RenderGameplayFrame(void)
{
    int i;
    int result;
    unsigned int frameStartMs;
    unsigned int frameEndMs;

    if (!g_xboxSplitScreenEnabled)
        return 0;

    frameStartMs = stdPlatform_GetTimeMsec();
    g_xboxSplitScreenFrameCount++;

    if (g_xboxSplitScreenFrameCount <= 12 || (g_xboxSplitScreenFrameCount % 120) == 0)
    {
        XDBGF("SplitFrame: begin frame=%u locals=%d split=%d multi=%d server=%d b3d=%d viewIdx=%d palEn=%d tint=(%.3f,%.3f,%.3f) filter=(%d,%d,%d) add=(%d,%d,%d) fade=%.3f\n",
              g_xboxSplitScreenFrameCount,
              g_xboxSplitScreenLocalCount,
              g_xboxSplitScreenEnabled,
              sithNet_isMulti,
              sithNet_isServer,
              (int)Video_modeStruct.b3DAccel,
              Video_modeStruct.viewSizeIdx,
              stdPalEffects_state.bEnabled,
              (double)stdPalEffects_state.effect.tint.x,
              (double)stdPalEffects_state.effect.tint.y,
              (double)stdPalEffects_state.effect.tint.z,
              stdPalEffects_state.effect.filter.x,
              stdPalEffects_state.effect.filter.y,
              stdPalEffects_state.effect.filter.z,
              stdPalEffects_state.effect.add.x,
              stdPalEffects_state.effect.add.y,
              stdPalEffects_state.effect.add.z,
              (double)stdPalEffects_state.effect.fade);
        XDBGF("SplitFrame: cmap frame=%u world=%p cmap0=%p cur=%p ident=%p accel=%d\n",
              g_xboxSplitScreenFrameCount,
              (void*)sithWorld_pCurrentWorld,
              sithWorld_pCurrentWorld ? (void*)sithWorld_pCurrentWorld->colormaps : 0,
              (void*)rdColormap_pCurMap,
              (void*)rdColormap_pIdentityMap,
              rdroid_curAcceleration);
        std3D_XboxDebugLogPaletteState("SplitFrame-begin");
    }

    Video_modeStruct.b3DAccel = (HKEY)1;
    stdDisplay_VBufferFill(Video_pMenuBuffer, Video_fillColor, 0);
    jkDev_DrawLog();
    jkHudInv_ClearRects();
    jkHud_ClearRects(0);

    xboxSplitScreen_ApplyColorEffects();
    rdAdvanceFrame();

    for (i = 0; i < g_xboxSplitScreenLocalCount; i++)
    {
        int vx, vy, vw, vh;

        if (!jkPlayer_playerInfos[i].playerThing || !jkPlayer_playerInfos[i].playerThing->sector)
            continue;

        xboxSplitScreen_SetContextForLocalSlot(i);
        xboxSplitScreen_GetViewport(i, &vx, &vy, &vw, &vh);
        std3D_XboxSetViewport(vx, vy, vw, vh);
        if (!sithCamera_currentCamera || !sithCamera_currentCamera->primaryFocus)
        {
            XDBGF("SplitScreenSlot: skip frame=%u slot=%d cam=%p focus=%p thing=%p sector=%p\n",
                  g_xboxSplitScreenFrameCount,
                  i,
                  (void*)sithCamera_currentCamera,
                  sithCamera_currentCamera ? (void*)sithCamera_currentCamera->primaryFocus : 0,
                  (void*)jkPlayer_playerInfos[i].playerThing,
                  jkPlayer_playerInfos[i].playerThing ? (void*)jkPlayer_playerInfos[i].playerThing->sector : 0);
            continue;
        }
        sithMain_UpdateCamera();

        if (g_xboxSplitScreenLoggedSlots < g_xboxSplitScreenLocalCount
            || g_xboxSplitScreenFrameCount <= 4
            || (g_xboxSplitScreenFrameCount % 120) == 0)
        {
            sithThing *player = jkPlayer_playerInfos[i].playerThing;
            rdClipFrustum *fr = sithCamera_currentCamera ? sithCamera_currentCamera->rdCam.pClipFrustum : NULL;
            XDBGF("SplitScreenSlot: frame=%u slot=%d thing=%p cam=%p focus=%p worldFocus=%p sector=%p curW=%d pov=%p persp=0x%X rdFov=%.2f aspect=%.4f fr=%p zn=%.6f zf=%.2f\n",
                  g_xboxSplitScreenFrameCount,
                  i,
                  (void*)player,
                  (void*)sithCamera_currentCamera,
                  sithCamera_currentCamera ? (void*)sithCamera_currentCamera->primaryFocus : 0,
                  sithWorld_pCurrentWorld ? (void*)sithWorld_pCurrentWorld->cameraFocus : 0,
                  sithCamera_currentCamera ? (void*)sithCamera_currentCamera->sector : 0,
                  player ? sithInventory_GetCurWeapon(player) : -1,
                  player && player->playerInfo ? (void*)player->playerInfo->povModel.model3 : 0,
                  sithCamera_currentCamera ? (unsigned)sithCamera_currentCamera->cameraPerspective : 0,
                  sithCamera_currentCamera ? (double)sithCamera_currentCamera->rdCam.fov : 0.0,
                  sithCamera_currentCamera ? (double)sithCamera_currentCamera->rdCam.screenAspectRatio : 0.0,
                  (void*)fr,
                  fr ? (double)fr->zNear : 0.0,
                  fr ? (double)fr->zFar : 0.0);
            if (g_xboxSplitScreenLoggedSlots < g_xboxSplitScreenLocalCount)
                g_xboxSplitScreenLoggedSlots++;
        }

        jkPlayer_DrawPov();
        std3D_XboxBeginViewportUI(vx, vy, vw, vh);
        xboxSplitScreen_DrawHudForCurrentPlayer();
        std3D_XboxEndViewportUI();
        jkHudInv_Draw();
    }

    xboxSplitScreen_RestoreContext();
    xboxSplitScreen_ResetViewport();
    jkDev_BlitLogToScreen();

    result = stdDisplay_DDrawGdiSurfaceFlip();
    frameEndMs = stdPlatform_GetTimeMsec();
    if (g_xboxSplitScreenFrameCount <= 5 || (g_xboxSplitScreenFrameCount % 120) == 0)
    {
        XDBGF("SplitScreenFrame: frame=%u locals=%d ms=%u result=%d\n",
              g_xboxSplitScreenFrameCount,
              g_xboxSplitScreenLocalCount,
              frameEndMs - frameStartMs,
              result);
    }
    return result;
}
