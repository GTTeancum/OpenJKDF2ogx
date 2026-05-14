/* xbox_autostubs.c - Auto-generated stubs for unresolved symbols */
/* Generated from linker errors. All functions return 0/null. */

#include "platform_xbox.h"
#include "types.h"
#include "Primitives/rdSprite.h"
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <wchar.h>

/* sithTime_physicsRolloverFrames — defined in src/Gameplay/sithTime.c (now in build) */
/* rdMath_DistancePointToPlane, rdMath_clampf — now in rdMath.c */
/* sithActor_Hit — real impl in src/World/sithActor.c (now in build) */
int jkCredits_Show(void) { return 0; }
int jkCredits_Skip(void) { return 0; }
int jkCredits_Tick(void) { return 0; }
int jkDSS_wrap_SendSaberInfo_alt(void) { return 0; }
int jkDev_Open(void) { return 0; }
/* jkGuiDialog_YesNoDialog, jkGuiForce_Show — real GUI implementations now compiled. */
int jkGuiMultiTally_Show(int a0) { return 0; }
/* stdComm_EarlyInit is compiled from src/Win95/stdComm.c. */
/* jkGuiSingleTally_Show, jkGui_SetModeMenu — real GUI implementations now compiled. */
/* rdMath_PointsCollinear — now in rdMath.c */
/* rdThing_Draw, NewEntry, SetModel3, SetSprite3, SetParticleCloud — now in rdThing.c */
/* rdThing_Draw dispatches to these — rdSprite/rdParticle/rdPolyLine now compiled */
/* sithActor_ActorActorCollide, sithActor_LoadParams, sithActor_sub_4ED1D0 —
 * real impls in src/World/sithActor.c (now in build) */
/* sithComm is compiled from src/Devices/sithComm.c. */
int sithGamesave_Flush(void) { return 0; }
int sithGamesave_Load(char * a0, int a1, int a2) { return 0; }
int sithGamesave_Write(char * a0, int a1, int a2, unsigned short * a3) { return 0; }
/* stdBitmap_EnsureData — real impl in src/General/stdBitmap.c (now in build) */
/* stdConffile — now compiled from src\General\stdConffile.c */
int stdConsole_Startup(char const * a0, unsigned int a1, int a2) { return 0; }
/* stdDisplay_* — real Xbox software-display shim now compiled. */
#ifndef STDSOUND_XBOX
int stdMci_CheckStatus(void) { return 0; }
int stdMci_Play(unsigned char a0, unsigned char a1) { return 0; }
int stdMci_Startup(void) { return 0; }
#endif
/* sithCog_actionCogIdk — defined in src/Cog/sithCog.c (now in build) */
/* sithCog_pActionCog — defined in src/Cog/sithCog.c (now in build) */
/* stdDisplay_VBufferNew/GetPalette — real Xbox software-display shim now compiled. */
#ifndef STDSOUND_XBOX
unsigned int stdSound_ParseWav(int a0, unsigned int * a1, int * a2, int * a3, int * a4) { return 0; }
#endif
unsigned int util_Weirdchecksum(unsigned char * a0, int a1, unsigned int a2) { return 0; }
void Video_Shutdown(void) { }
void Windows_Shutdown(void) { }
void Windows_Startup(void) { }
/* jkCog_Shutdown — defined in src/Cog/jkCog.c (now in build) */
flex_t jkGuiSound_cutsceneVolume = 1.0f;
void jk_SetCursor(HCURSOR hCursor) { (void)hCursor; }
void jk_ShowCursor(int a) { (void)a; }
void jkCredits_Shutdown(void) { }
void jkCredits_Startup(char * a0) { }
void jkDSS_Shutdown(void) { }
void jkDev_Shutdown(void) { }
void jkDev_Startup(void) { }
void jkGuiControlOptions_Shutdown(void) { }
void jkGuiControlOptions_Startup(void) { }
void jkGuiControlSaveLoad_Shutdown(void) { }
void jkGuiControlSaveLoad_Startup(void) { }
void jkGuiDecision_Shutdown(void) { }
void jkGuiDecision_Startup(void) { }
/* jkGuiDialog_* — real GUI implementations now compiled. */
void jkGuiDisplay_Shutdown(void) { }
void jkGuiDisplay_Startup(void) { }
/* jkGuiEsc_*, jkGuiForce_* — real GUI implementations now compiled. */
void jkGuiGameplay_Shutdown(void) { }
void jkGuiGameplay_Startup(void) { }
void jkGuiGeneral_Shutdown(void) { }
void jkGuiGeneral_Startup(void) { }
void jkGuiJoystick_Shutdown(void) { }
void jkGuiJoystick_Startup(void) { }
void jkGuiKeyboard_Shutdown(void) { }
void jkGuiKeyboard_Startup(void) { }
/* jkGuiMain_* — real GUI implementations now compiled. */
void jkGuiMap_Shutdown(void) { }
void jkGuiMap_Startup(void) { }
void jkGuiMods_Shutdown(void) { }
void jkGuiMods_Startup(void) { }
void jkGuiMouse_Shutdown(void) { }
void jkGuiMouse_Startup(void) { }
/* jkGuiObjectives_* — real GUI implementations now compiled. */
void jkGuiPlayer_Shutdown(void) { }
/* jkGuiRend_*, jkGuiSaveLoad_* — real GUI implementations now compiled. */
void jkGuiSetup_Shutdown(void) { }
void jkGuiSetup_Startup(void) { }
/* jkGuiSingleTally_*, jkGuiSingleplayer_* — real GUI implementations now compiled. */
void jkGuiSound_Shutdown(void) { }
void jkGuiSound_Startup(void) { }
/* jkGuiTitle_*, jkGui_Shutdown/copies_string — real GUI implementations now compiled. */
void jkQuakeConsole_Shutdown(void) { }
void jkQuakeConsole_Startup(void) { }
/* jkCutscene_* and jkSmack_* are real implementations now. */
/* rdMath_CalcSurfaceNormal, ClampVector, ClampVectorRange — now in rdMath.c */
/* rdPrimit3_ClearFrameCounters/ClipFace/ClipFaceRGB/ClipFaceRGBLevel/NoClipFace/NoClipFaceRGB — now in rdPrimit3.c */
void rdPrimit2_DrawCircle(struct rdCanvas *a0, int a1, int a2, float a3, float a4, unsigned short a5, int a6) { }
void rdRaster_Startup(void) { }
/* rdThing_AccumulateMatrices — now in rdThing.c */
/* rdThing_FreeEntry — now in rdThing.c */
/* sithActor_MoveJointsForEyePYR, _Remove, _RemoveCorpse,
 * _SetMaxHeathForDifficulty, _Tick — real impls in src/World/sithActor.c
 * (now in build) */
/* sithDSSThing send helpers are compiled from src/Dss/sithDSSThing.c. */
/* sithDSS state-sync helpers are compiled from src/Dss/sithDSS.c. */
/* sithTime_Startup, sithTime_Tick — real impls in src/Gameplay/sithTime.c (now in build) */
/* std3D_EndScene provided by std3D.c */
/* stdConffile_Close — now compiled from src\General\stdConffile.c */
/* stdDisplay_VBufferFree/Unlock — real Xbox software-display shim now compiled. */
#ifndef STDSOUND_XBOX
void stdMci_SetVolume(float a0) { }
void stdMci_Shutdown(void) { }
void stdMci_Stop(void) { }
#endif
/* xbox_debug_Print/Printf — real implementations in xbox_debug.c */

/* =========================================================================
 * Level loading chain stubs — COG system, AI, sprites, events
 * These are safe to stub for "see the level and move around" testing.
 * ====================================================================== */

/* COG parser, execution, and DSS cog sync are real implementations now. */

/* Events */

/* AI */

/* Sprites — rdSprite_FreeEntry, rdSprite_NewEntry now in rdSprite.c */

/* All jkCog*, jkCogExt*, sithCogFunction*, sithCogParse*, sithCogExec*
 * verbs and helpers — real impls in src/Cog/*.c (now in build). */

/* ====================================================================== */
/* Phase 4 cog VM dependencies — game subsystems referenced by jkCog.c    */
/* and sithCogFunction*.c that aren't in the build yet (jkHud, jkSaber,   */
/* jkDSS, jkEpisode, sithInventory, sithWeapon, sithTrackThing, sithAI,   */
/* sithDSSThing, sithMulti, sithEvent, sithItem, sithPuppet, stdStrTable, */
/* etc.).  Stubbed as no-ops so the cog VM links and runs; cog scripts    */
/* that call into these subsystems do nothing rather than crash.          */
/*                                                                        */
/* When a subsystem is wired up later, remove its stubs here.             */
/* ====================================================================== */
#include "Primitives/rdMatrix.h"

/* CRT aliases not in crt_aliases.c — must be C-linked.  jkCog.c calls
 * `__wcscat` (two leading underscores in source) so we define both
 * single- and double-underscore variants. */
extern "C" {
wchar_t* __cdecl _wcscpy(wchar_t* d, const wchar_t* s)               { return wcscpy(d,s); }
wchar_t* __cdecl _wcscat(wchar_t* d, const wchar_t* s)               { return wcscat(d,s); }
wchar_t* __cdecl __wcscat(wchar_t* d, const wchar_t* s)              { return wcscat(d,s); }
int     __cdecl _fputs(const char* s, FILE* f)                       { return fputs(s,f); }
size_t  __cdecl _fwrite(const void* p, size_t a, size_t b, FILE* f)  { return fwrite(p,a,b,f); }
void*   __cdecl _malloc(size_t n)                                    { return malloc(n); }
void    __cdecl _free(void* p)                                       { free(p); }
}

/* Variables with C++ mangled references (?name@@3type) — keep as C++ linkage. */
/* jkStrings_tableExtOver — referenced as C symbol _jkStrings_tableExtOver */
extern "C" {
}
/* sithAIAlign defined in types.h:2737 — don't redefine */

/* jkPlayer */
struct jkPlayerInfo;

/* jkEpisode */

/* jkHud */

/* jkSaber */

/* jkDSS — multiplayer state-sync packets, no-op on Xbox single-player */
void  __cdecl jkDSS_SendJKEnableSaber(sithThing*)                     { }
void  __cdecl jkDSS_SendJKPrintUniString(int, unsigned int)           { }
void  __cdecl jkDSS_SendJKSetWeaponMesh(sithThing*)                   { }
void  __cdecl jkDSS_SendSetSaberInfo(sithThing*)                      { }
void  __cdecl jkDSS_SendSetSaberInfo2(sithThing*)                     { }
void  __cdecl jkDSS_SendSetSaberInfoMots(sithThing*, int)             { }

/* sithInventory, sithWeapon, sithTrackThing, sithAI, sithActor, sithPuppet —
 * real impls now in build (src/Gameplay, src/World, src/AI, src/Engine). */

/* sithDSSThing — multiplayer state-sync */
/* sithDSSThing state-sync helpers are compiled from src/Dss/sithDSSThing.c. */

/* sithMulti — multiplayer; C-linked because sithCog references _sithMulti_SendChat */
extern "C" {
/* sithMulti chat and score sync are compiled from src/Dss/sithMulti.c. */
}

/* sithEvent, sithItem, sithPuppet, stdStrTable — real impls in src/Gameplay,
 * src/World, src/Engine, src/General (now in build). */
