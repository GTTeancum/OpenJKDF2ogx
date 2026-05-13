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
/* jkGuiRend_thing_four/five are provided by the real GUI renderer. */
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

/* Real menu core is compiled for Xbox. */
#if 0
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
extern int jkHudInv_InitItems(void);
extern int sithMain_Load(char *jklFname);
extern float jkPlayer_hudScale;
extern "C" void xbox_debug_Print(const char*);
extern "C" void xbox_debug_Printf(const char*, ...);
void jkGuiMain_Show(void)
{
    static int autostarted = 0;
    xbox_debug_Print("jkGuiMain_Show called\n");
    if (!autostarted) {
        int items_ok;
        int static_ok;
        autostarted = 1;
        /* HUD scale: PC SDL2 default is 2.0 (for hi-DPI desktop windows).
         * Xbox renders at 640x480 native and our UI ortho matches,
         * so the HUD bitmaps are 1:1. */
        jkPlayer_hudScale = 1.0f;
        xbox_debug_Print("jkGuiMain_Show: loading episode GOB 'JK1'\n");
        jkRes_LoadGob("JK1");

        /* Mirror jkMain_TitleShow's order (src/Main/jkMain.c:862):
         *   sithMain_Load("static.jkl");  // registers shared cogs into sithWorld_pStatic
         *   jkHudInv_InitItems();          // parses items.dat, calls
         *                                  // sithCog_LoadCogscript(name) per bin
         *
         * items.dat lines like `bin 2 bryar.cog 0 1 0x10` reference cog
         * scripts by name.  Without static.jkl loaded first, the
         * per-bin LoadCogscript returns NULL, every descriptor gets
         * cog=0, and AutoSelectWeapon's `if (desc->cog)` gate skips
         * every weapon — sithCog_SendMessageEx is never sent so
         * priority stays at -1 forever.  Result: bryar in inventory
         * but no equip on level start.
         *
         * Re-ordering is what the title-screen path does upstream;
         * this just makes the same call chain happen on Xbox without
         * the title screen. */
        static_ok = sithMain_Load("static.jkl");
        xbox_debug_Printf("jkGuiMain_Show: sithMain_Load(static.jkl) -> %d\n", static_ok);

        items_ok = jkHudInv_InitItems();
        xbox_debug_Printf("jkGuiMain_Show: jkHudInv_InitItems -> %d\n", items_ok);

    xbox_debug_Print("jkGuiMain_Show: auto-starting 01narshaddaa.jkl\n");
    jkMain_SwitchTo5("01narshaddaa.jkl");
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
#endif

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
/* jkGuiObjectives/jkGuiSingleplayer/jkGuiSaveLoad are real. */
int  jkGuiControlSaveLoad_Startup(void) STUB0
int  jkGuiControlSaveLoad_Shutdown(void) STUB0
int  jkGuiControlOptions_Startup(void) STUB0
int  jkGuiControlOptions_Shutdown(void) STUB0
int  jkGuiMods_Startup(void) STUB0
int  jkGuiMods_Shutdown(void) STUB0

/* jkHud — C++ mangled versions at end of file.
   C callers (jkMain.c) use implicit declaration which resolves
   to the C++ version via the linker's weak symbol resolution. */

/* jkHudInv — needs display */

/* jkDev — needs display/debug overlay */
int  jkDev_Startup(void) STUB0
int  jkDev_Shutdown(void) STUB0
void jkDev_Open(void) STUBV
void jkDev_Close(void) STUBV

/* jkSmack is compiled for Xbox. */

/* jkCredits — needs display */
int  jkCredits_Startup(const char* a) { (void)a; return 0; }
int  jkCredits_Shutdown(void) STUB0
void jkCredits_Show(int a, int b) STUBV
void jkCredits_Tick(int a) STUBV
void jkCredits_Skip(int a, int b) STUBV

/* jkCutscene is compiled for Xbox. */

/* jkDSS — needs networking */
int  jkDSS_Startup(void) STUB0
int  jkDSS_Shutdown(void) STUB0
void jkDSS_SendEndLevel(void) STUBV
void jkDSS_wrap_SendSaberInfo_alt(void) STUBV

/* jkControl_Startup, jkControl_Shutdown — real impls in src/Main/jkControl.c (now in build) */

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
/* stdString_CharToWchar / stdString_SafeWStrCopy — now in stdString.c */

/* Additional stubs needed by jkMain.c */
void jkGuiMultiplayer_ShowSynchronizing(void) STUBV
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


/* jkGame_Update body — orphan from removed stub; full impl now in src/Main/jkGame.c */

/* jkCog_Startup, jkCog_Shutdown — real impls in src/Cog/jkCog.c (now in build) */

/* [STUBBED] jkStrings — real code: src/Main/jkStrings.c (not yet enabled) */
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

/* sithGamesave — auto-stub in xbox_autostubs.c. Soft-respawn hijack
 * reverted: it broke the boot path. See XBOX_HACKS.md. */

/* sithTime_Pause, sithTime_Resume — real impls in src/Gameplay/sithTime.c (now in build) */

/* sithMulti — needs networking */
int  sithMulti_Shutdown(void) STUB0
void sithMulti_LobbyMessage(void* a) STUBV

/* sithSoundMixer — now compiled from src/Devices/sithSoundMixer.c */

/* sithControl — now compiled from src/Devices/sithControl.c */

/* rd* — now compiled from real source */

/* stdDisplay is provided by stdDisplay_xbox.c. */

/* stdConsole — needs display */
int   stdConsole_Startup(void) STUB0
int   stdConsole_Shutdown(void) STUB0

/* stdBitmap — needs renderer */
int   stdBitmap_EnsureData(void* a) STUB0

/* stdPalEffects_RefreshPalette — real impl in src/General/stdPalEffects.c (now in build) */

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

/* Window_AddMsgHandler / Window_RemoveMsgHandler / Window_SetDrawHandlers —
 * defined further down with proper WindowHandler_t-shaped signature. */

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
void stdConsole_Puts(char* msg, unsigned short flags) { (void)flags; OutputDebugStringA(msg ? msg : "(null)"); }

/* These were functions, not variables */
struct sithThing;

/* sithPuppet_PlayMode — real impl in src/Engine/sithPuppet.c (now in build) */

/* Window functions — enough of the Win95 message loop for jkGuiRend menus. */
typedef int (*WindowHandler_t_local)(HWND, UINT, WPARAM, LPARAM, LRESULT*);
typedef int (*WindowDrawHandler_t_local)(unsigned int);
static WindowHandler_t_local xbox_windowHandler = 0;
static WindowDrawHandler_t_local xbox_drawAndFlip = 0;
static WindowDrawHandler_t_local xbox_setCooperativeLevel = 0;
void jkGuiRend_UpdateController(void);
void jkMain_GuiAdvance(void);
int stdDisplay_DDrawGdiSurfaceFlip(void);
int Window_AddMsgHandler(WindowHandler_t_local h) { xbox_windowHandler = h; return 1; }
int Window_RemoveMsgHandler(WindowHandler_t_local h) { if (xbox_windowHandler == h) xbox_windowHandler = 0; return 1; }
void Window_SetDrawHandlers(WindowDrawHandler_t_local a, WindowDrawHandler_t_local b) { xbox_drawAndFlip = a; xbox_setCooperativeLevel = b; }
void Window_GetDrawHandlers(WindowDrawHandler_t_local *a, WindowDrawHandler_t_local *b)
{
    if (a) *a = xbox_drawAndFlip;
    if (b) *b = xbox_setCooperativeLevel;
}
int Window_MessageLoop(void)
{
    LRESULT unused = 0;
    jkGuiRend_UpdateController();
    jkMain_GuiAdvance();
    if (xbox_windowHandler)
        xbox_windowHandler(0, 0x000F, 0, 0, &unused); /* WM_PAINT */
    else
        stdDisplay_DDrawGdiSurfaceFlip();
    return 0;
}
int Window_ShowCursorUnwindowed(int a) { (void)a; return 1; }
int  Windows_GdiHandler(HWND a, UINT b, WPARAM c, HWND d, LRESULT* e) { (void)a;(void)b;(void)c;(void)d;(void)e; return 0; }
int  Windows_ErrorMsgboxWide(const char* fmt, ...) { (void)fmt; return 0; }

/* stdControl_MessageHandler — Xbox has no Win32 message system, so this
 * always returns 0 (don't filter messages).  The real impl in
 * src/Platform/Common/stdControl.c is a Win32 screensaver-suppression
 * filter; not relevant on this platform. */
int stdControl_MessageHandler(HWND hWnd, UINT Msg, WPARAM wParam, LPARAM lParam, LRESULT* unused)
{ (void)hWnd; (void)Msg; (void)wParam; (void)lParam; (void)unused; return 0; }

/* Input handler chain stubs — registered by jkControl_Startup, called per
 * frame.  Handlers return 0 = "didn't consume input, try next handler".
 * Returning 0 for systems we haven't wired (weapons, inventory) lets
 * sithControl_HandlePlayer (the movement handler) run normally.  Same
 * pattern any platform follows when a subsystem isn't enabled. */
/* sithWeapon_HandleWeaponKeys — real impl in src/Gameplay/sithWeapon.c (now in build) */
/* sithInventory_HandleInvSkillKeys — real impl in src/Gameplay/sithInventory.c (now in build) */

/* Functions called from jkControl_HandleHudKeys body for various
 * F-key/dev shortcuts.  Stubbed because the corresponding subsystems
 * (HUD popups, screenshot, gamma config, save UI) aren't wired up
 * on Xbox yet.  Pressing those keys does nothing rather than crashing. */
int  jkDev_PrintUniString(const unsigned short *s)      { (void)s; return 0; }
/* jkGuiTitle_quicksave_related_func1 is provided by the real title GUI. */

/* ================================================================
   Stubs for source files we don't pull (multiplayer/DSS/JSON/Quake-console)
   Signatures must match the headers exactly (mangled or C-linkage).
   ================================================================ */

struct sithThing;
struct sithSound;
struct rdVector3;
struct sithEventInfo;
struct rdMaterial;
struct rdParticle;
struct rdCanvas;

/* C-linkage stubs (headers wrapped in extern "C") */
extern "C" {
    int  sithMulti_ServerLeft(int a, struct sithEventInfo* b)               { (void)a; (void)b; return 0; }
    unsigned int sithMulti_IterPlayersnothingidk(int net_id)                { (void)net_id; return 0; }
    int  sithMulti_SendPing(int sendtoId)                                   { (void)sendtoId; return 0; }
    void sithMulti_SendQuit(int idx)                                        { (void)idx; }
    void DirectPlay_EnumPlayers(int a)                                      { (void)a; }
    int  stdJSON_IterateKeys(const char* p, void(*cb)(const char*,const char*,void*), void* ctx) { (void)p; (void)cb; (void)ctx; return 0; }
    int  stdFileUtil_DelFile(char* p)                                       { (void)p; return 1; }
}

/* C++-mangled stubs (headers without extern "C") */
void sithDSSThing_SendTakeItem(struct sithThing* pItem, struct sithThing* pActor, int mpFlags)
    { (void)pItem; (void)pActor; (void)mpFlags; }
void sithDSSThing_SendFireProjectile(struct sithThing* a, struct sithThing* b, struct rdVector3* c,
    struct rdVector3* d, struct sithSound* e, short f, float g, short h, float i, int j, int k, int l, int m)
    { (void)a; (void)b; (void)c; (void)d; (void)e; (void)f; (void)g; (void)h; (void)i; (void)j; (void)k; (void)l; (void)m; }
int  rdPrimit2_DrawClippedLine(struct rdCanvas* c, int x1, int y1, int x2, int y2, unsigned short col, int mask)
    { (void)c; (void)x1; (void)y1; (void)x2; (void)y2; (void)col; (void)mask; return 0; }
void jkQuakeConsole_ExecuteCommand(const char* pCmd)                        { (void)pCmd; }
void jkGuiSetup_Show(void)                                                  { }
int  jkGuiMap_Show(void)                                                    { return 0; }
void jkGuiMods_Show(void)                                                   { }
void jkGuiPlayer_ShowNewPlayer(int a1)                                      { (void)a1; }
int  jkGuiMultiplayer_Show(void)                                            { return -1; }
int  jkGuiMultiplayer_Show2(void)                                           { return 0; }
int  jkCredits_cdOverride                                                   = 0;
extern "C" const wchar_t *openjkdf2_waReleaseVersion                        = L"Xbox";
extern "C" const wchar_t *openjkdf2_waReleaseCommitShort                    = L"";
int  sithGamesave_GetProfilePath(char *out, int outSize, char *name) {
    if (!out || outSize <= 0) return 0;
    if (!name) name = "save";
    _snprintf(out, (size_t)outSize, "D:\\saves\\%s", name);
    out[outSize - 1] = 0;
    return 1;
}

/* rdParticle_* — real impls now in src/Primitives/rdParticle.c */

/* MSVC CRT helper: __isspace (single underscore _isspace exists in libc; the
   double-underscore mangling is what stdStrTable.c references via macros.) */
extern "C" int __isspace(int c) { return (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\v' || c == '\f'); }

/* Helper from upstream src/jk.c (we don't pull jk.c, only need this one fn). */
extern "C" int _string_modify_idk(int c) { return (c >= 'a' && c <= 'z') ? c - ('a' - 'A') : c; }

/* ================================================================
   Stubs for jkPlayer/jkHud/jkEpisode/jkSaber/jkGame dependencies that
   live in modules we don't pull (jkDev, jkSmack, jkQuakeConsole,
   stdDisplay/DDraw, stdFont, stdBitmap, rdPolyLine, etc.).
   ================================================================ */

struct stdVBuffer;
struct rdRect;
struct rdColor24;
struct rdTexFormat;
struct stdFont;
struct stdBitmap;
struct rdPolyLine;
struct sithThing;

/* Data symbols */
int stdMci_bIsGOG = 0;
int Window_isFullscreen = 1;
extern "C" int Window_isHiDpi = 0;
/* stdDisplay_masterPalette is provided by stdDisplay_xbox.c. */

/* Windows error dialog — no msgbox on Xbox, just no-op */
void Windows_GameErrorMsgbox(const char* fmt, ...) { (void)fmt; }

/* Window controls — Xbox is always fullscreen 640x480 */
void Window_SetFullscreen(int a) { (void)a; }
void Window_SetHiDpi(int a)      { (void)a; }

/* stdDisplay — DirectDraw path, not used on Xbox (we use std3D/D3D8 directly) */
/* stdDisplay VBuffer/present helpers are provided by stdDisplay_xbox.c. */

/* stdColor, stdFont, stdBitmap — real impls in src/General/std{Color,Font,Bitmap}.c (now in build) */

/* std3D_AddBitmapToTextureCache + std3D_PurgeBitmapRefs are implemented
 * in src/Platform/Xbox/std3D.c (uploads stdBitmap mips as FakeGL
 * textures, stored in pBmp->aTextureIds for HUD blits). */

/* stdDisplay_VBufferConvertColorFormat — DDraw colour conversion path,
   not used on Xbox (we render to D3D8 surfaces, not DDraw VBuffers). */
/* stdDisplay_VBufferConvertColorFormat is provided by stdDisplay_xbox.c. */

/* __wcsrchr — wide-char strrchr.  XDK CRT does NOT export this symbol,
   so we provide a minimal impl. */
extern "C" wchar_t* __wcsrchr(const wchar_t* s, wchar_t c) {
    const wchar_t* last = 0;
    if (!s) return 0;
    for (; *s; s++) if (*s == c) last = s;
    if (c == 0) return (wchar_t*)s;  /* match terminator like libc wcsrchr */
    return (wchar_t*)last;
}

/* rdPolyLine — real impl in src/Primitives/rdPolyLine.c (now in build).
 * Powers laser-bolt projectiles (bryar etc.) and saber trails. */

/* std3D_DrawUIBitmap + std3D_DrawUIClearedRect are implemented in
 * src/Platform/Xbox/std3D.c (immediate-mode textured/solid quads via
 * the FakeGL adapter). std3D_Screenshot kept as a stub. */
extern "C" {
    int  std3D_Screenshot(void) { return 0; }
}

/* stdFileUtil_MkDir — directory creation (config save). C linkage. */
extern "C" int stdFileUtil_MkDir(const char* p) { (void)p; return 1; }

/* jkDev — dev tools / debug overlay; stubbed (no dev console on Xbox yet) */
void jkDev_BlitLogToScreen(void)                                { }
void jkDev_DrawLog(void)                                        { }
int  jkDev_sub_41FC40(int a, const char* b)                     { (void)a; (void)b; return 0; }
int  jkDev_sub_41FB80(int a, const unsigned short* b)           { (void)a; (void)b; return 0; }
void jkDev_sub_41FC90(int a)                                    { (void)a; }
int  jkDev_TryCommand(const char* s)                            { (void)s; return 0; }
int  jkDev_DebugLog(const char* s)                              { (void)s; return 0; }

/* jkQuakeConsole_Render — stubbed (no quake console rendering on Xbox) */
void jkQuakeConsole_Render(void)                                { }

/* jkSmack — Smacker video; stubbed (Xbox lacks DirectShow Smacker codec) */
/* jkSmack_GetCurrentGuiState is compiled for Xbox. */

/* jkGuiTitle helper used by jkHud */
/* jkGuiTitle_sub_4189A0 is provided by the real title GUI. */
