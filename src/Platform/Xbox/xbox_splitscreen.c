#include "xbox_splitscreen.h"

#include "xbox_debug.h"
#include "Devices/sithControl.h"
#include "Dss/sithMulti.h"
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
#include "World/sithWeapon.h"
#include "World/sithWorld.h"
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
#ifdef __cplusplus
}
#endif

static int g_xboxSplitScreenEnabled = 0;
static int g_xboxSplitScreenRequested = 0;
static int g_xboxSplitScreenLocalCount = 1;
static int g_xboxSplitScreenLoggedViewports = 0;
static int g_xboxSplitScreenLoggedSlots = 0;
static unsigned int g_xboxSplitScreenFrameCount = 0;
static unsigned int g_xboxSplitScreenRespawnAt[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {0};

static void xboxSplitScreen_SeedPlayerWeapon(sithThing *player)
{
    if (!player || player->type != SITH_THING_PLAYER || !player->actorParams.playerinfo)
        return;

    sithInventory_SetBinAmount(player, SITHBIN_FISTS, 1.0f);
    sithInventory_SetAvailable(player, SITHBIN_FISTS, 1);
    sithInventory_SetCarries(player, SITHBIN_FISTS, 1);

    sithInventory_SetBinAmount(player, SITHBIN_BRYARPISTOL, 1.0f);
    sithInventory_SetAvailable(player, SITHBIN_BRYARPISTOL, 1);
    sithInventory_SetCarries(player, SITHBIN_BRYARPISTOL, 1);
    sithInventory_SetBinAmount(player, SITHBIN_ENERGY, 50.0f);

    if (!player->playerInfo || !player->playerInfo->povModel.model3 ||
        sithInventory_GetCurWeapon(player) == SITHBIN_NONE)
    {
        sithWeapon_StartupEntry();
        sithInventory_SetCurWeapon(player, SITHBIN_FISTS);
        sithWeapon_SelectWeapon(player, SITHBIN_BRYARPISTOL, 0);
        sithWeapon_handle_inv_msgs(player);
    }
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

int xboxSplitScreen_IsEnabled(void)
{
    return g_xboxSplitScreenEnabled;
}

int xboxSplitScreen_GetLocalPlayerCount(void)
{
    return g_xboxSplitScreenLocalCount;
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
    g_xboxSplitScreenLoggedViewports = 0;
    g_xboxSplitScreenLoggedSlots = 0;
    g_xboxSplitScreenFrameCount = 0;
    for (i = 0; i < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; i++)
        g_xboxSplitScreenRespawnAt[i] = 0;

    stdControl_XboxSetActiveController(0);
    std3D_XboxResetViewport();
}

void xboxSplitScreen_OnMultiplayerServerStarted(void)
{
    int i;
    int count = jkPlayer_maxPlayers;

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

    if (count > XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        count = XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS;
    if (count < 1)
        count = 1;

    g_xboxSplitScreenLocalCount = count;
    g_xboxSplitScreenEnabled = (count > 1);

    for (i = 0; i < count; i++)
    {
        if (!jkPlayer_playerInfos[i].playerThing)
            continue;

        sithPlayer_sub_4C87C0(i, i + 1);
        jk_snwprintf(jkPlayer_playerInfos[i].player_name, 32, L"Xbox P%d", i + 1);
        jk_snwprintf(jkPlayer_playerInfos[i].multi_name, 32, L"Xbox P%d", i + 1);
        jkPlayer_playerInfos[i].teamNum = 0;
        xboxSplitScreen_SetContextForLocalSlot(i);
        xboxSplitScreen_SeedPlayerWeapon(jkPlayer_playerInfos[i].playerThing);
        XDBGF("SplitScreenInit: slot=%d thing=%p sector=%p curW=%d pov=%p flags=0x%X tf=0x%X\n",
              i,
              (void*)jkPlayer_playerInfos[i].playerThing,
              jkPlayer_playerInfos[i].playerThing ? (void*)jkPlayer_playerInfos[i].playerThing->sector : 0,
              jkPlayer_playerInfos[i].playerThing ? sithInventory_GetCurWeapon(jkPlayer_playerInfos[i].playerThing) : -1,
              jkPlayer_playerInfos[i].playerThing && jkPlayer_playerInfos[i].playerThing->playerInfo
                  ? (void*)jkPlayer_playerInfos[i].playerThing->playerInfo->povModel.model3 : 0,
              jkPlayer_playerInfos[i].flags,
              jkPlayer_playerInfos[i].playerThing ? jkPlayer_playerInfos[i].playerThing->thingflags : 0);
    }

    sithPlayer_idk(0);
    sithCamera_SetsFocus();
    sithPlayer_ResetPalEffects();
    xboxSplitScreen_ClearLocalInvulnerability();

    XDBGF("SplitScreen: local players=%d enabled=%d maxPlayers=%d\n",
          g_xboxSplitScreenLocalCount, g_xboxSplitScreenEnabled, jkPlayer_maxPlayers);
}

void xboxSplitScreen_SetContextForLocalSlot(int slot)
{
    if (slot < 0)
        slot = 0;
    if (slot >= g_xboxSplitScreenLocalCount)
        slot = g_xboxSplitScreenLocalCount - 1;
    if (slot < 0)
        slot = 0;

    if (jkPlayer_playerInfos[slot].playerThing)
    {
        sithPlayer_idk(slot);
        sithCamera_SetsFocus();
        xboxSplitScreen_ClearLocalInvulnerability();
    }
}

void xboxSplitScreen_RestoreContext(void)
{
    xboxSplitScreen_SetContextForLocalSlot(0);
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

        if ((connectedMask & (1 << i)) == 0)
        {
            static int s_loggedSkippedPads = 0;
            if (s_loggedSkippedPads < 8)
            {
                XDBGF("SplitScreenCtl: skip slot=%d no controller mask=0x%X\n", i, connectedMask);
                s_loggedSkippedPads++;
            }
            continue;
        }

        stdControl_XboxSetActiveController(i);
        xboxSplitScreen_SetContextForLocalSlot(i);
        sithControl_Tick(deltaSecs, deltaMs);
    }

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
        if (!jkPlayer_playerInfos[i].playerThing)
            continue;

        xboxSplitScreen_SetContextForLocalSlot(i);
        xboxSplitScreen_ApplyViewport(i);
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
        xboxSplitScreen_DrawHudForCurrentPlayer();
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
