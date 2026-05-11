#include "sithMain.h"

#ifdef TARGET_XBOX
#include "xbox_debug.h"
#endif
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
    sithWorld_pStatic = sithWorld_New();
    sithWorld_pStatic->level_type_maybe |= 1;
    return sithWorld_Load(sithWorld_pStatic, path) != 0;
}

void sithMain_Free()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__);
    if ( sithWorld_pStatic )
    {
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

    if ( !sithWorld_Load(sithWorld_pCurrentWorld, a1) )
    {
#ifdef TARGET_XBOX
        XDBG("Mode1Init: sithWorld_Load FAILED\n");
#endif
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

    if ( !sithWorld_Load(sithWorld_pCurrentWorld, path) )
        return 0;

    sithWorld_Initialize();
    sithMain_Open();
    g_sithMode = 1;
    return 1;
}

int sithMain_Mode1Init_3(char *fpath)
{
    sithWorld_pCurrentWorld = sithWorld_New();
    if ( !sithWorld_Load(sithWorld_pCurrentWorld, fpath) )
        return 0;
    sithMain_Open();
    sithTime_Startup();
    sithMulti_Startup();
    g_sithMode = 1;
    return 1;
}

int sithMain_Open()
{
    jkPlayer_currentTickIdx = 0;
    sithRender_lastRenderTick = 1;
    sithWorld_sub_4D0A20(sithWorld_pCurrentWorld);
    sithEvent_Open();
    sithSurface_Open();
    sithAI_Open();
    sithSoundMixer_Open();
    sithCog_Open();
    sithControl_Open();
    sithAIAwareness_Startup();
    sithRender_Open();
    sithWeapon_StartupEntry();
    sithMain_bOpened = 1;
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
        { static int _tst=0; if(_tst<1){ XDBG("sithTick: enter else\n"); _tst++; } }
#endif
        ++jkPlayer_currentTickIdx;
        sithMain_sub_4C4D80();
#ifdef TARGET_XBOX
        { static int _ts1=0; if(_ts1<1){ XDBG("sithTick: ResumeMusic\n"); _ts1++; } }
#endif
        sithSoundMixer_ResumeMusic(0);
#ifdef TARGET_XBOX
        { static int _ts2=0; if(_ts2<1){ XDBG("sithTick: TimeTick\n"); _ts2++; } }
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
            { static int _ts3=0; if(_ts3<1){ XDBGF("sithTick: dt=%f frames=%u\n",(float)sithTime_deltaSeconds,(unsigned)wholeFramesToApply); _ts3++; } }
#endif
            if (wholeFramesToApply > 0)
            {
                /* Only read controls when physics frames will actually run.
                 * wholeFramesToApply==0 (sithTime stubbed) → skip entirely. */
#ifdef TARGET_XBOX
                { static int _ts4=0; if(_ts4<1){ XDBG("sithTick: ReadControls\n"); _ts4++; } }
#endif
                sithControl_ReadControls();
                if ( g_sithMode != 2 )
                {
#ifdef TARGET_XBOX
                    { static int _ts5=0; if(_ts5<1){ XDBG("sithTick: ControlTick\n"); _ts5++; } }
#endif
                    sithControl_Tick(sithTime_deltaSeconds, sithTime_deltaMs);
                }
#ifdef TARGET_XBOX
                { static int _tsFR=0; if(_tsFR<1){ XDBG("sithTick: FinishRead\n"); _tsFR++; } }
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
                { static int _tsL=0; if(_tsL<1){ XDBGF("sithTick: loop i=%d SoundMixerTick\n",i); _tsL++; } }
#endif
                sithSoundMixer_Tick(sithTime_deltaSeconds);
#ifdef TARGET_XBOX
                { static int _tsE=0; if(_tsE<1){ XDBG("sithTick: EventAdv\n"); _tsE++; } }
#endif
                sithEvent_Advance();

                if ( sithComm_bSyncMultiplayer )
                    sithComm_Sync();

                if ( (g_debugmodeFlags & DEBUGFLAG_NO_AIEVENTS) == 0  && (!sithNet_isMulti || sithNet_isMulti && sithNet_isServer))
                {
#ifdef TARGET_XBOX
                    { static int _tsA=0; if(_tsA<1){ XDBG("sithTick: AITickAll\n"); _tsA++; } }
#endif
                    sithAI_TickAll();
                }

#ifdef TARGET_XBOX
                { static int _tsS=0; if(_tsS<1){ XDBG("sithTick: SurfaceTick\n"); _tsS++; } }
#endif
                sithSurface_Tick(sithTime_deltaSeconds);
#ifdef TARGET_XBOX
                { static int _tsTh=0; if(_tsTh<1){ XDBG("sithTick: ThingTickAll\n"); _tsTh++; } }
#endif
                sithThing_TickAll(sithTime_deltaSeconds, sithTime_deltaMs);
#ifdef TARGET_XBOX
                { static int _tsMo=0; if(_tsMo<1){ XDBG("sithTick: MotsTick\n"); _tsMo++; } }
#endif
                sithThing_MotsTick(0x1F, 0, 0);
#ifdef TARGET_XBOX
                { static int _tsCg=0; if(_tsCg<1){ XDBG("sithTick: CogTickAll\n"); _tsCg++; } }
#endif
                sithCogScript_TickAll();
#ifdef TARGET_XBOX
                { static int _tsX=0; if(_tsX<1){ XDBG("sithTick: loop end\n"); _tsX++; } }
#endif

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
            { static int _tsF=0; if(_tsF<1){ XDBG("sithTick: non-stepped SoundMixerTick\n"); _tsF++; } }
#endif
            sithSoundMixer_Tick(sithTime_deltaSeconds);
            sithEvent_Advance();

            if ( sithComm_bSyncMultiplayer )
                sithComm_Sync();

            if ( (g_debugmodeFlags & DEBUGFLAG_NO_AIEVENTS) == 0 && (!sithNet_isMulti || sithNet_isMulti && sithNet_isServer))
                sithAI_TickAll();

            sithSurface_Tick(sithTime_deltaSeconds);
            if ( g_sithMode != 2 )
            {
#ifdef FIXED_TIMESTEP_PHYS
                sithControl_ReadControls();
#endif
                sithControl_Tick(sithTime_deltaSeconds, sithTime_deltaMs);
#ifdef FIXED_TIMESTEP_PHYS
                sithControl_FinishRead();
#endif
            }

            sithThing_TickAll(sithTime_deltaSeconds, sithTime_deltaMs);
            sithThing_MotsTick(0x1F, 0, 0);

            sithCogScript_TickAll();
        }

        //sithAI_PrintThings();
#ifdef TARGET_XBOX
        { static int _ts6=0; if(_ts6<1){ XDBG("sithTick: Console\n"); _ts6++; } }
#endif
        sithConsole_AdvanceLogBuf();
#ifdef TARGET_XBOX
        { static int _ts7=0; if(_ts7<1){ XDBG("sithTick: HandleTimeLimit\n"); _ts7++; } }
#endif
        sithMulti_HandleTimeLimit(sithTime_deltaMs);
#ifdef TARGET_XBOX
        { static int _ts8=0; if(_ts8<1){ XDBG("sithTick: GamesaveFlush\n"); _ts8++; } }
#endif
        sithGamesave_Flush();
#ifdef TARGET_XBOX
        { static int _ts9=0; if(_ts9<1){ XDBG("sithTick: return0\n"); _ts9++; } }
#endif

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
#if defined(TARGET_XBOX)
    /* 90deg horizontal default reads as "fisheye" on 4:3 NTSC.  Drop to
     * 75deg horizontal (≈59deg vertical at 4:3) for a tighter, less
     * warped feel that matches modern FPS conventions on a TV. */
    jkPlayer_fov = 75;
    jkPlayer_fovIsVertical = 0;
#endif

    if ( (g_submodeFlags & 8) == 0 )
    {
        sithMain_sub_4C4D80();

#if defined(QOL_IMPROVEMENTS)
        if (sithCamera_currentCamera && sithCamera_currentCamera->rdCam.canvas)
        {
            // Set screen aspect ratio
            flex_t aspect = sithCamera_currentCamera->rdCam.canvas->half_screen_height / sithCamera_currentCamera->rdCam.canvas->half_screen_width;
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

    sithTime_Startup();
    sithInventory_Reset(sithPlayer_pLocalPlayerThing);

    sithCog_SendSimpleMessageToAll(SITH_MESSAGE_STARTUP, 0, 0, 0, 0);
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
