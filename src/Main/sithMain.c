#include "sithMain.h"

#ifdef TARGET_XBOX
#include "xbox_debug.h"
#include "Platform/Xbox/xbox_splitscreen.h"
#endif

#define XSL_TRACE_UPDATE(label) do { } while (0)
#include "Main/jkGame.h"
#include "Main/Main.h"
#include "World/sithWorld.h"
#include "World/jkPlayer.h"
#include "Engine/sithCollision.h"
#include "World/sithActor.h"
#include "General/sithStrTable.h"
#include "General/stdString.h"
#include "General/stdFnames.h"
#include "Win95/stdComm.h"
#include "Devices/sithConsole.h"
#include "Win95/Window.h"
#include "AI/sithAI.h"
#include "AI/sithAIClass.h"
#include "AI/sithAIAwareness.h"
#include "Gameplay/sithEvent.h"
#include "Gameplay/sithInventory.h"
#include "Engine/sithRender.h"
#include "Engine/sithCamera.h"
#include "World/sithSprite.h"
#include "Engine/sithParticle.h"
#include "Engine/sithPuppet.h"
#include "World/sithSoundClass.h"
#include "World/sithMaterial.h"
#include "World/sithTemplate.h"
#include "World/sithModel.h"
#include "World/sithSurface.h"
#include "Devices/sithSound.h"
#include "Devices/sithSoundMixer.h"
#include "Gameplay/sithTime.h"
#include "Engine/sithRender.h"
#include "Devices/sithControl.h"
#include "Dss/sithMulti.h"
#include "Dss/sithGamesave.h"
#include "World/sithWeapon.h"
#include "World/sithSector.h"
#include "World/jkPlayer.h"
#include "Cog/sithCog.h"
#include "Devices/sithComm.h"
#include "stdPlatform.h"
#include "jk.h"

#ifdef FIXED_TIMESTEP_PHYS
#include <math.h>
#endif

// Added: FoV fixes
flex_t sithMain_lastAspect = 1.0;

#ifdef TARGET_XBOX
static void sithMain_XboxLogMotsInventoryBin(sithThing *player, int binIdx)
{
    sithPlayerInfo *playerInfo;
    sithItemDescriptor *desc;

    if (!Main_bMotsCompat || !player || binIdx < 0 || binIdx >= SITHBIN_NUMBINS)
        return;

    playerInfo = player->actorParams.playerinfo;
    desc = &sithInventory_aDescriptors[binIdx];
    if (!playerInfo || playerInfo == (sithPlayerInfo*)-136)
    {
        xbox_debug_Printf("MotSMode: AutoSave bin=%d playerInfo=%p desc=%.7s flags=0x%X\n",
                          binIdx,
                          (void*)playerInfo,
                          desc->fpath,
                          desc->flags);
        return;
    }

    xbox_debug_Printf("MotSMode: AutoSave bin=%d name=%.7s flags=0x%X amt=%.1f state=0x%X avail=%d carries=%d cog=%p\n",
                      binIdx,
                      desc->fpath,
                      desc->flags,
                      (double)sithInventory_GetBinAmount(player, binIdx),
                      playerInfo->iteminfo[binIdx].state,
                      sithInventory_GetAvailable(player, binIdx),
                      sithInventory_GetCarries(player, binIdx),
                      (void*)desc->cog);
}

static void sithMain_XboxLogMotsAutoSaveState(const char *phase)
{
    sithThing *player;
    sithPlayerInfo *playerInfo;

    if (!Main_bMotsCompat)
        return;

    player = sithPlayer_pLocalPlayerThing;
    playerInfo = player ? player->actorParams.playerinfo : NULL;
    xbox_debug_Printf("MotSMode: AutoSave %s player=%p playerIdx=%d type=%u classCog=%p captureCog=%p playerInfo=%p curW=%d curItem=%d curPower=%d staticCogs=%u worldCogs=%u things=%u\n",
                      phase,
                      (void*)player,
                      player ? (int)player->thingIdx : -1,
                      player ? (unsigned)player->type : 0,
                      player ? (void*)player->class_cog : 0,
                      player ? (void*)player->capture_cog : 0,
                      (void*)playerInfo,
                      playerInfo ? playerInfo->curWeapon : -1,
                      playerInfo ? playerInfo->curItem : -1,
                      playerInfo ? playerInfo->curPower : -1,
                      sithWorld_pStatic ? (unsigned)sithWorld_pStatic->numCogsLoaded : 0,
                      sithWorld_pCurrentWorld ? (unsigned)sithWorld_pCurrentWorld->numCogsLoaded : 0,
                      sithWorld_pCurrentWorld ? (unsigned)sithWorld_pCurrentWorld->numThingsLoaded : 0);

    sithMain_XboxLogMotsInventoryBin(player, SITHBIN_MOTS_FISTS);
    sithMain_XboxLogMotsInventoryBin(player, SITHBIN_MOTS_BRYARPISTOL);
    sithMain_XboxLogMotsInventoryBin(player, SITHBIN_MOTS_STORMTROOPER_RIFLE);
    sithMain_XboxLogMotsInventoryBin(player, SITHBIN_MOTS_EWEB);
    sithMain_XboxLogMotsInventoryBin(player, SITHBIN_MOTS_LIGHTSABER);
    sithMain_XboxLogMotsInventoryBin(player, SITHBIN_MOTS_STORMTROOPER_SCOPE);
}
#endif

static int sithMain_CogHasTrigger(sithCog *cog, int msgid)
{
    sithCogScript *script;
    uint32_t i;

    if (!cog || !cog->cogscript)
        return 0;

    script = cog->cogscript;
    for (i = 0; i < script->num_triggers; i++)
    {
        if (script->triggers[i].trigId == msgid)
            return 1;
    }

    return 0;
}

static int sithMain_MotsPlayerStartupInventoryMissing(sithThing *player)
{
    if (!player || !player->actorParams.playerinfo)
        return 0;

    return sithInventory_GetBinAmount(player, SITHBIN_MOTS_FISTS) <= 0.0
        && sithInventory_GetBinAmount(player, SITHBIN_MOTS_LIGHTSABER) <= 0.0
        && sithInventory_GetBinAmount(player, SITHBIN_MOTS_BRYARPISTOL) <= 0.0
        && sithInventory_GetBinAmount(player, SITHBIN_MOTS_BLASTECH) <= 0.0;
}

static void sithMain_SendMotsLocalPlayerClassStartupIfNeeded(void)
{
    sithThing *player;
    sithCog *classCog;

    if (!Main_bMotsCompat || sithNet_isMulti)
        return;

    player = sithPlayer_pLocalPlayerThing;
    if (!player || player->type != SITH_THING_PLAYER || !sithMain_MotsPlayerStartupInventoryMissing(player))
        return;

    classCog = player->class_cog;
    if (!sithMain_CogHasTrigger(classCog, SITH_MESSAGE_STARTUP))
    {
#ifdef TARGET_XBOX
        xbox_debug_Printf("MotSMode: local player class startup skipped player=%p classCog=%p hasStartup=0\n",
                          (void*)player, (void*)classCog);
#endif
        return;
    }

#ifdef TARGET_XBOX
    xbox_debug_Printf("MotSMode: firing local player class STARTUP player=%p thingIdx=%d classCog=%p name=%s script=%s\n",
                      (void*)player,
                      (int)player->thingIdx,
                      (void*)classCog,
                      classCog->cogscript_fpath,
                      classCog->cogscript ? classCog->cogscript->cog_fpath : "");
#endif
    sithCog_SendMessage(classCog, SITH_MESSAGE_STARTUP, SENDERTYPE_THING, player->thingIdx, 0, 0, 0);
}

int sithMain_Startup(HostServices *commonFuncs)
{
    int is_started; // esi

    pSithHS = commonFuncs;
    is_started = sithStrTable_Startup() & 1;
    is_started = sithEvent_Startup() & is_started;
    is_started = sithWorld_Startup() & is_started;
    is_started = sithRender_Startup() & is_started;
    is_started = sithCollision_Startup() & is_started;
    is_started = sithThing_Startup() & is_started;
    is_started = sithComm_Startup() & is_started;
    is_started = stdComm_Startup() & is_started;
    is_started = sithCog_Startup() & is_started;
    is_started = sithAI_Startup() & is_started;
    is_started = sithSprite_Startup() & is_started;
    is_started = sithParticle_Startup() & is_started;
    is_started = sithPuppet_Startup() & is_started;
    is_started = sithAIClass_Startup() & is_started;
    is_started = sithSoundClass_Startup() & is_started;
    is_started = sithMaterial_Startup() & is_started;
    is_started = sithTemplate_Startup() & is_started;
    is_started = sithModel_Startup() & is_started;
    is_started = sithSurface_Startup() & is_started;
    sithSound_Startup();
    sithSoundMixer_Startup();
    sithWeapon_Startup();

#ifndef NO_JK_MMAP
    //_memset(&g_sithMode, 0, 0x18u);
#endif
    g_sithMode = 0;
    g_submodeFlags = 0;
    sithSurface_byte_8EE668 = 0;
    g_debugmodeFlags = 0;
    jkPlayer_setDiff = 0;
    g_mapModeFlags = 0;

    // Added
    if (Main_bHeadless || Main_bDedicatedServer) {
        g_debugmodeFlags |= DEBUGFLAG_IN_EDITOR;
    }

    if ( !is_started )
        return 0;

    sithMain_bInitialized = 1;
    return 1;
}

void sithMain_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);
    //sithWeapon
    sithSoundMixer_Shutdown();
    sithSound_Shutdown();
    sithSurface_Shutdown();
    sithModel_Shutdown();
    sithTemplate_Shutdown();
    sithMaterial_Shutdown();
    sithSoundClass_Shutdown();
    sithAIClass_Shutdown();
    sithPuppet_Shutdown();
    sithParticle_Shutdown();
    sithSprite_Shutdown();
    sithAI_Shutdown();
    sithCog_Shutdown();
    stdComm_Shutdown();
    sithComm_Shutdown();
    sithThing_Shutdown();
    sithCollision_Shutdown();
    sithRender_Shutdown();
    sithWorld_Shutdown();
    sithEvent_Shutdown();
    sithStrTable_Shutdown();
    sithMain_bInitialized = 0;
}

int sithMain_Load(char *path)
{
    sithWorld *newStatic;

    if ( sithWorld_pStatic )
        sithMain_Free();

    newStatic = sithWorld_New();
    if ( !newStatic )
        return 0;

    newStatic->level_type_maybe |= 1;
    sithWorld_pStatic = newStatic;
    if ( !sithWorld_Load(newStatic, path) )
    {
        if ( sithWorld_pLoading == newStatic )
            sithWorld_pLoading = 0;
        sithWorld_FreeEntry(newStatic);
        sithWorld_pStatic = 0;
        return 0;
    }

    return 1;
}

void sithMain_Free()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);
    if ( sithWorld_pStatic )
    {
        if ( sithWorld_pLoading == sithWorld_pStatic )
            sithWorld_pLoading = 0;
        sithWorld_FreeEntry(sithWorld_pStatic);
        sithWorld_pStatic = 0;
    }
}

int sithMain_Mode1Init(char *a1)
{
#ifdef TARGET_XBOX
    XDBGF("Mode1Init: a1='%s'\n", a1 ? a1 : "(null)");
#endif
    sithWorld_pCurrentWorld = sithWorld_New();
#ifdef TARGET_XBOX
    XDBGF("Mode1Init: sithWorld_New=%p\n", sithWorld_pCurrentWorld);
#endif
    if ( !sithWorld_pCurrentWorld )
        return 0;

    if ( !sithWorld_Load(sithWorld_pCurrentWorld, a1) )
    {
#ifdef TARGET_XBOX
        XDBG("Mode1Init: sithWorld_Load FAILED\n");
#endif
        sithWorld_pCurrentWorld = 0;
        return 0;
    }

    sithTime_Startup();
    sithWorld_Initialize();
    sithMain_Open();
    sithTime_Startup();
    g_sithMode = 1;
    return 1;
}

int sithMain_OpenNormal(char *path)
{
    sithWorld_pCurrentWorld = sithWorld_New();
    if ( !sithWorld_pCurrentWorld )
        return 0;

    if ( !sithWorld_Load(sithWorld_pCurrentWorld, path) )
    {
        sithWorld_pCurrentWorld = 0;
        return 0;
    }

    sithWorld_Initialize();
    sithMain_Open();
    g_sithMode = 1;
    return 1;
}

int sithMain_Mode1Init_3(char *fpath)
{
#ifdef TARGET_XBOX
    XDBGF("MPLoadTrace: sithMain_Mode1Init_3 enter fpath='%s'\n", fpath ? fpath : "(null)");
#endif
    sithWorld_pCurrentWorld = sithWorld_New();
#ifdef TARGET_XBOX
    XDBGF("MPLoadTrace: sithMain_Mode1Init_3 sithWorld_New=%p\n", sithWorld_pCurrentWorld);
    XDBG("MPLoadTrace: sithMain_Mode1Init_3 before sithWorld_Load\n");
#endif
    if ( !sithWorld_pCurrentWorld )
        return 0;

    if ( !sithWorld_Load(sithWorld_pCurrentWorld, fpath) )
    {
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: sithMain_Mode1Init_3 sithWorld_Load FAILED\n");
#endif
        sithWorld_pCurrentWorld = 0;
        return 0;
    }
#ifdef TARGET_XBOX
    XDBGF("MPLoadTrace: sithMain_Mode1Init_3 after sithWorld_Load world=%p things=%d sectors=%d cogs=%d keyframes=%d\n",
          sithWorld_pCurrentWorld,
          sithWorld_pCurrentWorld ? sithWorld_pCurrentWorld->numThingsLoaded : -1,
          sithWorld_pCurrentWorld ? sithWorld_pCurrentWorld->numSectors : -1,
          sithWorld_pCurrentWorld ? sithWorld_pCurrentWorld->numCogsLoaded : -1,
          sithWorld_pCurrentWorld ? sithWorld_pCurrentWorld->numKeyframesLoaded : -1);
    XDBG("MPLoadTrace: sithMain_Mode1Init_3 before sithWorld_Initialize\n");
#endif
    sithWorld_Initialize();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Mode1Init_3 after sithWorld_Initialize; before sithMain_Open\n");
#endif
    sithMain_Open();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Mode1Init_3 after sithMain_Open\n");
#endif
    sithTime_Startup();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Mode1Init_3 after sithTime_Startup; before sithMulti_Startup\n");
#endif
    sithMulti_Startup();
#ifdef TARGET_XBOX
    XDBGF("MPLoadTrace: sithMain_Mode1Init_3 after sithMulti_Startup multi=%d server=%d\n", sithNet_isMulti, sithNet_isServer);
#endif
    g_sithMode = 1;
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Mode1Init_3 done\n");
#endif
    return 1;
}

int sithMain_Open()
{
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open begin\n");
#endif
    jkPlayer_currentTickIdx = 0;
    sithRender_lastRenderTick = 1;
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithWorld_sub_4D0A20\n");
#endif
    sithWorld_sub_4D0A20(sithWorld_pCurrentWorld);
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithEvent_Open\n");
#endif
    sithEvent_Open();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithSurface_Open\n");
#endif
    sithSurface_Open();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithAI_Open\n");
#endif
    sithAI_Open();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithSoundMixer_Open\n");
#endif
    sithSoundMixer_Open();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithCog_Open\n");
#endif
    sithCog_Open();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithControl_Open\n");
#endif
    sithControl_Open();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithAIAwareness_Startup\n");
#endif
    sithAIAwareness_Startup();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithRender_Open\n");
#endif
    sithRender_Open();
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open before sithWeapon_StartupEntry\n");
#endif
    sithWeapon_StartupEntry();
    sithMain_bOpened = 1;
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: sithMain_Open done\n");
#endif
    return 1;
}

void sithMain_Close()
{
    if ( sithMain_bOpened )
    {
        sithSoundMixer_StopSong();
        sithRender_Close();
        sithAIAwareness_Shutdown();
        sithControl_Close();
        sithCog_Close();
        sithSoundMixer_Close();
        sithWorld_Free();
        sithAI_Close();
        sithSurface_Startup2();
        sithEvent_Close();
        sithPlayer_Close();
        sithWeapon_ShutdownEntry();
        g_sithMode = 0;
        g_submodeFlags = 0;
        sithMain_bOpened = 0;
    }
}

void sithMain_SetEndLevel()
{
    sithMain_bEndLevel = 1;
}

int sithMain_tickStartMs;
int sithMain_tickEndMs;

// MOTS altered
int sithMain_Tick()
{
#if 0
    if (sithWorld_pCurrentWorld) {
        for (int i = 0; i < sithWorld_pCurrentWorld->numKeyframesLoaded; i++) {
            rdKeyframe* keyframe = &sithWorld_pCurrentWorld->keyframes[i];
            if (keyframe->id != i) {
                stdPlatform_Printf("BAD KEYFRAME!! %d -> %d\n", i, keyframe->id);
            }
        }
        
    }
#endif

    sithMain_tickStartMs = stdPlatform_GetTimeMsec(); // Added: perf analyzing

    if ( (g_submodeFlags & 8) != 0 )
    {
        sithTime_Tick();
        sithComm_Sync();

#ifdef TARGET_TWL
        // Fallback to stepped 30Hz physics if ms delta is very high
        if (sithTime_deltaMs > 100) {
            jkPlayer_bJankyPhysics = 0;
        }
        else {
            jkPlayer_bJankyPhysics = 1;
        }
#endif

#ifdef FIXED_TIMESTEP_PHYS
        if (NEEDS_STEPPED_PHYS) {
            // Run all physics at a fixed timestep
            flex_d_t rolloverCombine = sithTime_deltaSeconds + sithTime_physicsRolloverFrames;

            flex_d_t framesToApply = rolloverCombine * TARGET_PHYSTICK_FPS; // get number of 50FPS steps passed
            uint32_t wholeFramesToApply = (uint32_t)(float)round((float)framesToApply);
            sithTime_physicsRolloverFrames = rolloverCombine - (((flex_d_t)wholeFramesToApply) * DELTA_PHYSTICK_FPS);

            //printf("%f %f\n", framesToApply, rolloverCombine);

            flex_t tmp = sithTime_deltaSeconds;
            uint32_t tmp2 = sithTime_deltaMs;
            sithTime_deltaSeconds = DELTA_PHYSTICK_FPS;
            sithTime_deltaMs = (int)(DELTA_PHYSTICK_FPS * 1000.0);

            for (int i = (int)framesToApply; i > 0; i--)
            {
                sithSurface_Tick(sithTime_deltaSeconds);
                sithThing_TickAll(sithTime_deltaSeconds, sithTime_deltaMs);
            }

            sithTime_deltaSeconds = tmp;
            sithTime_deltaMs = tmp2;
        }
        else
#endif
        {
            sithSurface_Tick(sithTime_deltaSeconds);
            sithThing_TickAll(sithTime_deltaSeconds, sithTime_deltaMs);
        }
        sithConsole_AdvanceLogBuf();
        return 1;
    }
    else
    {
        // TODO REMOVE
        //sithWorld_pCurrentWorld->playerThing->physicsParams.physflags |= SITH_PF_FLY;
        //sithWorld_pCurrentWorld->playerThing->physicsParams.physflags &= ~SITH_PF_USEGRAVITY;

#ifdef TARGET_XBOX
#endif
        ++jkPlayer_currentTickIdx;
        sithMain_sub_4C4D80();
#ifdef TARGET_XBOX
#endif
        sithSoundMixer_ResumeMusic(0);
#ifdef TARGET_XBOX
#endif
        sithTime_Tick();

#ifdef FIXED_TIMESTEP_PHYS
        if (NEEDS_STEPPED_PHYS) {
            // Run all physics at a fixed timestep
            flex_d_t rolloverCombine = sithTime_deltaSeconds + sithTime_physicsRolloverFrames;

            flex_d_t framesToApply = rolloverCombine * TARGET_PHYSTICK_FPS; // get number of 50FPS steps passed
            uint32_t wholeFramesToApply = (uint32_t)(float)round((float)framesToApply);
            sithTime_physicsRolloverFrames = rolloverCombine - (((flex_d_t)wholeFramesToApply) * DELTA_PHYSTICK_FPS);

#ifdef TARGET_XBOX
#endif
            if (wholeFramesToApply > 0)
            {
                /* Only read controls when physics frames will actually run.
                 * wholeFramesToApply==0 (sithTime stubbed) → skip entirely. */
#ifdef TARGET_XBOX
#endif
                #ifdef TARGET_XBOX
                if (xboxSplitScreen_IsEnabled())
                    xboxSplitScreen_BeginControlFrame();
                else
                #endif
                sithControl_ReadControls();
                if ( g_sithMode != 2 )
                {
#ifdef TARGET_XBOX
#endif
                    #ifdef TARGET_XBOX
                    if (xboxSplitScreen_IsEnabled())
                        xboxSplitScreen_TickControls(sithTime_deltaSeconds, sithTime_deltaMs);
                    else
                    #endif
                    sithControl_Tick(sithTime_deltaSeconds, sithTime_deltaMs);
                }
#ifdef TARGET_XBOX
#endif
                #ifdef TARGET_XBOX
                if (xboxSplitScreen_IsEnabled())
                    xboxSplitScreen_EndControlFrame();
                else
                #endif
                sithControl_FinishRead();
            }

            flex_t tmp = sithTime_deltaSeconds;
            uint32_t tmp2 = sithTime_deltaMs;
            flex_t tmp3 = sithTime_TickHz;
            flex_t tmp4 = stdControl_updateKHz;
            flex_t tmp5 = stdControl_updateHz;
            uint32_t tmp6 = sithTime_curMs;
            sithTime_curMs -= sithTime_deltaMs;
            sithTime_deltaSeconds = DELTA_PHYSTICK_FPS;
            sithTime_deltaMs = (int)(DELTA_PHYSTICK_FPS * 1000.0);
            sithTime_TickHz = 1.0 / sithTime_deltaSeconds;

            for (int i = 0; i < wholeFramesToApply; i++)
            {
#ifdef TARGET_XBOX
#endif
                XSL_TRACE_UPDATE("fixed before sound");
                sithSoundMixer_Tick(sithTime_deltaSeconds);
#ifdef TARGET_XBOX
#endif
                XSL_TRACE_UPDATE("fixed after sound before event");
                sithEvent_Advance();
                XSL_TRACE_UPDATE("fixed after event before comm");

                if ( sithComm_bSyncMultiplayer )
                {
                    XSL_TRACE_UPDATE("fixed before comm");
                    sithComm_Sync();
                    XSL_TRACE_UPDATE("fixed after comm");
                }

                if ( (g_debugmodeFlags & DEBUGFLAG_NO_AIEVENTS) == 0  && (!sithNet_isMulti || sithNet_isMulti && sithNet_isServer))
                {
#ifdef TARGET_XBOX
#endif
                    XSL_TRACE_UPDATE("fixed before ai");
                    sithAI_TickAll();
                    XSL_TRACE_UPDATE("fixed after ai");
                }

#ifdef TARGET_XBOX
#endif
                XSL_TRACE_UPDATE("fixed before surface");
                sithSurface_Tick(sithTime_deltaSeconds);
#ifdef TARGET_XBOX
#endif
                XSL_TRACE_UPDATE("fixed after surface before things");
                sithThing_TickAll(sithTime_deltaSeconds, sithTime_deltaMs);
#ifdef TARGET_XBOX
#endif
                XSL_TRACE_UPDATE("fixed after things before mots");
                sithThing_MotsTick(0x1F, 0, 0);
#ifdef TARGET_XBOX
#endif
                XSL_TRACE_UPDATE("fixed after mots before cog");
                sithCogScript_TickAll();
#ifdef TARGET_XBOX
#endif
                XSL_TRACE_UPDATE("fixed after cog");

                // COG scripts will sleep for periods of time based on sithTime_curMs,
                // so we have to emulate the current time as well
                sithTime_curMs += sithTime_deltaMs;
            }

            sithTime_deltaSeconds = tmp;
            sithTime_deltaMs = tmp2;
            sithTime_TickHz = tmp3;
            sithTime_curMs = tmp6;
            //stdControl_updateKHz = tmp4;
            //stdControl_updateHz = tmp5;
        }
        else
#endif
        {
#ifdef TARGET_XBOX
#endif
            XSL_TRACE_UPDATE("normal before sound");
            sithSoundMixer_Tick(sithTime_deltaSeconds);
            XSL_TRACE_UPDATE("normal after sound before event");
            sithEvent_Advance();
            XSL_TRACE_UPDATE("normal after event before comm");

            if ( sithComm_bSyncMultiplayer )
            {
                XSL_TRACE_UPDATE("normal before comm");
                sithComm_Sync();
                XSL_TRACE_UPDATE("normal after comm");
            }

            if ( (g_debugmodeFlags & DEBUGFLAG_NO_AIEVENTS) == 0 && (!sithNet_isMulti || sithNet_isMulti && sithNet_isServer))
            {
                XSL_TRACE_UPDATE("normal before ai");
                sithAI_TickAll();
                XSL_TRACE_UPDATE("normal after ai");
            }

            XSL_TRACE_UPDATE("normal before surface");
            sithSurface_Tick(sithTime_deltaSeconds);
            XSL_TRACE_UPDATE("normal after surface before controls");
            if ( g_sithMode != 2 )
            {
#ifdef TARGET_XBOX
                if (xboxSplitScreen_IsEnabled())
                {
                    xboxSplitScreen_BeginControlFrame();
                }
                else
#endif
                {
#ifdef FIXED_TIMESTEP_PHYS
                    sithControl_ReadControls();
#endif
                }
                #ifdef TARGET_XBOX
                if (xboxSplitScreen_IsEnabled())
                    xboxSplitScreen_TickControls(sithTime_deltaSeconds, sithTime_deltaMs);
                else
                #endif
                sithControl_Tick(sithTime_deltaSeconds, sithTime_deltaMs);
#ifdef TARGET_XBOX
                if (xboxSplitScreen_IsEnabled())
                {
                    xboxSplitScreen_EndControlFrame();
                }
                else
#endif
                {
#ifdef FIXED_TIMESTEP_PHYS
                    sithControl_FinishRead();
#endif
                }
            }

            XSL_TRACE_UPDATE("normal after controls before things");
            sithThing_TickAll(sithTime_deltaSeconds, sithTime_deltaMs);
            XSL_TRACE_UPDATE("normal after things before mots");
            sithThing_MotsTick(0x1F, 0, 0);

            XSL_TRACE_UPDATE("normal after mots before cog");
            sithCogScript_TickAll();
            XSL_TRACE_UPDATE("normal after cog");
        }

        //sithAI_PrintThings();
#ifdef TARGET_XBOX
#endif
        XSL_TRACE_UPDATE("before console advance");
        sithConsole_AdvanceLogBuf();
#ifdef TARGET_XBOX
#endif
        XSL_TRACE_UPDATE("after console before timelimit");
        sithMulti_HandleTimeLimit(sithTime_deltaMs);
#ifdef TARGET_XBOX
#endif
        XSL_TRACE_UPDATE("after timelimit before save flush");
        sithGamesave_Flush();
#ifdef TARGET_XBOX
#endif
        XSL_TRACE_UPDATE("after save flush");

        sithMain_tickEndMs = stdPlatform_GetTimeMsec();

        return 0;
    }
}

void sithMain_UpdateCamera()
{
#if defined(TARGET_TWL)
    jkPlayer_fov = 98; // 90deg vertical, 106deg horizontal stock
    jkPlayer_bJankyPhysics = 1;
    jkPlayer_fovIsVertical = 0;
    jkPlayer_enableOrigAspect = 0;
#endif

    if ( (g_submodeFlags & 8) == 0 )
    {
        sithMain_sub_4C4D80();

#if defined(QOL_IMPROVEMENTS)
        if (sithCamera_currentCamera && sithCamera_currentCamera->rdCam.canvas)
        {
            // Set screen aspect ratio
            flex_t aspect = sithCamera_currentCamera->rdCam.canvas->half_screen_height / sithCamera_currentCamera->rdCam.canvas->half_screen_width;
#if defined(TARGET_XBOX)
            {
                flex_t splitAspect = xboxSplitScreen_GetCurrentViewportAspect();
                if (splitAspect > 0.0)
                    aspect = splitAspect;
            }
#endif
#if defined(TARGET_TWL)
            //aspect = 192.0/256.0;
            //const flex_t canvasWidth = 256.0;
            //const flex_t canvasHeight = 192.0;
            //aspect = 1.0;
            aspect = 192.0/256.0;
            const flex_t canvasWidth = 256.0;
            const flex_t canvasHeight = 192.0;
            sithCamera_currentCamera->rdCam.canvas->half_screen_width = canvasWidth/2;
            sithCamera_currentCamera->rdCam.canvas->half_screen_height = canvasHeight/2;
            sithCamera_currentCamera->rdCam.canvas->widthMinusOne = canvasWidth - 1.0;
            sithCamera_currentCamera->rdCam.canvas->heightMinusOne = canvasHeight - 1.0;
            static flex_t sithMain_UpdateCamera_lastFov = 90.0;
            static void* sithMain_UpdateCamera_lastCamera = NULL;

            //if (aspect != sithMain_lastAspect || jkPlayer_fov != sithCamera_currentCamera->rdCam.fov || jkPlayer_fov != sithMain_UpdateCamera_lastFov || sithMain_UpdateCamera_lastCamera != sithCamera_currentCamera) {
#endif
                if (!Main_bMotsCompat)
                {
                    rdCamera_SetAspectRatio(&sithCamera_currentCamera->rdCam, aspect);
                    rdCamera_SetFOV(&sithCamera_currentCamera->rdCam, jkPlayer_fov);
                    rdCamera_SetOrthoScale(&sithCamera_currentCamera->rdCam, 250.0);
                }
                else {
                    rdCamera_SetAspectRatio(&sithCamera_currentCamera->rdCam, aspect);

                    // We still need this override for cameras that don't have zoom (third-person)
                    if (sithCamera_currentCamera->cameraPerspective != 1) {
                        rdCamera_SetFOV(&sithCamera_currentCamera->rdCam, jkPlayer_fov);
                    }
                    rdCamera_SetOrthoScale(&sithCamera_currentCamera->rdCam, 250.0);
                }
#if defined(TARGET_TWL)
            //}
#endif

            sithMain_lastAspect = aspect;
#if defined(TARGET_TWL)
            sithMain_UpdateCamera_lastFov = jkPlayer_fov;
            sithMain_UpdateCamera_lastCamera = sithCamera_currentCamera;
#endif
        }
#endif

        //sithCamera_currentCamera->rdCam.screenAspectRatio += 0.01;
#ifdef TARGET_XBOX
        { static int _smuc=0; if(_smuc<1){ XDBGF("sithMain_UpdateCamera: cam=%p sector=%p\n", (void*)sithCamera_currentCamera, (void*)(sithCamera_currentCamera ? sithCamera_currentCamera->sector : 0)); _smuc++; } }
#endif
        sithCamera_FollowFocus(sithCamera_currentCamera);
        sithCamera_SetRdCameraAndRenderidk();
    }
}

void sithMain_sub_4C4D80()
{
    if ( !++sithRender_lastRenderTick )
    {
        sithWorld_sub_4D0A20(sithWorld_pCurrentWorld);
        sithRender_lastRenderTick = 1;
    }
}

void sithMain_set_sithmode_5()
{
    g_sithMode = 5;
}

void sithMain_SetEpisodeName(char *text)
{
    _strncpy(sithWorld_episodeName, text, 0x1Fu);
    sithWorld_episodeName[31] = 0;
}

// MOTS altered
void sithMain_AutoSave()
{
    sithThing *v3; // esi
    sithCog *v4; // eax
    char v5[128]; // [esp+10h] [ebp-80h] BYREF


#ifdef LINUX_TMP
    //g_debugmodeFlags |= 1;
#endif

#ifdef TARGET_XBOX
    sithMain_XboxLogMotsAutoSaveState("enter");
#endif
    sithTime_Startup();
#ifdef TARGET_XBOX
    sithMain_XboxLogMotsAutoSaveState("before-reset");
#endif
    sithInventory_Reset(sithPlayer_pLocalPlayerThing);
#ifdef TARGET_XBOX
    sithMain_XboxLogMotsAutoSaveState("after-reset");
#endif

#ifdef TARGET_XBOX
    sithMain_XboxLogMotsAutoSaveState("before-startup");
#endif
    sithCog_SendSimpleMessageToAll(SITH_MESSAGE_STARTUP, 0, 0, 0, 0);
    sithMain_SendMotsLocalPlayerClassStartupIfNeeded();
#ifdef TARGET_XBOX
    sithMain_XboxLogMotsAutoSaveState("after-startup");
#endif
    for (uint32_t v2 = 0; v2 < sithWorld_pCurrentWorld->numThingsLoaded; v2++)
    {
        v3 = &sithWorld_pCurrentWorld->things[v2];
        v4 = v3->class_cog;
        if (Main_bMotsCompat && !v3->type) continue; // MOTS added

        if ( v4 )
        {
            sithCog_SendMessage(v4, SITH_MESSAGE_CREATED, SENDERTYPE_THING, v3->thingIdx, 0, 0, 0);
        }
        if ( v3->type == SITH_THING_ACTOR )
        {
            sithActor_SetMaxHeathForDifficulty(v3);
        }
    }
#ifdef TARGET_XBOX
    sithMain_XboxLogMotsAutoSaveState("after-created");
#endif

    if ( sithNet_isMulti )
    {
        sithPlayer_debug_ToNextCheckpoint(sithPlayer_pLocalPlayerThing);
        sithMulti_SendWelcome(stdComm_dplayIdSelf, playerThingIdx, -1);
        sithMulti_SendWelcome(stdComm_dplayIdSelf, playerThingIdx, -1);
        sithTime_Startup();
    }
    else
    {
        stdString_snprintf(v5, 128, "%s%s", "_JKAUTO_", sithWorld_pCurrentWorld->map_jkl_fname);
        stdFnames_ChangeExt(v5, "jks");
        sithGamesave_Write(v5, 1, 0, 0);
        sithTime_Startup();
    }
}
