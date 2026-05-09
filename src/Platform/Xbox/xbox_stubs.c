/*
 * xbox_stubs.c - Stub implementations for unported game modules
 * OpenJKDF2 Original Xbox Port
 *
 * ONLY stubs functions that genuinely can't link yet (GUI, display,
 * video, renderer helpers, player systems).  Real engine modules
 * (stdGob, jkRes, sithMain, etc.) should link from their actual
 * source files — do NOT stub them here.
 *
 * CRT helpers and MSVC runtime shims are also provided here.
 */

#include "platform_xbox.h"
#include "xbox_debug.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>

/* Forward-declare HostServices so we can define std_pHS */
struct HostServices;

/* ================================================================
   C++ GLOBALS — referenced from C++ translation units
   ================================================================ */
/* pHS is in the extern "C" block below */
/* std_genBuffer, stdMemory_*, jkHud_bOpened — now in globals.c */

/* embeddedResource stubs */
struct embeddedResource_t { const char* name; const void* data; unsigned int size; };
const embeddedResource_t* const embeddedResource_aFiles = 0;
const unsigned int embeddedResource_aFiles_num = 0;

/* ================================================================
   Everything below uses C linkage so names match what C callers expect.
   The C++ globals above intentionally use C++ mangling.
   ================================================================ */
/* All globals and stubs — C++ linkage to match callers */
HostServices* std_pHS = 0;
HostServices* pHS = 0;

/* ================================================================
   MSVC RUNTIME HELPERS - not in XDK CRT, need manual provision
   ================================================================ */

/* _fltused is provided by libc.lib -- do not redefine here */

long __cdecl _ftol2_sse(double d) { return (long)d; }
long __cdecl _ftol2(double d)     { return (long)d; }

__int64 __cdecl _allmul(__int64 a, __int64 b) { return a * b; }

/* ================================================================
   DATA SYMBOLS (globals from excluded source files)
   ================================================================ */
/* C++ mangled globals */
int   jkGuiRend_thing_four    = 0;
int   jkGuiRend_thing_five    = 0;
int   jkPlayer_fpslimit       = 60;
int   jkPlayer_personality    = 0;
/* g_app_suspended — in globals.c */
int   Window_xSize            = 640;
int   Window_ySize            = 480;
struct rdCanvas;
struct stdVBuffer;
rdCanvas*   Video_pCanvasOverlayMap = 0;
stdVBuffer* Video_pOverlayMapBuffer = 0;

/* stdDisplay_pCurVideoMode initialized in main_xbox.c before Main_Startup */

/* ================================================================
   GAME MODULE STUBS — only things that genuinely can't link yet
   ================================================================ */
#define STUB0  { return 0; }
#define STUBV  { }
#define STUBP  { return 0; }

/* jkPlayer — needs full game systems */
int  jkPlayer_Startup(void) STUB0
int  jkPlayer_Shutdown(void) STUB0
int  jkPlayer_Close(void) STUB0
void jkPlayer_InitForceBins(void) STUBV
void jkPlayer_InitThings(void) STUBV
void jkPlayer_InitSaber(void) STUBV
void jkPlayer_MpcInitBins(void) STUBV
void jkPlayer_SetAmmoMaximums(void) STUBV
void jkPlayer_nullsub_1(void) STUBV
void jkPlayer_idkEndLevel(void) STUBV
void jkPlayer_ResetVars(void) STUBV
void jkPlayer_WriteConfSwap(void) STUBV
int  jkPlayer_GetChoice(void) STUB0

/* jkGui — needs display system */
int  jkGui_Startup(void) STUB0
int  jkGui_Shutdown(void) STUB0
void jkGui_SetModeMenu(void) STUBV
void jkGui_SetModeGame(void) STUBV
void* jkGui_copies_string(void) STUBP

/* jkGuiMain — needs display */
int  jkGuiMain_Startup(void) STUB0
int  jkGuiMain_Shutdown(void) STUB0
void jkGuiMain_ShowCutscenes(void) STUBV

/*
 * jkGuiMain_Show — stub: skip menu, auto-start first level.
 * jkMain_SwitchTo5 sets the level name and transitions to
 * state 5 (gameplay), which calls sithMain_Mode1Init.
 * Remove this once real menu GUI is implemented.
 */
extern int jkMain_SwitchTo5(char *pJklFname);
extern int jkSmack_gameMode;
extern void jkRes_LoadGob(char *a1);
extern "C" void xbox_debug_Print(const char*);
void jkGuiMain_Show(void)
{
    static int autostarted = 0;
    xbox_debug_Print("jkGuiMain_Show called\n");
    if (!autostarted) {
        autostarted = 1;
        xbox_debug_Print("jkGuiMain_Show: loading episode GOB 'JK1'\n");
        jkRes_LoadGob("JK1");
        xbox_debug_Print("jkGuiMain_Show: auto-starting 01narshadda.jkl\n");
        jkMain_SwitchTo5("01narshadda.jkl");
        /* SwitchTo5 sets jkSmack_gameMode=3 which doesn't match any init
         * branch in jkMain_GameplayShow.  Override to 0 = fresh level load
         * so sithMain_Mode1Init gets called. */
        jkSmack_gameMode = 0;
        xbox_debug_Print("jkGuiMain_Show: SwitchTo5 done, gameMode forced to 0\n");
    }
}

/* jkGuiTitle — needs display */
int  jkGuiTitle_Startup(void) STUB0
int  jkGuiTitle_Shutdown(void) STUB0
void jkGuiTitle_ShowLoading(void) STUBV
void jkGuiTitle_ShowLoadingStatic(void) STUBV
void jkGuiTitle_LoadingFinalize(void) STUBV

/* jkGuiDialog — needs display */
int  jkGuiDialog_Startup(void) STUB0
int  jkGuiDialog_Shutdown(void) STUB0
int  jkGuiDialog_ErrorDialog(void* a, void* b) STUB0
int  jkGuiDialog_YesNoDialog(void* a, void* b) STUB0
int  jkGuiDialog_OkCancelDialog(void* a, void* b) STUB0

/* jkGuiEsc — needs display */
int  jkGuiEsc_Startup(void) STUB0
int  jkGuiEsc_Shutdown(void) STUB0
void jkGuiEsc_Show(int a, int b) STUBV

/* jkGuiRend — needs display */
int  jkGuiRend_Startup(void) STUB0
int  jkGuiRend_Shutdown(void) STUB0

/* jkGuiForce — needs display */
int  jkGuiForce_Startup(void) STUB0
int  jkGuiForce_Shutdown(void) STUB0
void jkGuiForce_Show(int a, int b) STUBV

/* jkGuiSingleTally — needs display */
int  jkGuiSingleTally_Startup(void) STUB0
int  jkGuiSingleTally_Shutdown(void) STUB0
void jkGuiSingleTally_Show(int a, int b) STUBV

/* jkGuiMultiTally — needs display */
void jkGuiMultiTally_Show(int a, int b) STUBV

/* Remaining jkGui* Startup/Shutdown pairs — all need display */
int  jkGuiMap_Startup(void) STUB0
int  jkGuiMap_Shutdown(void) STUB0
int  jkGuiSound_Startup(void) STUB0
int  jkGuiSound_Shutdown(void) STUB0
int  jkGuiDisplay_Startup(void) STUB0
int  jkGuiDisplay_Shutdown(void) STUB0
int  jkGuiSetup_Startup(void) STUB0
int  jkGuiSetup_Shutdown(void) STUB0
int  jkGuiMouse_Startup(void) STUB0
int  jkGuiMouse_Shutdown(void) STUB0
int  jkGuiKeyboard_Startup(void) STUB0
int  jkGuiKeyboard_Shutdown(void) STUB0
int  jkGuiJoystick_Startup(void) STUB0
int  jkGuiJoystick_Shutdown(void) STUB0
int  jkGuiPlayer_Startup(void) STUB0
int  jkGuiPlayer_Shutdown(void) STUB0
int  jkGuiGeneral_Startup(void) STUB0
int  jkGuiGeneral_Shutdown(void) STUB0
int  jkGuiGameplay_Startup(void) STUB0
int  jkGuiGameplay_Shutdown(void) STUB0
int  jkGuiDecision_Startup(void) STUB0
int  jkGuiDecision_Shutdown(void) STUB0
int  jkGuiObjectives_Startup(void) STUB0
int  jkGuiObjectives_Shutdown(void) STUB0
int  jkGuiSingleplayer_Startup(void) STUB0
int  jkGuiSingleplayer_Shutdown(void) STUB0
int  jkGuiSaveLoad_Startup(void) STUB0
int  jkGuiSaveLoad_Shutdown(void) STUB0
int  jkGuiControlSaveLoad_Startup(void) STUB0
int  jkGuiControlSaveLoad_Shutdown(void) STUB0
int  jkGuiControlOptions_Startup(void) STUB0
int  jkGuiControlOptions_Shutdown(void) STUB0
int  jkGuiMods_Startup(void) STUB0
int  jkGuiMods_Shutdown(void) STUB0

/* jkHud — C++ mangled versions at end of file.
   C callers (jkMain.c) use implicit declaration which resolves
   to the C++ version via the linker's weak symbol resolution. */
void jkHudScope_Open(void) STUBV
void jkHudScope_Close(void) STUBV
void jkHudCameraView_Open(void) STUBV
void jkHudCameraView_Close(void) STUBV

/* jkHudInv — needs display */
int  jkHudInv_Startup(void) STUB0
int  jkHudInv_Shutdown(void) STUB0
void jkHudInv_Close(void) STUBV
void jkHudInv_InitItems(void) STUBV
void jkHudInv_LoadItemRes(void) STUBV

/* jkDev — needs display/debug overlay */
int  jkDev_Startup(void) STUB0
int  jkDev_Shutdown(void) STUB0
void jkDev_Open(void) STUBV
void jkDev_Close(void) STUBV

/* jkSmack — needs video playback */
int  jkSmack_Startup(void) STUB0
int  jkSmack_Shutdown(void) STUB0
int  jkSmack_SmackPlay(void* a, void* b, int c) STUB0

/* jkCredits — needs display */
int  jkCredits_Startup(const char* a) { (void)a; return 0; }
int  jkCredits_Shutdown(void) STUB0
void jkCredits_Show(int a, int b) STUBV
void jkCredits_Tick(int a) STUBV
void jkCredits_Skip(int a, int b) STUBV

/* jkCutscene — needs video */
int  jkCutscene_Startup(const char* a) { (void)a; return 0; }
int  jkCutscene_Shutdown(void) STUB0
void jkCutscene_PauseShow(int a, int b) STUBV
void jkCutscene_sub_421310(int a, int b) STUBV
void jkCutscene_sub_421410(int a, int b) STUBV
void jkCutscene_smack_related_loops(int a) STUBV

/* jkDSS — needs networking */
int  jkDSS_Startup(void) STUB0
int  jkDSS_Shutdown(void) STUB0
void jkDSS_SendEndLevel(void) STUBV
void jkDSS_wrap_SendSaberInfo_alt(void) STUBV

/* jkControl — needs input integration */
int  jkControl_Startup(void) STUB0
int  jkControl_Shutdown(void) STUB0

/* jkGui* — multiplayer/network GUI stubs */
void jkGuiBuildMulti_StartupEditCharacter(void) STUBV
int  jkGuiBuildMulti_Startup(void) STUB0
int  jkGuiBuildMulti_Shutdown(void) STUB0
void jkGuiMultiTally_Startup(void) STUBV
void jkGuiMultiplayer_Startup(void) STUBV
void jkGuiMultiplayer_Shutdown(void) STUBV
void jkGuiNetHost_Startup(void) STUBV
void jkGuiNetHost_Shutdown(void) STUBV

/* smack — no Xbox implementation */
void smack_Shutdown(void) STUBV

/* Additional stubs needed by Main.c */
void jkGuiNetHost_LoadSettings(void) STUBV
void jkGuiNetHost_SaveSettings(void) STUBV
int  sithMulti_CreatePlayer(void* a) STUB0
void sithTime_idk_record(void) STUBV
int  jkPlayer_CreateConf(void* a) STUB0
/* stdString_CharToWchar / stdString_SafeWStrCopy — now in stdString.c */

/* Additional stubs needed by jkMain.c */
void jkGuiMultiplayer_ShowSynchronizing(void) STUBV
int  jkPlayer_Open(void) STUB0
/* sithControl_Close/IsOpen — now in sithControl.c */
int  sithWorld_GetMemorySize(void) STUB0

/* ================================================================
   ENGINE MODULE STUBS — temporary until real .c files compile for Xbox.
   When enabling a real source file in the vcproj, remove its stub here.
   Marked [STUBBED] so they're easy to find and remove later.
   ================================================================ */

/* jkGame — minimal Xbox implementation (full jkGame.c needs stdPalEffects + HUD modules) */
/* Forward declarations — all defined in compiled rdroid.c / sithMain.c */
void rdAdvanceFrame(void);
void rdFinishFrame(void);
void sithMain_UpdateCamera(void);

int  jkGame_Startup(void) STUB0
int  jkGame_Shutdown(void) STUB0

/* Render-state diagnostics — plain C++ extern (globals.c also C++, mangling matches) */
extern int sithRender_geoMode;
extern int rdCache_numProcFaces;

int jkGame_Update(void)
{
    /* Minimal render path:
     *   rdAdvanceFrame   — clears frame counters, advances rdCamera/rdCache state
     *   sithMain_UpdateCamera → sithCamera_SetRdCameraAndRenderidk → sithRender_Draw
     *                      — fills rdCache with level geometry (sectors, things)
     *   rdFinishFrame    — rdCache_Flush: submits DrawIndexedPrimitive calls to D3D
     */
    rdAdvanceFrame();
    sithMain_UpdateCamera();

#ifdef TARGET_XBOX
    /* One-shot: log geoMode + faces filled by sithRender_Draw (before rdCache_Flush resets) */
    { static int _jgu = 0; if (_jgu < 3) {
        XDBGF("jkGame_Update: geoMode=%d faces=%d\n",
              sithRender_geoMode, rdCache_numProcFaces);
        _jgu++;
    }}
#endif

    rdFinishFrame();
    return 1;
}

/* [STUBBED] jkCog / jkAI / jkEpisode — real code: src/Cog/jkCog.c, src/Main/jkAI.c, src/Main/jkEpisode.c */
int  jkCog_Startup(void) STUB0
int  jkCog_Shutdown(void) STUB0
int  jkAI_Startup(void) STUB0
int  jkEpisode_Startup(void) STUB0
void jkEpisode_Load(void* a) STUBV
void jkEpisode_EndLevel(void) STUBV
void jkEpisode_idk4(void) STUBV
void* jkEpisode_GetCurrentEpisodeEntry(void) STUBP
void* jkEpisode_GetNextEntryInDecisionPath(void* a) STUBP

/* [STUBBED] jkStrings — real code: src/Main/jkStrings.c (not yet enabled) */
int   jkStrings_Startup(void) STUB0
int   jkStrings_Shutdown(void) STUB0
void* jkStrings_GetUniStringWithFallback(const char* k) STUBP
/* jkGob — now in src/Main/jkGob.c (Main filter) */

/* [STUBBED] jkQuakeConsole */
int   jkQuakeConsole_Startup(void) STUB0
int   jkQuakeConsole_Shutdown(void) STUB0

/* sithMain — now compiled from src/Main/sithMain.c */

/* sithCog_StartupEnhanced — real implementation in sithCog.c */

/* sithCvar — now in src/Main/sithCvar.c (Main filter) */

/* rdStartup/rdShutdown — now compiled from src/Engine/rdroid.c */

/* Win32 GDI stubs excluded from jk.c by TARGET_XBOX guard */
void jk_UnmapViewOfFile(void* p) { (void)p; }
void jk_CloseHandle(void* h) { (void)h; }

/* stdGob — now in src/Win95/stdGob.c (Engine Core filter) */
/* stdString — now in src/General/stdString.c (Engine Core filter) */
/* stdStartup/stdShutdown/stdInitServices — now in src/Win95/std.c (Main filter) */
/* stdMath_Clamp — now in src/General/stdMath.c (Engine Core filter) */
/* InstallHelper_SetCwd — now in src/Main/InstallHelper.c (Main filter) */
/* jk_exit / jk_snwprintf — now in src/jk.c (Engine Core filter) */

/* sithCamera, sithRender — now compiled from real source */

/* sithInventory / sithPlayer — needs full game state */
void sithInventory_ClearInventory(void* a) STUBV
void sithInventory_SetCarries(void* a, int b) STUBV
int  sithPlayer_GetBinAmt(void* a, int b) STUB0
void sithPlayer_SetBinAmt(void* a, int b, int c) STUBV

/* sithGamesave — needs save system */
int  sithGamesave_Load(void* a) STUB0

/* sithTime — needs game loop */
void sithTime_Pause(void) STUBV
void sithTime_Resume(void) STUBV

/* sithMulti — needs networking */
int  sithMulti_Shutdown(void) STUB0
void sithMulti_LobbyMessage(void* a) STUBV

/* sithSoundMixer — now compiled from src/Devices/sithSoundMixer.c */

/* sithControl — now compiled from src/Devices/sithControl.c */

/* rd* — now compiled from real source */

/* stdDisplay — needs display */
int   stdDisplay_DrawAndFlipGdi(void) STUB0
int   stdDisplay_SetCooperativeLevel(void* a, int b) STUB0
int   stdDisplay_SetMode(void* a) STUB0
void* stdDisplay_GetPalette(void) STUBP

/* stdConsole — needs display */
int   stdConsole_Startup(void) STUB0
int   stdConsole_Shutdown(void) STUB0

/* stdBitmap — needs renderer */
int   stdBitmap_EnsureData(void* a) STUB0

/* stdPalEffects — needs display */
void  stdPalEffects_RefreshPalette(void* a) STUBV

/* stdHttp — needs networking (PLATFORM_NOSOCKETS) */
int   stdHttp_Startup(void) STUB0
int   stdHttp_Shutdown(void) STUB0

/* Video — needs display */
int   Video_Startup(void) STUB0
int   Video_Shutdown(void) STUB0
void  Video_SwitchToGDI(void) STUBV

/* Windows — needs display (Win95 Window module) */
int   Windows_Startup(void) STUB0
int   Windows_Shutdown(void) STUB0
void  Windows_ShutdownGdi(void) STUBV
int   Windows_InitWindow(void) { return 1; } /* Must return 1 so Main_Startup continues */
void  Windows_GdiHandler(void) STUBV
void  Windows_ErrorMsgboxWide(void* a, void* b) STUBV

/* DirectPlay — needs networking */
void  DirectPlay_SetSessionFlagidk(int a) STUBV
void  DirectPlay_SetSessionDesc(void* a) STUBV

/* ================================================================
   PHASE 1 ADDITIONS — Window message handler stubs
   ================================================================ */

int Window_AddMsgHandler(void* handler)     { (void)handler; return 1; }
int Window_RemoveMsgHandler(void* handler)  { (void)handler; return 1; }
void Window_SetDrawHandlers(void *pfnDraw, void *pfnDrawEnd) { (void)pfnDraw; (void)pfnDrawEnd; }

/* ================================================================
   ADDITIONAL STUBS for link resolution (C++ mangled names needed)
   ================================================================ */

/* stdConsole_Puts — C++ version at end of file (std.c is C++) */

/* InstallHelper — referenced from Main.c (C) but declared extern in C++ */
void InstallHelper_SetCwd(void) { /* Xbox has no CWD concept */ }

/* stdControl — referenced from jkMain.c */
void stdControl_ToggleCursor(void) { }
void stdControl_ShowCursor(void) { }
/* stdControl_Flush — now in stdControl_xbox.c */

/* jkHud — C++ mangled versions (also have C versions above) */
/* The C versions above provide _jkHud_Open etc. These are already stubbed. */

/* jkPlayer_StartupVars — C++ version at end of file */

/* stdJSON — referenced from sithCvar (C++) */
void stdJSON_SetString(void* a, const char* b, const char* c) { (void)a; (void)b; (void)c; }
void stdJSON_SaveFloat(void* a, const char* b, float c) { (void)a; (void)b; (void)c; }
void stdJSON_SaveInt(void* a, const char* b, int c) { (void)a; (void)b; (void)c; }
void stdJSON_SaveBool(void* a, const char* b, int c) { (void)a; (void)b; (void)c; }
const char* stdJSON_GetString(void* a, const char* b) { (void)a; (void)b; return ""; }
float stdJSON_GetFloat(void* a, const char* b) { (void)a; (void)b; return 0.0f; }
int  stdJSON_GetInt(void* a, const char* b) { (void)a; (void)b; return 0; }
int  stdJSON_GetBool(void* a, const char* b) { (void)a; (void)b; return 0; }
void stdJSON_EraseAll(void) { }

/* stdUpdater — referenced from sithCvar (C++) */
void stdUpdater_StartupCvars(void) { }

/* util_FileExists — now in stdFile_xbox.c */

/* stdGob — referenced from jkRes.c, jkGob.c (C linkage) */
/* These are already provided by stdGob.c in the C++ list, but they
   emit C++ mangled names. The C files expect _stdGob_Startup etc.
   We need extern "C" wrappers or provide them here. */

/* stdFileUtil — real implementation in stdFile_xbox.c */

/* stdMath_Clamp — now in stdMath.c */

/* jkMain_GuiAdvance — C++ version at end of file */

/* Main_Startup — referenced from main_xbox.c (C++) */
/* The real Main_Startup is in Main.c compiled as C, but main_xbox.c
   includes it as C++. The C version exports _Main_Startup, but the
   C++ caller expects ?Main_Startup@@... We need an extern "C" import
   or a C++ wrapper. Since main_xbox.c is C++, it needs the declaration
   to be extern "C". This is better handled in main_xbox.c itself. */

/* NtCreateFile/NtClose — Xbox kernel functions.
   stdFile_xbox.c includes <xtl.h> as C++ but declares NtCreateFile
   with C++ linkage. xboxkrnl.lib exports C names. Need extern "C". */

/* C++ linkage stubs — match caller expectations */
int  jkHud_Open(void) { return 0; }
void jkHud_Close(void) { }
void jkPlayer_StartupVars(void) { }
void stdConsole_Puts(char* msg, unsigned short flags) { (void)flags; OutputDebugStringA(msg ? msg : "(null)"); }

/* These were functions, not variables */
struct sithThing;
void jkPlayer_renderSaberWeaponMesh(sithThing* a) { (void)a; }
void jkGame_ddraw_idk_palettes(void) { }

/* sithPuppet_PlayMode with callback */
int  sithPuppet_PlayMode(sithThing* thing, int mode, void (*callback)(sithThing*, int, unsigned int))
     { (void)thing; (void)mode; (void)callback; return 0; }

/* Window functions — match WindowHandler_t signatures */
typedef int (*WindowHandler_t_local)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
int Window_AddMsgHandler(WindowHandler_t_local h) { (void)h; return 1; }
int Window_RemoveMsgHandler(WindowHandler_t_local h) { (void)h; return 1; }
void Window_SetDrawHandlers(int (*a)(unsigned int), int (*b)(unsigned int)) { (void)a; (void)b; }
int  Windows_GdiHandler(HWND a, UINT b, WPARAM c, HWND d, LRESULT* e) { (void)a;(void)b;(void)c;(void)d;(void)e; return 0; }
int  Windows_ErrorMsgboxWide(const char* fmt, ...) { (void)fmt; return 0; }
