#include "jkMain.h"

#ifdef TARGET_XBOX
#include "xbox_debug.h"
#include "Platform/Xbox/xbox_splitscreen.h"
#ifdef XBOX_VERBOSE_FORMAT_LOGS
#define JKTRACE(msg) xbox_debug_Print(msg)
#define JKTRACEF xbox_debug_Printf
#else
#define JKTRACE(msg)
#define JKTRACEF if (0) xbox_debug_Printf
#endif
#else
#define JKTRACE(msg)
#define JKTRACEF(fmt, ...)
#endif

#include "../jk.h"
#include "Engine/rdroid.h"
#include "Engine/rdColormap.h"
#include "Main/sithMain.h"
#include "Devices/sithControl.h"
#include "Devices/sithSoundMixer.h"
#include "Dss/sithGamesave.h"
#include "Engine/sithCamera.h"
#include "Dss/sithMulti.h"
#include "Engine/sithRender.h"
#include "Engine/sithCamera.h"
#include "Gameplay/sithTime.h"
#include "Main/jkSmack.h"
#include "Main/jkGame.h"
#include "Main/jkCredits.h"
#include "Main/jkCutscene.h"
#include "Main/jkHudInv.h"
#include "Main/jkHud.h"
#include "Main/jkHudScope.h"
#include "Main/jkHudCameraView.h"
#include "Main/jkDev.h"
#include "Main/jkEpisode.h"
#include "Main/jkRes.h"
#include "Main/jkStrings.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIMultiTally.h"
#include "Gui/jkGUIForce.h"
#include "Gui/jkGUIMain.h"
#include "Gui/jkGUITitle.h"
#include "Gui/jkGUIDialog.h"
#include "Gui/jkGUIEsc.h"
#include "Gui/jkGUISingleTally.h"
#include "Gui/jkGUIMultiplayer.h"
#include "Gui/jkGUIDisplay.h"
#include "World/jkPlayer.h"
#include "Gameplay/jkSaber.h"
#include "World/sithWorld.h"
#include "Platform/stdControl.h"
#include "Platform/std3D.h"
#include "Win95/Windows.h"
#include "Win95/Video.h"
#include "Win95/stdComm.h"
#include "Win95/stdDisplay.h"
#include "Win95/Window.h"
#include "General/util.h"
#include "General/stdBitmap.h"
#include "General/stdPalEffects.h"
#include "General/stdString.h"
#include "World/jkPlayer.h"
#include "Dss/jkDSS.h"
#include "stdPlatform.h"

#ifdef TARGET_XBOX
#ifdef __cplusplus
extern "C"
#endif
void std3D_XboxReleaseMenuTextures(void);

static void jkMain_XboxLogTransitionResources(const char *phase)
{
    MEMORYSTATUS memStatus;
    sithWorld *world = sithWorld_pCurrentWorld;

    memStatus.dwLength = sizeof(memStatus);
    GlobalMemoryStatus(&memStatus);

    XDBGF("ResourceTrace: phase=%s state=%d next=%d gameMode=%d stop=%d init=%d ddraw=%d video=%d guiModes=%d six=%d eight=%d multi=%d server=%d split=%d localPlayers=%d phys=%lu page=%lu world=%p things=%d/%d sectors=%d surfaces=%d cogs=%d mats=%d/%d models=%lu/%lu sprites=%d/%d sounds=%d/%d level='%s'\n",
          phase ? phase : "(null)",
          jkSmack_currentGuiState,
          jkSmack_nextGuiState,
          jkSmack_gameMode,
          jkSmack_stopTick,
          jkMain_bInit,
          jkGame_isDDraw,
          Video_bOpened,
          jkGui_modesets,
          thing_six,
          thing_eight,
          sithNet_isMulti,
          sithNet_isServer,
          xboxSplitScreen_IsEnabled(),
          xboxSplitScreen_GetRequestedLocalPlayerCount(),
          memStatus.dwAvailPhys,
          memStatus.dwAvailPageFile,
          world,
          world ? world->numThingsLoaded : -1,
          world ? world->numThings : -1,
          world ? world->numSectors : -1,
          world ? world->numSurfaces : -1,
          world ? world->numCogsLoaded : -1,
          world ? world->numMaterialsLoaded : -1,
          world ? world->numMaterials : -1,
          world ? (unsigned long)world->numModelsLoaded : 0,
          world ? (unsigned long)world->numModels : 0,
          world ? world->numSpritesLoaded : -1,
          world ? world->numSprites : -1,
          world ? world->numSoundsLoaded : -1,
          world ? world->numSounds : -1,
          jkMain_aLevelJklFname);
}
#endif

#if defined(TARGET_TWL)
#define TICKRATE_MS (0) // no cap
#elif defined(QOL_IMPROVEMENTS)
#define TICKRATE_MS (jkPlayer_fpslimit ? 1000 / jkPlayer_fpslimit : 0) // no cap
#else
#define TICKRATE_MS (20) // 50fps
#endif

char jkMain_aLevelJklFnameMots[128];
char jkMain_motsIdk[128];

jkEpisodeEntry* jkMain_pEpisodeEnt = NULL;
jkEpisodeEntry* jkMain_pEpisodeEnt2 = NULL;

#ifdef TARGET_XBOX
static int jkMain_ResolveVideoPath(const char *fname, char *out)
{
    char candidate[128];

    _sprintf(candidate, "video%c%s", '\\', fname);
    if ( jkRes_FileExists(candidate, out, 128) )
        return 1;

    _sprintf(candidate, "Resource%cVIDEO%c%s", '\\', '\\', fname);
    if ( jkRes_FileExists(candidate, out, 128) )
        return 1;

    return 0;
}

static int jkMain_IsMultiplayerEpisodeType(jkEpisodeTypeFlags_t type)
{
    /* Match the normal host menu's multiplayer episode load mask. */
    return (type & (JK_EPISODE_DEATHMATCH | JK_EPISODE_4_UNK | JK_EPISODE_SPECIAL_CTF)) != 0;
}

static int jkMain_MultiplayerFlagsForEpisodeType(jkEpisodeTypeFlags_t type)
{
    if ( type & JK_EPISODE_SPECIAL_CTF )
        return MULTIMODEFLAG_TEAMS | MULTIMODEFLAG_2 | MULTIMODEFLAG_100;

    return 0;
}

static int jkMain_CreateLocalMultiplayerHost(const char *pGobPath, const char *pEpisodeName, jkEpisodeTypeFlags_t type)
{
    int multiModeFlags;
    HRESULT result;

    XDBGF("MPLoadTrace: CreateLocalMultiplayerHost enter gob='%s' jkl='%s' type=0x%x alreadyMulti=%d server=%d requestedPlayers=%d\n",
          pGobPath ? pGobPath : "(null)",
          pEpisodeName ? pEpisodeName : "(null)",
          type,
          sithNet_isMulti,
          sithNet_isServer,
          xboxSplitScreen_GetRequestedLocalPlayerCount());
    if ( sithNet_isMulti && sithNet_isServer )
    {
        XDBG("MPLoadTrace: CreateLocalMultiplayerHost already hosting\n");
        return 1;
    }

    multiModeFlags = jkMain_MultiplayerFlagsForEpisodeType(type);
    sithNet_scorelimit = 0;
    sithNet_multiplayer_timelimit = 0;
    XDBGF("MPLoadTrace: calling sithMulti_CreatePlayer flags=0x%x\n", multiModeFlags);
    result = sithMulti_CreatePlayer(L"OpenJKDF2 Xbox", L"", pGobPath, pEpisodeName, xboxSplitScreen_GetRequestedLocalPlayerCount(), 8, multiModeFlags, 180, 0);
    if ( result )
    {
        XDBGF("MPLoadTrace: sithMulti_CreatePlayer failed hr=0x%x\n", result);
        return 0;
    }

    XDBGF("MPLoadTrace: sithMulti_CreatePlayer ok multi=%d server=%d maxPlayers=%d\n",
          sithNet_isMulti,
          sithNet_isServer,
          jkPlayer_maxPlayers);
    return 1;
}
#endif

static jkGuiStateFuncs jkMain_aGuiStateFuncs[16] = {
    {0,  0,  0},
    {jkMain_VideoShow, jkMain_VideoTick, jkMain_VideoLeave},
    {jkMain_TitleShow, jkMain_TitleTick, jkMain_TitleLeave},
    {jkMain_MainShow, jkMain_MainTick, jkMain_MainLeave},
    {jkMain_VideoShow, jkMain_VideoTick, jkMain_VideoLeave},
    {jkMain_GameplayShow, jkMain_GameplayTick, jkMain_GameplayLeave},
    {jkMain_EscapeMenuShow, jkMain_EscapeMenuTick, jkMain_EscapeMenuLeave},
    {jkMain_CdSwitchShow,  0,  0},
    {jkMain_VideoShow, jkMain_VideoTick, jkMain_VideoLeave},
    {jkMain_EndLevelScreenShow, jkMain_EndLevelScreenTick, jkMain_EndLevelScreenLeave},
    {jkMain_VideoShow, jkMain_VideoTick, jkMain_VideoLeave},
    {jkMain_ChoiceShow, jkMain_ChoiceTick, jkMain_ChoiceLeave},
    {jkMain_CutsceneShow, jkMain_CutsceneTick, jkMain_CutsceneLeave},
    {jkMain_CreditsShow, jkMain_CreditsTick, jkMain_CreditsLeave},
    {jkMain_UnkShow, jkMain_UnkTick, jkMain_UnkLeave},
    {jkMain_VideoShow, jkMain_VideoTick, jkMain_VideoLeave}, // MOTS added
};

void jkMain_Startup()
{
    jkPlayer_Startup();
    jkPlayer_InitForceBins();
    jkMain_bInit = 1;
}

void jkMain_Shutdown()
{
    jkPlayer_Shutdown();
    sithMain_Close();

    // Added: memleak
    if ( jkEpisode_mLoad.paEntries )
    {
        pHS->free(jkEpisode_mLoad.paEntries);
        jkEpisode_mLoad.paEntries = 0;
    }

    // Added: prevent UAF
    jkMain_pEpisodeEnt = NULL;
    jkMain_pEpisodeEnt2 = NULL;

    jkMain_bInit = 0;
}

// TODO merge SDL2 in
#if !defined(SDL2_RENDER) && !defined(TARGET_TWL)
int jkMain_SetVideoMode()
{
    signed int result; // eax
    wchar_t *v1; // eax
    wchar_t *v2; // eax
    wchar_t *v3; // [esp-4h] [ebp-10h]
    wchar_t *v4; // [esp-4h] [ebp-10h]

    if ( jkGame_isDDraw )
        return 0;
    jkPlayer_Open();
    if ( Video_SetVideoDesc(sithWorld_pCurrentWorld->colormaps->colors) )
        goto LABEL_12;
    if ( !sithNet_isMulti )
    {
        thing_six = 1;
        sithControl_Close();
        v3 = jkStrings_GetUniStringWithFallback("ERR_CHANGING_VIDEO_DESC");
        v1 = jkStrings_GetUniStringWithFallback("ERR_CHANGING_VIDEO_MODE");
        jkGuiDialog_ErrorDialog(v1, v3);
        sithControl_Open();
        thing_six = 0;
    }
    _memcpy(&Video_modeStruct, &Video_modeStruct2, sizeof(Video_modeStruct));
    jkGuiDisplay_sub_4149C0();
    if ( Video_SetVideoDesc(sithWorld_pCurrentWorld->colormaps->colors) )
    {
LABEL_12:
        Windows_InitGdi(stdDisplay_pCurDevice->video_device[0].windowedMaybe);
        jkGame_isDDraw = 1;
        result = 1;
    }
    else
    {
        jkPlayer_Close();
        if ( sithControl_IsOpen() )
            sithControl_Close();
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = 3;
        v4 = jkStrings_GetUniStringWithFallback("ERR_CHANGING_VIDEO_ABORT");
        v2 = jkStrings_GetUniStringWithFallback("ERR_CHANGING_VIDEO_MODE");
        jkGuiDialog_ErrorDialog(v2, v4);
        result = 0;
    }
    return result;
}
#endif

void jkMain_SetVideoModeGdi()
{
    if ( jkGame_isDDraw )
    {
        Windows_ShutdownGdi();
        Video_SwitchToGDI();
        jkPlayer_Close();
        jkGame_isDDraw = 0;
    }
}

void jkMain_InitPlayerThings()
{
    jkPlayer_InitThings();
}

int jkMain_SwitchTo5_2()
{
    signed int result; // eax

    result = 1;
    jkSmack_gameMode = 4;
    jkPlayer_bLoadingSomething = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = 5;
    return result;
}

int jkMain_SwitchTo5(char *pJklFname)
{
    signed int result; // eax

    _strncpy(jkMain_aLevelJklFname, pJklFname, 0x7Fu);
    jkMain_aLevelJklFname[127] = 0;
    jkSmack_gameMode = 3;
    result = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = 5;
    return result;
}

// MOTS altered
void jkMain_GuiAdvance()
{
    unsigned int v1; // esi
    int v3; // esi
    int v4; // esi
    void (__cdecl *v5)(int, int); // ecx
    void (__cdecl *v7)(int, int); // ecx
    void (__cdecl *v8)(int); // ecx

    { static int top = 0; if (top < 3) { JKTRACE("GuiAdv: enter\n"); top++; } }
    if ( !g_app_suspended )
    {
        if ( thing_nine )
            stdControl_ToggleCursor(0);
        if ( thing_eight )
        {
            if ( sithNet_isMulti && !thing_six)
            {
                v1 = stdPlatform_GetTimeMsec();
                
                if (v1 > jkMain_lastTickMs + TICKRATE_MS)
                {
                    jkMain_lastTickMs = v1;
                    if (!sithMain_Tick()) return;
                }
                
                if ( g_sithMode == 5 )
                {
                    if ( jkGuiRend_thing_five )
                        jkGuiRend_thing_four = 1;
                    jkSmack_stopTick = 1;
                    jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
                    thing_nine = 0;
                    return;
                }
                if ( sithMulti_bTimelimitMet )
                {
                    sithMulti_bTimelimitMet = 0;
                    if ( sithNet_isServer )
                        jkDSS_SendEndLevel();
                }
                if ( sithMain_bEndLevel )
                {
                    sithMain_bEndLevel = 0;
                    if (Main_bMotsCompat)
                        jkPlayer_idkEndLevel(); // MOTS added
                    jkMain_EndLevel(1);
                }
                jkPlayer_nullsub_1(&playerThings[playerThingIdx]);
                jkGame_dword_552B5C += stdPlatform_GetTimeMsec() - v1;
                v3 = stdPlatform_GetTimeMsec();
                if ( g_app_suspended && jkSmack_currentGuiState != 6 ) {
#if defined(SDL2_RENDER) || defined(TARGET_TWL)
                    if (jkMain_lastTickMs == v1)
#endif
                    jkGame_Update();
                }
                jkGame_updateMsecsTotal += stdPlatform_GetTimeMsec() - v3;
            }
        }
        thing_nine = 0;
        return;
    }

    { static int dbg = 0; if (dbg < 5) { JKTRACE("GuiAdv: in suspended path\n"); dbg++; } }
    if ( !thing_nine )
    {
        switch ( jkSmack_currentGuiState )
        {
            case JK_GAMEMODE_VIDEO:
            case JK_GAMEMODE_VIDEO2:
            case JK_GAMEMODE_VIDEO3:
            case JK_GAMEMODE_VIDEO4:
            case JK_GAMEMODE_MOTS_CUTSCENE: // MOTS added
                jkCutscene_PauseShow(0);
                break;
            case JK_GAMEMODE_GAMEPLAY:
                stdControl_ToggleCursor(1);
                jkGame_ddraw_idk_palettes();
                break;
            default:
                break;
        }
        stdControl_Flush();
        thing_nine = 1;
    }
    { static int d3 = 0; if (d3 < 3) { if (jkSmack_stopTick) JKTRACE("GuiAdv: stopTick=1\n"); else JKTRACE("GuiAdv: stopTick=0!\n"); d3++; } }
    if ( jkSmack_stopTick && !jkGuiRend_thing_five )
    {
        JKTRACE("GuiAdv: TRANSITION\n");
        jkGuiRend_thing_four = 0;
        v4 = jkSmack_currentGuiState;
        v5 = jkMain_aGuiStateFuncs[jkSmack_currentGuiState].leaveFunc;
#ifdef TARGET_XBOX
        jkMain_XboxLogTransitionResources("gui-transition-before-leave");
#endif
#ifdef TARGET_XBOX
        if (jkSmack_currentGuiState == JK_GAMEMODE_VIDEO ||
            jkSmack_currentGuiState == JK_GAMEMODE_VIDEO2 ||
            jkSmack_currentGuiState == JK_GAMEMODE_VIDEO3 ||
            jkSmack_currentGuiState == JK_GAMEMODE_VIDEO4 ||
            jkSmack_currentGuiState == JK_GAMEMODE_MOTS_CUTSCENE)
        {
            JKTRACEF("GuiAdv: force video leave state=%d next=%d old=%p new=%p\n",
                     jkSmack_currentGuiState, jkSmack_nextGuiState, (void*)v5, (void*)jkMain_VideoLeave);
            v5 = jkMain_VideoLeave;
        }
#endif
        if ( v5 )
            v5(jkSmack_currentGuiState, jkSmack_nextGuiState);
#ifdef TARGET_XBOX
        jkMain_XboxLogTransitionResources("gui-transition-after-leave");
#endif
        //jk_printf("leave %u\n", jkSmack_currentGuiState);

        jkSmack_stopTick = 0;
        jkSmack_currentGuiState = jkSmack_nextGuiState;
        v7 = jkMain_aGuiStateFuncs[jkSmack_nextGuiState].showFunc;
#ifdef TARGET_XBOX
        if (jkSmack_nextGuiState == JK_GAMEMODE_VIDEO ||
            jkSmack_nextGuiState == JK_GAMEMODE_VIDEO2 ||
            jkSmack_nextGuiState == JK_GAMEMODE_VIDEO3 ||
            jkSmack_nextGuiState == JK_GAMEMODE_VIDEO4 ||
            jkSmack_nextGuiState == JK_GAMEMODE_MOTS_CUTSCENE)
        {
            JKTRACEF("GuiAdv: force video show state=%d prev=%d old=%p new=%p path='%s'\n",
                     jkSmack_nextGuiState, v4, (void*)v7, (void*)jkMain_VideoShow, jkMain_aLevelJklFname);
            v7 = jkMain_VideoShow;
        }
#endif
        if ( !v7 )
            goto LABEL_35;
        //jk_printf("show %u\n", jkSmack_currentGuiState);
        v7(jkSmack_nextGuiState, v4);
#ifdef TARGET_XBOX
        jkMain_XboxLogTransitionResources("gui-transition-after-show");
#endif
        //jk_printf("showed %u\n", jkSmack_currentGuiState);
    }
LABEL_35:
    { static int lbl=0; if(lbl<3){JKTRACEF("GuiAdv: LABEL_35 stopTick=%d state=%d\n",jkSmack_stopTick,jkSmack_currentGuiState);lbl++;} }
    if ( !jkSmack_stopTick )
    {
        v8 = jkMain_aGuiStateFuncs[jkSmack_currentGuiState].tickFunc;
#ifdef TARGET_XBOX
        if (jkSmack_currentGuiState == JK_GAMEMODE_VIDEO ||
            jkSmack_currentGuiState == JK_GAMEMODE_VIDEO2 ||
            jkSmack_currentGuiState == JK_GAMEMODE_VIDEO3 ||
            jkSmack_currentGuiState == JK_GAMEMODE_VIDEO4 ||
            jkSmack_currentGuiState == JK_GAMEMODE_MOTS_CUTSCENE)
        {
            static int vtForceLog = 0;
            if (vtForceLog < 16)
            {
                JKTRACEF("GuiAdv: force video tick state=%d old=%p new=%p rendering=%d stop=%d\n",
                         jkSmack_currentGuiState, (void*)v8, (void*)jkMain_VideoTick,
                         jkCutscene_isRendering, jkSmack_stopTick);
                vtForceLog++;
            }
            v8 = jkMain_VideoTick;
        }
#endif
        { static int tf=0; if(tf<3){JKTRACEF("GuiAdv: tickFunc=%p\n",(void*)v8);tf++;} }
        if ( v8 )
        {
            { static int tc=0; if(tc<3){JKTRACE("GuiAdv: calling tickFunc\n");tc++;} }
            v8(jkSmack_currentGuiState);
            { static int td=0; if(td<3){JKTRACE("GuiAdv: tickFunc returned\n");td++;} }
        }
    }
    { static int le=0; if(le<3){JKTRACE("GuiAdv: returning\n");le++;} }
}

void jkMain_EscapeMenuShow(int a1, int a2)
{
    if ( !sithNet_isMulti ){
        sithTime_Pause();
    }

    // Added
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
    
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    jkGuiEsc_Show();
}

void jkMain_EscapeMenuTick(int a2)
{
    unsigned int v1; // esi
    int v3; // esi

    if (!sithNet_isMulti) {
        return;
    }

    if (thing_six) {
        return;
    }
    
    if (!thing_eight) {
        return;
    }

    v1 = stdPlatform_GetTimeMsec();
    
    if (v1 > jkMain_lastTickMs + TICKRATE_MS)
    {
        jkMain_lastTickMs = v1;
        if (sithMain_Tick()) return;
    }
    
    if ( g_sithMode == 5 )
    {
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
    }
    else
    {
        if ( sithMulti_bTimelimitMet )
        {
            sithMulti_bTimelimitMet = 0;
            if ( sithNet_isServer )
                jkDSS_SendEndLevel();
        }
        if ( sithMain_bEndLevel )
        {
            sithMain_bEndLevel = 0;
            if (Main_bMotsCompat)
                jkPlayer_idkEndLevel(); // MOTS added
            jkMain_EndLevel(1);
        }
        jkPlayer_nullsub_1(&playerThings[playerThingIdx]);
        jkGame_dword_552B5C += stdPlatform_GetTimeMsec() - v1;
        v3 = stdPlatform_GetTimeMsec();
        if ( g_app_suspended && a2 != 6 ) {
#ifdef SDL2_RENDER
        if (jkMain_lastTickMs == v1)
#endif
            jkGame_Update();
        }
        jkGame_updateMsecsTotal += stdPlatform_GetTimeMsec() - v3;
    }
}

// MOTS altered
void jkMain_EscapeMenuLeave(int a2, int a3)
{
    int v3; // eax

#ifdef TARGET_XBOX
    jkMain_XboxLogTransitionResources("escape-leave-enter");
#endif

    if ( !sithNet_isMulti )
    {
        sithTime_Resume();
        if (a3 == JK_GAMEMODE_GAMEPLAY || a3 == JK_GAMEMODE_MOTS_CUTSCENE)
            sithSoundMixer_PauseSong(0);
    }

    // MOTS added
    if ( a3 != JK_GAMEMODE_GAMEPLAY && a3 != JK_GAMEMODE_MOTS_CUTSCENE)
    {
        if ( a3 == JK_GAMEMODE_ESCAPE )
        {
            stdControl_ToggleCursor(0);
            sithSoundMixer_StopAll();
        }
        if ( jkGame_isDDraw )
        {
            Windows_ShutdownGdi();
            Video_SwitchToGDI();
            jkPlayer_Close();
            jkGame_isDDraw = 0;
        }
        if ( a3 != JK_GAMEMODE_ESCAPE && jkMain_bInit )
        {
            jkPlayer_Shutdown();
            sithMain_Close();
            jkMain_bInit = 0;
            thing_eight = 0;
        }
        if ( sithNet_isMulti && a3 != JK_GAMEMODE_ESCAPE )
        {
            thing_eight = 0;
            if ( sithNet_isServer )
                DirectPlay_SetSessionFlagidk(0);
            if ( a3 == 3 ) {
                // MOTS added
                if (Main_bMotsCompat) {
                    sithMulti_LobbyMessage();
                }
                sithMulti_Shutdown();
            }
            else {
                sithMulti_LobbyMessage();
            }
            thing_six = 1;
            v3 = jkGuiMultiTally_Show(sithNet_isMulti);
            thing_six = 0;
            if ( v3 == -1 )
            {
                sithMulti_Shutdown();
                if ( jkGuiRend_thing_five )
                    jkGuiRend_thing_four = 1;
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
            }
        }
    }
    jkGui_SetModeGame();

#ifdef TARGET_XBOX
    jkMain_XboxLogTransitionResources("escape-leave-exit");
#endif
}

// MOTS altered
void jkMain_EndLevelScreenShow(int a1, int a2)
{
    stdControl_ToggleCursor(0); // Added

    if (!Main_bMotsCompat) {
        if ( jkEpisode_mLoad.type != JK_EPISODE_SINGLEPLAYER && jkSmack_gameMode == 2
          || jkGuiSingleTally_Show() != -1
          && (sithPlayer_GetBinAmt(SITHBIN_NEW_STARS) <= 0.0 && sithPlayer_GetBinAmt(SITHBIN_SPEND_STARS) <= 0.0
           || jkGuiForce_Show(1, 0.0, jkMain_dword_552B98, 0, 0, 1) != -1) )
        {
            jkMain_StartNextLevelInEpisode(0, 1);
            return;
        }
    }
    else 
    { 
        // MOTS added
        if (jkGuiSingleTally_Show() != -1) {
            if (sithPlayer_GetBinAmt(SITHBIN_NEW_STARS) <= 0.0 && sithPlayer_GetBinAmt(SITHBIN_SPEND_STARS) <= 0.0) {
                jkMain_StartNextLevelInEpisode(0, 1);
                return;
            }

            jkPlayer_idkEndLevel();
            if (jkGuiForce_Show(1, 0.0, jkMain_dword_552B98, 0, 0, 1) != -1) {
                jkMain_StartNextLevelInEpisode(0, 1);
                return;
            }
        }
    }

    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = 3;
    return;
}

void jkMain_EndLevelScreenTick(int a1)
{
    ;
}

void jkMain_EndLevelScreenLeave(int a1, int a2)
{
    ;
}

void jkMain_GameplayShow(int a1, int a2)
{
    signed int level_loaded; // esi
    signed int v3; // eax
    wchar_t *v4; // eax
    DWORD v5; // eax
    wchar_t *v6; // [esp-4h] [ebp-Ch]

    JKTRACEF("GameplayShow: enter a1=%d a2=%d gameMode=%d\n", a1, a2, jkSmack_gameMode);
#ifdef TARGET_XBOX
    jkMain_XboxLogTransitionResources("gameplay-show-enter");
    XDBGF("MPLoadTrace: GameplayShow enter a1=%d a2=%d gameMode=%d level='%s' multi=%d server=%d split=%d players=%d\n",
          a1,
          a2,
          jkSmack_gameMode,
          jkMain_aLevelJklFname,
          sithNet_isMulti,
          sithNet_isServer,
          xboxSplitScreen_IsEnabled(),
          xboxSplitScreen_GetRequestedLocalPlayerCount());
#endif
    level_loaded = 0;

    // MOTS added something here TODO
    if (a2 == JK_GAMEMODE_MOTS_CUTSCENE) {
        stdString_SafeStrCopy(jkMain_aLevelJklFname,jkMain_aLevelJklFnameMots, 128);
    }
    else if ( a2 == JK_GAMEMODE_ESCAPE )
    {
        sithSoundMixer_ResumeAll();
        sithSoundMixer_ResumeMusic(1);
#ifdef SDL2_RENDER
        jkGame_isDDraw = 0;
        Window_RemoveMsgHandler((WindowHandler_t)Windows_GdiHandler);
#endif
    }
    else if ( jkSmack_gameMode == JK_GAMEMODE_VIDEO2 )
    {
        jkPlayer_Startup();
        jkPlayer_InitForceBins();
        jkMain_bInit = 1;
        jkPlayer_InitSaber();
        sithMain_AutoSave();
    }
    else {
        // MOTS added
        jkMain_motsIdk[0] = 0;

        if ( jkSmack_gameMode == 1 )
        {
            jkGui_copies_string(gamemode_1_str);
            jkGuiTitle_ShowLoading(gamemode_1_str, 0);
        }
        else
        {
            jkGui_copies_string(jkMain_aLevelJklFname);
            jkGuiTitle_ShowLoading(jkMain_aLevelJklFname, 0);
        }

        // MOTS added:
        // jkEpisode_Shutdown
        v3 = 0; // Added
        JKTRACEF("GameplayShow: gameMode=%d, loading level '%s'\n", jkSmack_gameMode, jkMain_aLevelJklFname);
        if ( jkSmack_gameMode == 0)
        {
#ifdef JKM_DSS
            jkPlayer_SetAmmoMaximums(0);
#endif
            JKTRACE("GameplayShow: calling sithMain_Mode1Init\n");
            v3 = sithMain_Mode1Init(jkMain_aLevelJklFname);
            JKTRACEF("GameplayShow: Mode1Init returned %d\n", v3);
        }
        else if ( jkSmack_gameMode == 1 )
        {
#ifdef JKM_DSS
            jkPlayer_SetAmmoMaximums(0);
#endif
            v3 = sithGamesave_Load(jkMain_aLevelJklFname, 0, 1);
        }
        else if ( jkSmack_gameMode == 2 )
        {
#ifdef JKM_DSS
            jkPlayer_SetAmmoMaximums(jkPlayer_personality);
#endif
#ifdef TARGET_XBOX
            XDBG("MPLoadTrace: GameplayShow before sithMain_Mode1Init_3\n");
            jkMain_XboxLogTransitionResources("mp-load-before-world");
#endif
            v3 = sithMain_Mode1Init_3(jkMain_aLevelJklFname);
#ifdef TARGET_XBOX
            XDBGF("MPLoadTrace: GameplayShow sithMain_Mode1Init_3 returned %d\n", v3);
            jkMain_XboxLogTransitionResources("mp-load-after-world");
#endif
        }
        else
        {
            JKTRACEF("GameplayShow: ERROR gameMode=%d no handler!\n", jkSmack_gameMode);
        }

        level_loaded = v3;
#ifdef TARGET_XBOX
        XDBGF("MPLoadTrace: GameplayShow level_loaded=%d before LoadingFinalize\n", level_loaded);
        jkMain_XboxLogTransitionResources("gameplay-load-before-finalize");
#endif
        JKTRACEF("GameplayShow: level_loaded=%d\n", level_loaded);
        jkGuiTitle_LoadingFinalize();
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: GameplayShow after LoadingFinalize\n");
        jkMain_XboxLogTransitionResources("gameplay-load-after-finalize");
#endif
        if ( !level_loaded )
        {
            JKTRACE("GameplayShow: LEVEL LOAD FAILED\n");
#ifdef TARGET_XBOX
            jkMain_XboxLogTransitionResources("gameplay-load-fail-before-cleanup");
            XDBGF("MPLoadTrace: GameplayShow load failed cleanup multi=%d server=%d opened=%d currentWorld=%p\n",
                  sithNet_isMulti,
                  sithNet_isServer,
                  sithMain_bOpened,
                  sithWorld_pCurrentWorld);
#endif
            if ( sithNet_isMulti )
            {
                sithMulti_Shutdown();
                thing_six = 0;
                thing_eight = 0;
            }
#ifdef TARGET_XBOX
            jkMain_XboxLogTransitionResources("gameplay-load-fail-after-multi-cleanup");
#endif
            if ( jkGame_isDDraw )
            {
                Windows_ShutdownGdi();
                Video_SwitchToGDI();
                jkPlayer_Close();
                jkGame_isDDraw = 0;
            }
            if ( jkGuiRend_thing_five )
                jkGuiRend_thing_four = 1;
            jkSmack_stopTick = 1;
            jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
            v6 = jkStrings_GetUniStringWithFallback("ERR_CANNOT_LOAD_LEVEL");
            v4 = jkStrings_GetUniStringWithFallback("ERROR");
            jkGuiDialog_ErrorDialog(v4, v6);
            return;
        }

        // MOTS added:
        //sithWorld_GetMemorySize(sithWorld_pCurrentWorld,local_44,local_88);

        if ( !sithNet_isMulti )
        {
            JKTRACE("GameplayShow: jkPlayer_Startup\n");
            jkPlayer_Startup();
            JKTRACE("GameplayShow: jkPlayer_InitForceBins\n");
            jkPlayer_InitForceBins();
            jkMain_bInit = 1;
            if ( jkSmack_gameMode == 2 || !jkSmack_gameMode )
            {
                JKTRACE("GameplayShow: sithCamera_SetsFocus\n");
                sithCamera_SetsFocus();
                JKTRACE("GameplayShow: jkPlayer_InitSaber\n");
                jkPlayer_InitSaber();
                JKTRACE("GameplayShow: sithMain_AutoSave\n");
                sithMain_AutoSave();
                JKTRACE("GameplayShow: post-init done\n");
            }
        }
        else if ( sithNet_isServer )
        {
LABEL_28:
#ifdef TARGET_XBOX
            XDBG("MPLoadTrace: GameplayShow server post-load before ClearInventory\n");
#endif
            sithInventory_ClearInventory(sithPlayer_pLocalPlayerThing);
#ifdef TARGET_XBOX
            XDBG("MPLoadTrace: GameplayShow server post-load before MpcInitBins\n");
#endif
            jkPlayer_MpcInitBins(sithPlayer_pLocalPlayer);
            
#ifdef TARGET_XBOX
            XDBG("MPLoadTrace: GameplayShow server post-load before jkPlayer_Startup\n");
#endif
            jkPlayer_Startup();
#ifdef TARGET_XBOX
            XDBG("MPLoadTrace: GameplayShow server post-load before jkPlayer_InitForceBins\n");
#endif
            jkPlayer_InitForceBins();
            jkMain_bInit = 1;
            if ( jkSmack_gameMode == 2 || !jkSmack_gameMode )
            {
#ifdef TARGET_XBOX
                XDBG("MPLoadTrace: GameplayShow server post-load before sithCamera_SetsFocus\n");
#endif
                sithCamera_SetsFocus();
#ifdef TARGET_XBOX
                XDBG("MPLoadTrace: GameplayShow server post-load before jkPlayer_InitSaber\n");
#endif
                jkPlayer_InitSaber();
#ifdef TARGET_XBOX
                XDBG("MPLoadTrace: GameplayShow server post-load before sithMain_AutoSave\n");
#endif
                sithMain_AutoSave();
            }
            if ( sithNet_isMulti )
            {
                if ( sithNet_isServer )
                {
#ifdef TARGET_XBOX
                    XDBG("MPLoadTrace: GameplayShow server post-load before DirectPlay_SetSessionFlagidk\n");
#endif
                    DirectPlay_SetSessionFlagidk(1);
                    v5 = idx_13b4_related;
                    if ( idx_13b4_related >= (unsigned int)jkPlayer_maxPlayers )
                        v5 = jkPlayer_maxPlayers;
#ifdef TARGET_XBOX
                    XDBGF("MPLoadTrace: GameplayShow server post-load before DirectPlay_SetSessionDesc max=%u\n", (unsigned)v5);
#endif
                    DirectPlay_SetSessionDesc(jkMain_aLevelJklFname, v5);
                }
                if ( sithNet_isMulti )
#ifdef TARGET_XBOX
                {
                    XDBG("MPLoadTrace: GameplayShow server post-load before jkDSS_wrap_SendSaberInfo_alt\n");
#endif
                    jkDSS_wrap_SendSaberInfo_alt();
#ifdef TARGET_XBOX
                    XDBG("MPLoadTrace: GameplayShow server post-load after jkDSS_wrap_SendSaberInfo_alt\n");
                }
#endif
            }
        }
        else {
            thing_six = 1;
            stdControl_ToggleCursor(0);
#if !defined(TARGET_NO_MULTIPLAYER_MENUS)
            if ( jkGuiMultiplayer_ShowSynchronizing() == 1 )
            {
                thing_six = 0;
                stdControl_ToggleCursor(1);
                goto LABEL_28;
            }
#endif
            sithMain_Close();
            sithMulti_Shutdown();
            if ( jkGuiRend_thing_five )
                jkGuiRend_thing_four = 1;
            jkSmack_stopTick = 1;
            jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
            thing_six = 0;
            return;
        }

        if (Main_bMotsCompat) {
            sithPlayer_SetBinAmt(SITHBIN_NEW_STARS, 0);
            if (jkMain_motsIdk[0] != 0) {
                stdString_SafeStrCopy(jkMain_aLevelJklFnameMots, jkMain_aLevelJklFname,128);
                stdString_SafeStrCopy(jkMain_aLevelJklFname,jkMain_motsIdk,128);
                if (jkGuiRend_thing_five != 0) {
                    jkGuiRend_thing_four = 1;
                }
                jkMain_aLevelJklFname[127] = '\0';
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_MOTS_CUTSCENE;
                return;
            }
        }
    }

    JKTRACE("GameplayShow: calling SetVideoMode\n");
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: GameplayShow before jkMain_SetVideoMode\n");
        jkMain_XboxLogTransitionResources("gameplay-before-video-mode");
#endif
    if ( jkMain_SetVideoMode() )
    {
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: GameplayShow after jkMain_SetVideoMode ok\n");
        jkMain_XboxLogTransitionResources("gameplay-after-video-mode");
#endif
        JKTRACE("GameplayShow: SetVideoMode ok, ToggleCursor\n");
        stdControl_ToggleCursor(1);
        JKTRACE("GameplayShow: Flush\n");
        stdControl_Flush();
        JKTRACE("GameplayShow: jkGame_Update\n");
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: GameplayShow before warmup jkGame_Update\n");
#endif
        jkGame_Update();
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: GameplayShow after warmup jkGame_Update\n");
#endif
#ifdef TARGET_XBOX
        JKTRACE("GameplayShow: reset game clock after load/display warmup\n");
#endif
        sithTime_Startup();
        jkMain_lastTickMs = stdPlatform_GetTimeMsec();
#ifdef TARGET_XBOX
        JKTRACEF("GameplayShow: lastTick reset to %u\n", (unsigned)jkMain_lastTickMs);
#endif
        JKTRACE("GameplayShow: thing_eight=1\n");
        thing_eight = 1;
        JKTRACE("GameplayShow: done\n");
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: GameplayShow done\n");
        jkMain_XboxLogTransitionResources("gameplay-show-ready");
#endif
    }
    else
    {
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: GameplayShow jkMain_SetVideoMode FAILED\n");
        jkMain_XboxLogTransitionResources("gameplay-video-mode-failed");
#endif
        JKTRACE("GameplayShow: SetVideoMode FAILED\n");
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        thing_eight = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
    }
}

void jkMain_GameplayTick(int a2)
{
    unsigned int v1; // esi
    int v3; // esi
#if defined(TARGET_XBOX) && defined(XBOX_PERF_SMOKE)
    static unsigned int s_perfLastMs = 0;
    static unsigned int s_perfFrames = 0;
    static unsigned int s_perfSithTickCalls = 0;
    static unsigned int s_perfGameUpdateCalls = 0;
    static unsigned long s_perfSithTickMs = 0;
    static unsigned long s_perfPlayerMs = 0;
    static unsigned long s_perfGameUpdateMs = 0;
    static unsigned long s_perfOtherMs = 0;
    unsigned int perfFuncStart = stdPlatform_GetTimeMsec();
    if (!s_perfLastMs)
        s_perfLastMs = perfFuncStart;
#endif

    { static int ge=0; if(ge<3){JKTRACEF("GameplayTick: enter six=%d eight=%d\n",thing_six,thing_eight);ge++;} }

    if (thing_six) {
        return;
    }

    if (!thing_eight) {
        return;
    }

    v1 = stdPlatform_GetTimeMsec();
    { static int gt=0; if(gt<3){JKTRACEF("GameplayTick: tick v1=%u last=%u\n",v1,jkMain_lastTickMs);gt++;} }

    if (v1 > jkMain_lastTickMs + TICKRATE_MS)
    {
        jkMain_lastTickMs = v1;
        { static int gs=0; if(gs<3){JKTRACE("GameplayTick: calling sithMain_Tick\n");gs++;} }
#if defined(TARGET_XBOX) && defined(XBOX_PERF_SMOKE)
        { unsigned int perfT0 = stdPlatform_GetTimeMsec();
#endif
        if (sithMain_Tick()) { JKTRACE("GameplayTick: sithMain_Tick nonzero, returning\n"); return; }
#if defined(TARGET_XBOX) && defined(XBOX_PERF_SMOKE)
          s_perfSithTickMs += stdPlatform_GetTimeMsec() - perfT0;
          s_perfSithTickCalls++;
        }
#endif
        { static int ga=0; if(ga<3){JKTRACE("GameplayTick: after sithMain_Tick\n");ga++;} }
    }
    
    if ( g_sithMode == 5 )
    {
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
    }
    else
    {
        if ( sithMulti_bTimelimitMet )
        {
            sithMulti_bTimelimitMet = 0;
            if ( sithNet_isServer )
                jkDSS_SendEndLevel();
        }
        if ( sithMain_bEndLevel )
        {
            sithMain_bEndLevel = 0;
            if (Main_bMotsCompat)
                jkPlayer_idkEndLevel(); // MOTS added
            jkMain_EndLevel(1);
        }
        { static int gn=0; if(gn<3){JKTRACE("GameplayTick: nullsub\n");gn++;} }
#if defined(TARGET_XBOX) && defined(XBOX_PERF_SMOKE)
        { unsigned int perfPlayerStart = stdPlatform_GetTimeMsec();
#endif
        jkPlayer_nullsub_1(&playerThings[playerThingIdx]);
        jkGame_dword_552B5C += stdPlatform_GetTimeMsec() - v1;
#if defined(TARGET_XBOX) && defined(XBOX_PERF_SMOKE)
          s_perfPlayerMs += stdPlatform_GetTimeMsec() - perfPlayerStart;
        }
#endif
        v3 = stdPlatform_GetTimeMsec();
        { static int gg=0; if(gg<3){JKTRACEF("GameplayTick: jkGame_Update check susp=%d a2=%d\n",g_app_suspended,a2);gg++;} }
        if ( g_app_suspended && a2 != 6 ) {
#ifdef SDL2_RENDER
            if (jkMain_lastTickMs == v1)
#endif
            {
                { static int gu=0; if(gu<3){JKTRACE("GameplayTick: calling jkGame_Update\n");gu++;} }
#if defined(TARGET_XBOX) && defined(XBOX_PERF_SMOKE)
                { unsigned int perfUpdateStart = stdPlatform_GetTimeMsec();
#endif
                jkGame_Update();
#if defined(TARGET_XBOX) && defined(XBOX_PERF_SMOKE)
                  s_perfGameUpdateMs += stdPlatform_GetTimeMsec() - perfUpdateStart;
                  s_perfGameUpdateCalls++;
                }
#endif
                { static int gv=0; if(gv<3){JKTRACE("GameplayTick: jkGame_Update done\n");gv++;} }
            }
        }
        jkGame_updateMsecsTotal += stdPlatform_GetTimeMsec() - v3;
        { static int gr=0; if(gr<3){JKTRACE("GameplayTick: returning\n");gr++;} }
    }
#if defined(TARGET_XBOX) && defined(XBOX_PERF_SMOKE)
    s_perfFrames++;
    s_perfOtherMs += stdPlatform_GetTimeMsec() - perfFuncStart;
    {
        unsigned int nowMs = stdPlatform_GetTimeMsec();
        if (nowMs - s_perfLastMs >= 5000)
        {
            XPERF("PerfGame: frames=%u spanMs=%u sithCalls=%u gameCalls=%u sithMs=%lu playerMs=%lu gameMs=%lu totalMs=%lu\n",
                  s_perfFrames, nowMs - s_perfLastMs, s_perfSithTickCalls,
                  s_perfGameUpdateCalls, s_perfSithTickMs, s_perfPlayerMs,
                  s_perfGameUpdateMs, s_perfOtherMs);
            s_perfLastMs = nowMs;
            s_perfFrames = 0;
            s_perfSithTickCalls = 0;
            s_perfGameUpdateCalls = 0;
            s_perfSithTickMs = 0;
            s_perfPlayerMs = 0;
            s_perfGameUpdateMs = 0;
            s_perfOtherMs = 0;
        }
    }
#endif
}

// MOTS altered
void jkMain_GameplayLeave(int a2, int a3)
{
    int v3; // eax

#ifdef TARGET_XBOX
    jkMain_XboxLogTransitionResources("gameplay-leave-enter");
#endif

    // MOTS added
    if (a3 == JK_GAMEMODE_MOTS_CUTSCENE)
    {
#ifdef TARGET_XBOX
        jkMain_XboxLogTransitionResources("gameplay-leave-to-mots-cutscene");
#endif
        return;
    }

    if ( a3 == JK_GAMEMODE_ESCAPE )
    {
        stdControl_ToggleCursor(0);
        sithSoundMixer_StopAll();
    }
    if ( jkGame_isDDraw )
    {
        Windows_ShutdownGdi();
        Video_SwitchToGDI();
        jkPlayer_Close();
        jkGame_isDDraw = 0;
    }
    if ( a3 != 6 && jkMain_bInit )
    {
        jkPlayer_Shutdown();
        sithMain_Close();
        jkMain_bInit = 0;
        thing_eight = 0;
    }
    if ( sithNet_isMulti && a3 != 6 )
    {
        thing_eight = 0;
        if ( sithNet_isServer )
            DirectPlay_SetSessionFlagidk(0);
        if ( a3 == 3 ) {
            // MOTS added
            if (Main_bMotsCompat) {
                sithMulti_LobbyMessage();
            }
            sithMulti_Shutdown();
        }
        else {
            sithMulti_LobbyMessage();
        }
        thing_six = 1;
        v3 = jkGuiMultiTally_Show(sithNet_isMulti);
        thing_six = 0;
        if ( v3 == -1 )
        {
            sithMulti_Shutdown();
            if ( jkGuiRend_thing_five )
                jkGuiRend_thing_four = 1;
            jkSmack_stopTick = 1;
            jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
        }
    }

#ifdef TARGET_XBOX
    jkMain_XboxLogTransitionResources("gameplay-leave-exit");
#endif
}

void jkMain_TitleShow(int a1, int a2)
{
    jkGuiTitle_ShowLoadingStatic();
    sithMain_Load("static.jkl");
    jkHudInv_InitItems(); // MOTS inlined?
}

void jkMain_TitleTick(int a1)
{
    jkGuiTitle_LoadingFinalize();
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
}

void jkMain_TitleLeave(int a1, int a2)
{
    ;
}

void jkMain_MainShow(int a1, int a2)
{
    JKTRACEF("jkMain_MainShow: enter a1=%d a2=%d\n", a1, a2);
    stdControl_ShowCursor(1);
    JKTRACE("jkMain_MainShow: ShowCursor done\n");
    stdControl_ToggleCursor(0); // Added
    JKTRACE("jkMain_MainShow: calling jkGuiMain_Show\n");
    jkGuiMain_Show();
    JKTRACE("jkMain_MainShow: jkGuiMain_Show returned\n");
}

void jkMain_MainTick(int a1)
{
    ;
}

void jkMain_MainLeave(int a1, int a2)
{
    ;
}

void jkMain_ChoiceShow(int a1, int a2)
{
    int v1; // [esp+0h] [ebp-4h] BYREF

    if ( jkGuiForce_Show(0, 0.0, 1, 0, &v1, 1) == -1 )
    {
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
    }
    else
    {
        jkMain_StartNextLevelInEpisode(0, v1);
    }
}

void jkMain_ChoiceTick(int a1)
{
    ;
}

void jkMain_ChoiceLeave(int a1, int a2)
{
    ;
}

void jkMain_UnkShow(int a1, int a2)
{
    jkPlayer_SetAmmoMaximums(0); // MOTS added
}

void jkMain_UnkTick(int a1)
{
    jkRes_LoadGob(jkMain_strIdk);
    if ( jkEpisode_mLoad.paEntries )
    {
        pHS->free(jkEpisode_mLoad.paEntries);
        jkEpisode_mLoad.paEntries = 0;

        // Added: prevent UAF
        jkMain_pEpisodeEnt = NULL;
        jkMain_pEpisodeEnt2 = NULL;
    }
    jkEpisode_Load(&jkEpisode_mLoad);

    jkSmack_gameMode = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_GAMEPLAY;
}

void jkMain_UnkLeave(int a1, int a2)
{
    ;
}

int jkMain_sub_403470(char *a1)
{
    int result; // eax

    sithInventory_549FA0 = 1;
    _strncpy(jkMain_aLevelJklFname, a1, 0x7Fu);
    result = 0;
    jkMain_aLevelJklFname[127] = 0;
#ifdef TARGET_XBOX
    if ( jkMain_IsMultiplayerEpisodeType(jkGui_episodeLoad.type) )
    {
        if ( !jkMain_CreateLocalMultiplayerHost(jkRes_episodeGobName, a1, jkGui_episodeLoad.type) )
            return 0;
        jkSmack_gameMode = 2;
    }
    else
#endif
    jkSmack_gameMode = 0;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_GAMEPLAY;
    return result;
}

int jkMain_LoadFile(char *a1)
{
    if (jkRes_LoadCD(1))
    {
        sithInventory_549FA0 = 1;
        jkRes_LoadGob(a1);
        if ( jkEpisode_mLoad.paEntries )
        {
            pHS->free(jkEpisode_mLoad.paEntries);
            jkEpisode_mLoad.paEntries = 0;

            // Added: prevent UAF
            jkMain_pEpisodeEnt = NULL;
            jkMain_pEpisodeEnt2 = NULL;
        }
        if ( jkEpisode_Load(&jkEpisode_mLoad) )
        {
            return jkMain_StartNextLevelInEpisode(1, 1);
        }
        else
        {
            Windows_ErrorMsgboxWide("ERR_CANNOT_LOAD_FILE %s", a1);
            return 0;
        }
    }
    return 0;
}

int jkMain_loadFile2(char *pGobPath, char *pEpisodeName)
{
    BOOL v2; // esi
    int result; // eax

#ifdef TARGET_XBOX
    XDBGF("MPLoadTrace: jkMain_loadFile2 enter gob='%s' jkl='%s'\n",
          pGobPath ? pGobPath : "(null)",
          pEpisodeName ? pEpisodeName : "(null)");
#endif
    _strncpy(jkMain_aLevelJklFname, pEpisodeName, 0x7Fu);
    jkMain_aLevelJklFname[127] = 0;
    jkSmack_gameMode = 2;
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: jkMain_loadFile2 before jkRes_LoadGob\n");
#endif
    jkRes_LoadGob(pGobPath);
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: jkMain_loadFile2 after jkRes_LoadGob\n");
#endif
    if ( jkEpisode_mLoad.paEntries )
    {
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: jkMain_loadFile2 freeing previous episode entries\n");
#endif
        pHS->free(jkEpisode_mLoad.paEntries);
        jkEpisode_mLoad.paEntries = 0;

        // Added: prevent UAF
        jkMain_pEpisodeEnt = NULL;
        jkMain_pEpisodeEnt2 = NULL;
    }
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: jkMain_loadFile2 before jkEpisode_Load\n");
#endif
    v2 = jkEpisode_Load(&jkEpisode_mLoad);
#ifdef TARGET_XBOX
    XDBGF("MPLoadTrace: jkEpisode_Load returned %d type=0x%x numSeq=%d\n",
          v2,
          jkEpisode_mLoad.type,
          jkEpisode_mLoad.numSeq);
    XDBG("MPLoadTrace: jkMain_loadFile2 before jkEpisode_idk4\n");
#endif
    jkEpisode_idk4(&jkEpisode_mLoad, pEpisodeName);
#ifdef TARGET_XBOX
    XDBG("MPLoadTrace: jkMain_loadFile2 after jkEpisode_idk4\n");
#endif
    if ( v2 )
    {
#ifdef TARGET_XBOX
        if ( jkMain_IsMultiplayerEpisodeType(jkEpisode_mLoad.type) )
        {
            XDBG("MPLoadTrace: jkMain_loadFile2 detected multiplayer episode; creating host\n");
            if ( !jkMain_CreateLocalMultiplayerHost(pGobPath, pEpisodeName, jkEpisode_mLoad.type) )
            {
                XDBG("MPLoadTrace: jkMain_loadFile2 host creation failed\n");
                return 0;
            }
            XDBG("MPLoadTrace: jkMain_loadFile2 host creation ok\n");
        }
#endif
        result = 1;
        jkPlayer_bLoadingSomething = 1;
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = 5;
#ifdef TARGET_XBOX
        XDBGF("MPLoadTrace: jkMain_loadFile2 scheduled gameplay nextState=%d gameMode=%d level='%s'\n",
              jkSmack_nextGuiState,
              jkSmack_gameMode,
              jkMain_aLevelJklFname);
#endif
    }
    else
    {
#ifdef TARGET_XBOX
        XDBG("MPLoadTrace: jkMain_loadFile2 episode load failed\n");
#endif
        Windows_ErrorMsgboxWide("ERR_CANNOT_LOAD_FILE %s", pGobPath);
        result = 0;
    }
    return result;
}

// Added
int jkMain_LoadLevelSingleplayer(char *pGobPath, char *pEpisodeName)
{
    BOOL v2; // esi
    int result; // eax

    _strncpy(jkMain_aLevelJklFname, pEpisodeName, 0x7Fu);
    jkMain_aLevelJklFname[127] = 0;
    sithInventory_549FA0 = 1;
    jkSmack_gameMode = 0;
    jkRes_LoadGob(pGobPath);
    if ( jkEpisode_mLoad.paEntries )
    {
        pHS->free(jkEpisode_mLoad.paEntries);
        jkEpisode_mLoad.paEntries = 0;

        // Added: prevent UAF
        jkMain_pEpisodeEnt = NULL;
        jkMain_pEpisodeEnt2 = NULL;
    }
    v2 = jkEpisode_Load(&jkEpisode_mLoad);
    jkEpisode_idk4(&jkEpisode_mLoad, pEpisodeName);
    if ( v2 )
    {
#ifdef TARGET_XBOX
        if ( jkMain_IsMultiplayerEpisodeType(jkEpisode_mLoad.type) )
        {
            if ( !jkMain_CreateLocalMultiplayerHost(pGobPath, pEpisodeName, jkEpisode_mLoad.type) )
                return 0;
            jkSmack_gameMode = 2;
        }
#endif
        result = 1;
        jkPlayer_bLoadingSomething = 1;
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = 5;
    }
    else
    {
        Windows_ErrorMsgboxWide("ERR_CANNOT_LOAD_FILE %s", pGobPath);
        result = 0;
    }
    return result;
}

int jkMain_StartNextLevelInEpisode(int a1, int bIsAPath)
{
    jkEpisodeEntry *v2; // eax
    jkEpisodeEntry *v3; // ecx
    int v4; // eax
    signed int result; // eax

#ifdef TARGET_XBOX
    jkMain_XboxLogTransitionResources("start-next-level-enter");
#endif

    if ( !jkEpisode_mLoad.numSeq )
    {
        if ( jkGuiRend_thing_five )
        {
            jkGuiRend_thing_four = 1;
        }
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
#ifdef TARGET_XBOX
        jkMain_XboxLogTransitionResources("start-next-level-no-sequence");
#endif
        return 0;
    }
    if ( a1 )
    {
        v2 = jkEpisode_GetCurrentEpisodeEntry(&jkEpisode_mLoad);
        v3 = v2;
        jkMain_pEpisodeEnt = v2;
        jkMain_pEpisodeEnt2 = v2;
        jkPlayer_bLoadingSomething = 0;
    }
    else
    {
        v3 = jkMain_pEpisodeEnt;
        v2 = jkMain_pEpisodeEnt2;
    }
    if ( jkPlayer_bLoadingSomething )
    {
        jkMain_pEpisodeEnt = jkEpisode_GetCurrentEpisodeEntry(&jkEpisode_mLoad);
        v2 = jkEpisode_GetNextEntryInDecisionPath(&jkEpisode_mLoad, bIsAPath);
        v3 = jkMain_pEpisodeEnt;
        jkMain_pEpisodeEnt2 = v2;
        jkPlayer_bLoadingSomething = 0;
    }
    if ( !v2 )
    {
        v4 = jkGuiRend_thing_five;
        if ( v3->gotoA == -1 )
        {
            if ( jkGuiRend_thing_five )
                jkGuiRend_thing_four = 1;
            jkSmack_stopTick = 1;
            jkSmack_nextGuiState = JK_GAMEMODE_CREDITS;
#ifdef TARGET_XBOX
            jkMain_XboxLogTransitionResources("start-next-level-credits");
#endif
            return 1;
        }
        if ( v4 )
            jkGuiRend_thing_four = 1;

        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
#ifdef TARGET_XBOX
        jkMain_XboxLogTransitionResources("start-next-level-no-next");
#endif
        return 0;
    }
    if ( sithNet_isMulti && (sithNet_MultiModeFlags & MULTIMODEFLAG_SINGLE_LEVEL) != 0 )
    {
        v4 = jkGuiRend_thing_five;
        
        if ( v4 )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
#ifdef TARGET_XBOX
        jkMain_XboxLogTransitionResources("start-next-level-mp-single-level");
#endif
        return 0;
    }
    if ( v3->level == v2->level || jkSmack_currentGuiState == JK_GAMEMODE_ENDLEVEL )
    {
        if ( v2->type == 1 && jkSmack_currentGuiState == JK_GAMEMODE_GAMEPLAY )
        {
            if ( jkGuiRend_thing_five )
                jkGuiRend_thing_four = 1;
            jkSmack_stopTick = 1;
            jkSmack_nextGuiState = JK_GAMEMODE_CD_SWITCH;
            result = 1;
        }
        else
        {
            jkPlayer_bLoadingSomething = 1;
            jkMain_cd_swap_reverify(v2);
            result = 1;
        }
    }
    else
    {
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_ENDLEVEL;
        result = 1;
    }
#ifdef TARGET_XBOX
    jkMain_XboxLogTransitionResources("start-next-level-exit");
#endif
    return result;
}

int jkMain_cd_swap_reverify(jkEpisodeEntry *ent)
{
    int v1; // eax
    int v2; // eax
    signed int result; // eax
    signed int v4; // edi
    int v5; // edi
    signed int v6; // esi
    wchar_t *v7; // eax
    wchar_t *v8; // [esp-4h] [ebp-94h]
    char v9[128]; // [esp+10h] [ebp-80h] BYREF

    v1 = ent->type;
    if ( !v1 )
    {
        v5 = 0;
        v6 = 0;
        while ( !v6 )
        {
            if ( Windows_installType < 9 )
                v6 = jkRes_LoadCD(ent->cdNum);
            else
                v6 = 1;
            if ( !v6 )
            {
                v8 = jkStrings_GetUniStringWithFallback("GUI_CONFIRM_ABORTCD");
                v7 = jkStrings_GetUniStringWithFallback("GUI_ABORTCDREQUEST");
                if ( jkGuiDialog_YesNoDialog(v7, v8) )
                    v5 = 1;
            }
            if ( v5 )
            {
                if ( !v6 )
                {
                    if ( jkGuiRend_thing_five )
                        jkGuiRend_thing_four = 1;
                    jkSmack_stopTick = 1;
                    jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
                    return 1;
                }
                break;
            }
        }
        stdString_SafeStrCopy(jkMain_aLevelJklFname, ent->fileName, 128);
        jkSmack_gameMode = sithNet_isMulti != 0 ? 2 : 0;
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_GAMEPLAY;
        return 1;
    }
    v2 = v1 - 1;
    if ( v2 )
    {
        if ( v2 == 1 )
        {
            if ( jkGuiRend_thing_five )
                jkGuiRend_thing_four = 1;
            jkSmack_stopTick = 1;
            jkSmack_nextGuiState = JK_GAMEMODE_CHOICE; // force select/choice?
            return 1;
        }
        return 1;
    }

    // Added: Move down
    //if ( jkPlayer_setDisableCutscenes )
    //    v4 = 0;
    //else
    //    v4 = jkRes_LoadCD(ent->cdNum);

    jkPlayer_WriteConfSwap(&playerThings[playerThingIdx], ent->cdNum, ent->fileName);
    // Added: Move down
    //if ( !v4 )
    //    return jkMain_StartNextLevelInEpisode(0, 1);

    // Added: Cutscenes disabled
    if ( jkPlayer_setDisableCutscenes )
        return jkMain_StartNextLevelInEpisode(0, 1);

#ifdef TARGET_XBOX
    if ( !jkMain_ResolveVideoPath(ent->fileName, v9) ) {
#else
    _sprintf(v9, "video%c%s", 92, ent->fileName);
    if ( !util_FileExists(v9) ) {
#endif
        // Added: check file first before asking for CDs
        v4 = jkRes_LoadCD(ent->cdNum);

        if ( !v4 ) {
            return jkMain_StartNextLevelInEpisode(0, 1);
        }

#ifdef TARGET_XBOX
        if ( !jkMain_ResolveVideoPath(ent->fileName, v9) ) {
#else
        if ( !util_FileExists(v9) ) {
#endif
            return jkMain_StartNextLevelInEpisode(0, 1);
        }
    }
    jkRes_FileExists(v9, jkMain_aLevelJklFname, 128);
    switch ( jkSmack_currentGuiState )
    {
        case 3:
        case 9:
            if ( jkGuiRend_thing_five )
                jkGuiRend_thing_four = 1;
            jkSmack_stopTick = 1;
            jkSmack_nextGuiState = JK_GAMEMODE_VIDEO4;
            result = 1;
            break;
        case 5:
        case 7:
        case 8:
            if ( jkGuiRend_thing_five )
                jkGuiRend_thing_four = 1;
            jkSmack_stopTick = 1;
            jkSmack_nextGuiState = JK_GAMEMODE_VIDEO3;
            result = 1;
            break;
        default:
            return 1;
    }
    return result;
}

int jkMain_SetMap(int levelNum)
{
    jkEpisode_EndLevel(&jkEpisode_mLoad, levelNum);
    return jkMain_cd_swap_reverify(jkEpisode_GetCurrentEpisodeEntry(&jkEpisode_mLoad));
}

void jkMain_do_guistate6()
{
    if ( !jkSmack_stopTick )
    {
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_ESCAPE;
    }
}

int jkMain_sub_4034D0(char *a1, char *a2, char *a3, wchar_t *a4)
{
    sithInventory_549FA0 = 0;
    _strncpy(jkMain_aLevelJklFname, a2, 0x7Fu);
    jkMain_aLevelJklFname[127] = 0;
    _strncpy(jkMain_strIdk, a1, 0x7Fu);
    jkMain_strIdk[127] = 0;
    _strncpy(gamemode_1_str, a3, 0x7Fu);
    gamemode_1_str[127] = 0;
    _wcsncpy(jkMain_wstrIdk, a4, 0x7Fu);

    jkMain_wstrIdk[127] = 0;
    jkPlayer_bLoadingSomething = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_UNK;
    return 1;
}

int jkMain_MissionReload()
{
    signed int result; // eax

    result = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_GAMEPLAY;
    return result;
}

int jkMain_MenuReturn()
{
    signed int result; // eax

    result = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
    return result;
}

int jkMain_EndLevel(int bIsAPath)
{
    if (!Main_bMotsCompat && jkEpisode_mLoad.numSeq )
    {
        jkEpisodeEntry* pEpisodeEnt = jkEpisode_GetCurrentEpisodeEntry(&jkEpisode_mLoad);
        if ( pEpisodeEnt->darkpow || pEpisodeEnt->lightpow )
        {
            if ( pEpisodeEnt->lightpow )
            {
                if ( pEpisodeEnt->lightpow >= SITHBIN_FP_START && pEpisodeEnt->lightpow <= SITHBIN_FP_END && jkPlayer_GetChoice() != 2 )
                    sithInventory_SetCarries(playerThings[playerThingIdx].actorThing, pEpisodeEnt->lightpow, 1);
            }
            if ( pEpisodeEnt->darkpow )
            {
                if ( pEpisodeEnt->darkpow >= SITHBIN_FP_START && pEpisodeEnt->darkpow <= SITHBIN_FP_END && jkPlayer_GetChoice() != 1 )
                    sithInventory_SetCarries(playerThings[playerThingIdx].actorThing, pEpisodeEnt->darkpow, 1);
            }
        }
    }

    if (Main_bMotsCompat) {
        jkPlayer_idkEndLevel();
    }

    return jkMain_StartNextLevelInEpisode(0, bIsAPath);
}

void jkMain_CdSwitchShow(int a1, int a2)
{
    jkMain_StartNextLevelInEpisode(0, 1);
}

// MOTS altered
void jkMain_VideoShow(int a1, int a2)
{
    signed int result; // eax

    // Added: Fix a bug with the door on Level 10?
    //if (Main_bMotsCompat && !sithNet_isMulti )
    //    sithTime_Pause();

#ifdef TARGET_XBOX
    stdPlatform_Printf("CutsceneTrace: VideoShow state=%d prev=%d path='%s'\n", a1, a2, jkMain_aLevelJklFname);
#endif
    result = jkCutscene_sub_421310(jkMain_aLevelJklFname);
#ifdef TARGET_XBOX
    stdPlatform_Printf("CutsceneTrace: VideoShow openResult=%d stop=%d next=%d\n", result, jkSmack_stopTick, jkSmack_nextGuiState);
#endif
    if ( !result )
    {
        Windows_ErrorMsgboxWide("ERR_CANNOT_LOAD_FILE %s", jkMain_aLevelJklFname);
        switch ( a1 )
        {
            case JK_GAMEMODE_VIDEO:
                result = 1;
                if ( jkGuiRend_thing_five )
                    jkGuiRend_thing_four = 1;
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_TITLE;
                break;
            case JK_GAMEMODE_VIDEO2:
                result = 1;
                if ( jkGuiRend_thing_five )
                    jkGuiRend_thing_four = 1;
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_CUTSCENE;
                break;
            case JK_GAMEMODE_VIDEO3:
            case JK_GAMEMODE_VIDEO4:
                result = jkMain_StartNextLevelInEpisode(0, 1);
                break;
            case JK_GAMEMODE_MOTS_CUTSCENE: // MOTS added
                if (jkGuiRend_thing_five != 0) {
                    jkGuiRend_thing_four = 1;
                }
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_GAMEPLAY;
                return;
            default:
                result = 1;
                if ( jkGuiRend_thing_five )
                    jkGuiRend_thing_four = 1;
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
                break;
        }
    }
    return;
}

void jkMain_VideoTick(int a2)
{
    signed int result; // eax

    result = jkCutscene_smack_related_loops();
    if ( result )
    {
        result = a2 - 1;
        switch ( a2 )
        {
            case JK_GAMEMODE_VIDEO:
                result = 1;
                if ( jkGuiRend_thing_five )
                    jkGuiRend_thing_four = 1;
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_TITLE;
                break;
            case JK_GAMEMODE_VIDEO2:
                result = 1;
                if ( jkGuiRend_thing_five )
                    jkGuiRend_thing_four = 1;
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_CUTSCENE;
                break;
            case JK_GAMEMODE_VIDEO3:
                result = 1;
                if ( jkGuiRend_thing_five )
                    jkGuiRend_thing_four = 1;
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_ENDLEVEL;
                break;
            case JK_GAMEMODE_VIDEO4:
            case JK_GAMEMODE_MOTS_CUTSCENE: // MOTS added
                result = 1;
                if ( jkGuiRend_thing_five )
                    jkGuiRend_thing_four = 1;
                jkSmack_stopTick = 1;
                jkSmack_nextGuiState = JK_GAMEMODE_GAMEPLAY;
                break;
            default:
                return;
        }
    }
    return;
}

void jkMain_VideoLeave(int a1, int a2)
{
    // Added: Fix a bug with the door on Level 10?
    //if (Main_bMotsCompat && !sithNet_isMulti )
    //    sithTime_Resume();

    jkCutscene_sub_421410();
    if ( a1 == JK_GAMEMODE_VIDEO3 || a1 == JK_GAMEMODE_VIDEO4 )
        jkMain_StartNextLevelInEpisode(0, 1);
}

void jkMain_CreditsShow(int a1, int a2)
{
    if ( !jkCredits_Show() )
    {
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
    }
}

void jkMain_CreditsTick(int a1)
{
    if ( jkCredits_Tick() )
    {
        if ( jkGuiRend_thing_five )
            jkGuiRend_thing_four = 1;
        jkSmack_stopTick = 1;
        jkSmack_nextGuiState = JK_GAMEMODE_MAIN;
    }
}

void jkMain_CreditsLeave(int a1, int a2)
{
    jkCredits_Skip();
}

void jkMain_CutsceneShow(int a1, int a2)
{
    jkGuiMain_ShowCutscenes();
}

void jkMain_CutsceneTick(int a1)
{
    ;
}

void jkMain_CutsceneLeave(int a1, int a2)
{
    ;
}

int jkMain_SwitchTo13()
{
    signed int result; // eax

    result = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_CREDITS;
    return result;
}

int jkMain_SwitchTo12()
{
    signed int result; // eax

    result = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_CUTSCENE;
    return result;
}

int jkMain_SwitchTo4(const char *pFpath)
{
    int result; // eax

    jkRes_FileExists(pFpath, jkMain_aLevelJklFname, 128);
    result = 1;
    if ( jkGuiRend_thing_five )
        jkGuiRend_thing_four = 1;
    jkSmack_stopTick = 1;
    jkSmack_nextGuiState = JK_GAMEMODE_VIDEO2;
    return result;
}

// MOTS added
void jkMain_StartupCutscene(char *pCutsceneStr)
{
    char local_80 [128];

    jkPlayer_WriteConfSwap(playerThings + playerThingIdx, 1, pCutsceneStr);
#ifdef TARGET_XBOX
    if (jkMain_ResolveVideoPath(pCutsceneStr, local_80)) {
#else
    _sprintf(local_80,"video%c%s", '\\', pCutsceneStr);

    if (util_FileExists(local_80)) {
#endif
        jkRes_FileExists(local_80, jkMain_motsIdk, 0x80);
    }
}

#if defined(SDL2_RENDER) || defined(TARGET_TWL)
void jkMain_FixRes()
{
    uint32_t newW;
    uint32_t newH;
    if (!jkGame_isDDraw)
        return;
    
    newW = Window_xSize;
    newH = Window_ySize;

    //if (jkGame_isDDraw)
    {
        newW = (uint32_t)((flex_t)Window_xSize * ((480.0*2.0)/Window_ySize));
        newH = 480*2;
    }

    if (newW > Window_xSize)
    {
        newW = Window_xSize;
        newH = Window_ySize;
    }

    if (newW < 640)
        newW = 640;
    if (newH < 480)
        newH = 480;

    Video_modeStruct.viewSizeIdx = 0;
    Video_modeStruct.aViewSizes[Video_modeStruct.viewSizeIdx].xMin = 0;
    Video_modeStruct.aViewSizes[Video_modeStruct.viewSizeIdx].yMin = 0;
    Video_modeStruct.aViewSizes[Video_modeStruct.viewSizeIdx].xMax = newW / 2;
    Video_modeStruct.aViewSizes[Video_modeStruct.viewSizeIdx].yMax = newH / 2;
    
    stdDisplay_pCurVideoMode->format.width = newW;
    stdDisplay_pCurVideoMode->format.height = newH;
    stdDisplay_pCurVideoMode->widthMaybe = newW;
    stdDisplay_pCurVideoMode->format.width_in_pixels = newW;
    stdDisplay_pCurVideoMode->format.width_in_bytes = newW;
    
    Video_menuBuffer.format.width_in_pixels = newW;
    Video_otherBuf.format.width_in_pixels = newW;
    Video_menuBuffer.format.width_in_bytes = newW;
    Video_otherBuf.format.width_in_bytes = newW;
    Video_menuBuffer.format.width = newW;
    Video_otherBuf.format.width = newW;
    Video_menuBuffer.format.height = newH;
    Video_otherBuf.format.height = newH;
    
    _memcpy(&Video_format, &stdDisplay_pCurVideoMode->format, sizeof(stdVBufferTexFmt));
    _memcpy(&Video_format2, &stdDisplay_pCurVideoMode->format, sizeof(stdVBufferTexFmt));
    
    Video_format.width = newW;
    Video_format.height = newH;
    
    jkDev_Close();
    jkHud_Close();
    if (Main_bMotsCompat) {
        jkHudScope_Close();
        jkHudCameraView_Close();
    }
    jkHudInv_Close();
    sithCamera_Close();
    rdCanvas_Free(Video_pCanvas);

#if defined(SDL2_RENDER)
    rdCanvas_Free(Video_pCanvasOverlayMap);
#endif

    jkHudInv_LoadItemRes();
    jkHud_Open();
    if (Main_bMotsCompat) {
        jkHudScope_Open();
        jkHudCameraView_Open();
    }
    jkDev_Open();
    
    Video_pCanvas = rdCanvas_New(2, Video_pMenuBuffer, Video_pVbufIdk, 0, 0, newW, newH, 6);
#if defined(SDL2_RENDER)
    Video_pCanvasOverlayMap = rdCanvas_New(2, Video_pOverlayMapBuffer, Video_pOverlayMapBuffer, 0, 0, newW, newH, 6);
#endif
    sithCamera_Open(Video_pCanvas, stdDisplay_pCurVideoMode->widthMaybe);
}

int jkMain_SetVideoMode()
{
    signed int result; // eax
    wchar_t *v1; // eax
    wchar_t *v2; // eax
    wchar_t *v3; // [esp-4h] [ebp-10h]
    wchar_t *v4; // [esp-4h] [ebp-10h]
    uint32_t newW;
    uint32_t newH;

    JKTRACE("SetVideoMode: enter\n");
    if ( jkGame_isDDraw )
    {
        JKTRACE("SetVideoMode: isDDraw, returning 0\n");
        return 0;
    }

    JKTRACE("SetVideoMode: sithControl_Open\n");
    sithControl_Open();
    JKTRACE("SetVideoMode: sithRender_SetRenderWeaponHandle\n");
    sithRender_SetRenderWeaponHandle(jkPlayer_renderSaberWeaponMesh);

    newW = Window_xSize;
    newH = Window_ySize;

    //if (jkGame_isDDraw)
    {
        newW = (uint32_t)((flex_t)Window_xSize * ((480.0*2.0)/Window_ySize));
        newH = 480*2;
    }

    if (newW > Window_xSize)
    {
        newW = Window_xSize;
        newH = Window_ySize;
    }

    if (newW < 640)
        newW = 640;
    if (newH < 480)
        newH = 480;

    Video_modeStruct.viewSizeIdx = 0;
    Video_modeStruct.aViewSizes[Video_modeStruct.viewSizeIdx].xMin = 0;
    Video_modeStruct.aViewSizes[Video_modeStruct.viewSizeIdx].yMin = 0;
    Video_modeStruct.aViewSizes[Video_modeStruct.viewSizeIdx].xMax = newW / 2;
    Video_modeStruct.aViewSizes[Video_modeStruct.viewSizeIdx].yMax = newH / 2;

    JKTRACEF("SetVideoMode: res=%ux%u pCurVideoMode=%p\n", newW, newH, (void*)stdDisplay_pCurVideoMode);
    stdDisplay_pCurVideoMode->format.width = newW;
    stdDisplay_pCurVideoMode->format.height = newH;
    stdDisplay_pCurVideoMode->widthMaybe = newW;
    stdDisplay_pCurVideoMode->format.width_in_pixels = newW;
    stdDisplay_pCurVideoMode->format.width_in_bytes = newW;
    
    Video_menuBuffer.format.width_in_pixels = newW;
    Video_otherBuf.format.width_in_pixels = newW;
    Video_menuBuffer.format.width_in_bytes = newW;
    Video_otherBuf.format.width_in_bytes = newW;
    Video_menuBuffer.format.width = newW;
    Video_otherBuf.format.width = newW;
    Video_menuBuffer.format.height = newH;
    Video_otherBuf.format.height = newH;
    
    _memcpy(&Video_format, &stdDisplay_pCurVideoMode->format, sizeof(stdVBufferTexFmt));
    _memcpy(&Video_format2, &stdDisplay_pCurVideoMode->format, sizeof(stdVBufferTexFmt));
    
    Video_format.width = newW;
    Video_format.height = newH;
    
    Window_AddMsgHandler((WindowHandler_t)Windows_GdiHandler);

    rdroid_curAcceleration = 1;
    stdPalEffects_RefreshPalette();
    JKTRACE("SetVideoMode: sithRender_SetPalette\n");
#ifdef TARGET_XBOX
    JKTRACEF("SetVideoMode: pre SetPalette mode=%d multi=%d split=%d world=%p cmap0=%p cur=%p ident=%p accel=%d pal=%p\n",
             jkSmack_gameMode,
             sithNet_isMulti,
             xboxSplitScreen_IsEnabled(),
             (void*)sithWorld_pCurrentWorld,
             sithWorld_pCurrentWorld ? (void*)sithWorld_pCurrentWorld->colormaps : 0,
             (void*)rdColormap_pCurMap,
             (void*)rdColormap_pIdentityMap,
             rdroid_curAcceleration,
             (void*)stdDisplay_GetPalette());
#endif
    sithRender_SetPalette(stdDisplay_GetPalette());
#ifdef TARGET_XBOX
    JKTRACEF("SetVideoMode: post SetPalette mode=%d multi=%d split=%d world=%p cmap0=%p cur=%p ident=%p accel=%d\n",
             jkSmack_gameMode,
             sithNet_isMulti,
             xboxSplitScreen_IsEnabled(),
             (void*)sithWorld_pCurrentWorld,
             sithWorld_pCurrentWorld ? (void*)sithWorld_pCurrentWorld->colormaps : 0,
             (void*)rdColormap_pCurMap,
             (void*)rdColormap_pIdentityMap,
             rdroid_curAcceleration);
    std3D_XboxDebugLogPaletteState("SetVideoMode-post");
#endif

    JKTRACE("SetVideoMode: jkHudInv_LoadItemRes\n");
    jkHudInv_LoadItemRes();
    JKTRACE("SetVideoMode: jkHud_Close\n");
    jkHud_Close();
    if (Main_bMotsCompat) {
        jkHudScope_Close();
        jkHudCameraView_Close();
    }
#ifdef TARGET_XBOX
    JKTRACE("SetVideoMode: release transient menu textures\n");
    std3D_XboxReleaseMenuTextures();
#endif
    JKTRACE("SetVideoMode: jkHud_Open\n");
    jkHud_Open();
    JKTRACE("SetVideoMode: jkHud_Open done\n");
    if (Main_bMotsCompat) {
        jkHudScope_Open();
        jkHudCameraView_Open();
    }
    jkDev_Open();

    JKTRACEF("SetVideoMode: rdCanvas_New pMenuBuffer=%p pVbufIdk=%p\n", (void*)Video_pMenuBuffer, (void*)Video_pVbufIdk);
    Video_pCanvas = rdCanvas_New(2, Video_pMenuBuffer, Video_pVbufIdk, 0, 0, newW, newH, 6);
    JKTRACEF("SetVideoMode: rdCanvas_New returned %p\n", (void*)Video_pCanvas);
    if ( !Video_pCanvas )
    {
#ifdef TARGET_XBOX
        JKTRACE("SetVideoMode: rdCanvas_New FAILED\n");
        jkMain_XboxLogTransitionResources("gameplay-video-mode-canvas-failed");
#endif
        jkDev_Close();
        if (Main_bMotsCompat) {
            jkHudScope_Close();
            jkHudCameraView_Close();
        }
        jkHud_Close();
        jkHudInv_Close();
        Window_RemoveMsgHandler((WindowHandler_t)Windows_GdiHandler);
        if ( sithControl_IsOpen() )
            sithControl_Close();
        return 0;
    }
#if defined(SDL2_RENDER)
    Video_pCanvasOverlayMap = rdCanvas_New(2, Video_pOverlayMapBuffer, Video_pOverlayMapBuffer, 0, 0, newW, newH, 6);
    if ( !Video_pCanvasOverlayMap )
    {
#ifdef TARGET_XBOX
        JKTRACE("SetVideoMode: overlay rdCanvas_New FAILED\n");
        jkMain_XboxLogTransitionResources("gameplay-video-mode-overlay-canvas-failed");
#endif
        sithCamera_Close();
        rdCanvas_Free(Video_pCanvas);
        Video_pCanvas = 0;
        jkDev_Close();
        if (Main_bMotsCompat) {
            jkHudScope_Close();
            jkHudCameraView_Close();
        }
        jkHud_Close();
        jkHudInv_Close();
        Window_RemoveMsgHandler((WindowHandler_t)Windows_GdiHandler);
        if ( sithControl_IsOpen() )
            sithControl_Close();
        return 0;
    }
#endif
#ifdef JKM_LIGHTING
    if (Main_bMotsCompat) {
        sithRender_SetSomeRenderflag(0xaa);
    }
    else {
        sithRender_SetSomeRenderflag(0x2a);
    }
#else
    sithRender_SetSomeRenderflag(0x2a);
#endif
    sithRender_SetGeoMode(Video_modeStruct.geoMode);
    sithRender_SetLightMode(Video_modeStruct.lightMode);
    sithRender_SetTexMode(Video_modeStruct.texMode);
    JKTRACE("SetVideoMode: sithCamera_Open\n");
    sithCamera_Open(Video_pCanvas, stdDisplay_pCurVideoMode->widthMaybe);
    JKTRACE("SetVideoMode: sithCamera_Open done\n");

    if ( !stdDisplay_SetMode(0, 0, 0) )
    {
#ifdef TARGET_XBOX
        JKTRACE("SetVideoMode: stdDisplay_SetMode FAILED\n");
        jkMain_XboxLogTransitionResources("gameplay-video-mode-display-failed");
#endif
        sithCamera_Close();
#if defined(SDL2_RENDER)
        rdCanvas_Free(Video_pCanvasOverlayMap);
        Video_pCanvasOverlayMap = 0;
#endif
        rdCanvas_Free(Video_pCanvas);
        Video_pCanvas = 0;
        jkDev_Close();
        if (Main_bMotsCompat) {
            jkHudScope_Close();
            jkHudCameraView_Close();
        }
        jkHud_Close();
        jkHudInv_Close();
        Window_RemoveMsgHandler((WindowHandler_t)Windows_GdiHandler);
        if ( sithControl_IsOpen() )
            sithControl_Close();
        return 0;
    }
    JKTRACE("SetVideoMode: done\n");

    Video_bOpened = 1;
    jkGame_isDDraw = 1;
    return 1;
}
#endif
