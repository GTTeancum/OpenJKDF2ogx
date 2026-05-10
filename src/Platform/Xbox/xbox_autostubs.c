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
int jkCutscene_PauseShow(int a0) { return 0; }
int jkCutscene_smack_related_loops(void) { return 0; }
int jkCutscene_sub_421310(char * a0) { return 0; }
int jkCutscene_sub_421410(void) { return 0; }
int jkDSS_wrap_SendSaberInfo_alt(void) { return 0; }
int jkDev_Open(void) { return 0; }
int jkGuiDialog_YesNoDialog(unsigned short * a0, unsigned short * a1) { return 0; }
int jkGuiForce_Show(int a0, int a1, int a2, unsigned short * a3, int * a4, int a5) { return 0; }
int jkGuiMultiTally_Show(int a0) { return 0; }
int jkGuiSingleTally_Show(void) { return 0; }
int jkGui_SetModeMenu(void const * a0) { return 0; }
int jkSmack_SmackPlay(char const * a0) { return 0; }
/* rdMath_PointsCollinear — now in rdMath.c */
/* rdThing_Draw, NewEntry, SetModel3, SetSprite3, SetParticleCloud — now in rdThing.c */
/* rdThing_Draw dispatches to these — stub the ones not yet compiled */
int rdSprite_Draw(struct rdThing * a0, struct rdMatrix34 * a1) { return 0; }
int rdParticle_Draw(struct rdThing * a0, struct rdMatrix34 * a1) { return 0; }
int rdPolyLine_Draw(struct rdThing * a0, struct rdMatrix34 * a1) { return 0; }
/* sithActor_ActorActorCollide, sithActor_LoadParams, sithActor_sub_4ED1D0 —
 * real impls in src/World/sithActor.c (now in build) */
int sithComm_Startup(void) { return 0; }
int sithComm_Sync(void) { return 0; }
int sithGamesave_Flush(void) { return 0; }
int sithGamesave_Load(char * a0, int a1, int a2) { return 0; }
int sithGamesave_Write(char * a0, int a1, int a2, unsigned short * a3) { return 0; }
/* stdBitmap_EnsureData — real impl in src/General/stdBitmap.c (now in build) */
/* stdConffile — now compiled from src\General\stdConffile.c */
int stdConsole_Startup(char const * a0, unsigned int a1, int a2) { return 0; }
int stdDisplay_DrawAndFlipGdi(unsigned int a0) { return 0; }
int stdDisplay_SetCooperativeLevel(unsigned int a0) { return 0; }
int stdDisplay_SetMode(unsigned int a0, void const * a1, int a2) { return 0; }
int stdDisplay_VBufferFill(struct stdVBuffer * a0, int a1, struct rdRect * a2) { return 0; }
int stdDisplay_VBufferLock(struct stdVBuffer * vbuf) { if (vbuf) vbuf->bSurfaceLocked = 1; return 1; }
int stdDisplay_VBufferSetColorKey(struct stdVBuffer * vbuf, int key) { if (vbuf) vbuf->transparent_color = (unsigned int)key; return 1; }
int stdDisplay_ddraw_waitforvblank(void) { return 0; }
int stdMci_CheckStatus(void) { return 0; }
int stdMci_Play(unsigned char a0, unsigned char a1) { return 0; }
int stdMci_Startup(void) { return 0; }
/* sithCog_actionCogIdk — defined in src/Cog/sithCog.c (now in build) */
/* sithCog_pActionCog — defined in src/Cog/sithCog.c (now in build) */
struct stdVBuffer * stdDisplay_VBufferNew(struct stdVBufferTexFmt * fmt, int a1, int a2, void const * a3) {
    unsigned int bpp_bytes;
    unsigned int pixel_sz;
    unsigned int w;
    unsigned int h;
    struct stdVBuffer *vbuf;
    if (!fmt) return 0;
    w = (unsigned int)fmt->width;
    h = (unsigned int)fmt->height;
    if (w == 0) w = 1;
    if (h == 0) h = 1;
    bpp_bytes = fmt->format.is16bit ? 2 : 1;
    pixel_sz = w * h * bpp_bytes;
    vbuf = (struct stdVBuffer *)malloc(sizeof(struct stdVBuffer) + pixel_sz);
    if (!vbuf) return 0;
    memset(vbuf, 0, sizeof(struct stdVBuffer));
    vbuf->format = *fmt;
    vbuf->format.texture_size_in_bytes = pixel_sz;
    vbuf->format.width_in_bytes = w * bpp_bytes;
    vbuf->format.width_in_pixels = w;
    vbuf->surface_lock_alloc = (char*)(vbuf + 1);
    return vbuf;
}
unsigned char * stdDisplay_GetPalette(void) { return 0; }
unsigned int stdSound_ParseWav(int a0, unsigned int * a1, int * a2, int * a3, int * a4) { return 0; }
unsigned int util_Weirdchecksum(unsigned char * a0, int a1, unsigned int a2) { return 0; }
void Video_Shutdown(void) { }
void Windows_Shutdown(void) { }
void Windows_Startup(void) { }
/* jkCog_Shutdown — defined in src/Cog/jkCog.c (now in build) */
void jkCredits_Shutdown(void) { }
void jkCredits_Startup(char * a0) { }
void jkCutscene_Shutdown(void) { }
void jkCutscene_Startup(char * a0) { }
void jkDSS_Shutdown(void) { }
void jkDev_Shutdown(void) { }
void jkDev_Startup(void) { }
void jkGuiControlOptions_Shutdown(void) { }
void jkGuiControlOptions_Startup(void) { }
void jkGuiControlSaveLoad_Shutdown(void) { }
void jkGuiControlSaveLoad_Startup(void) { }
void jkGuiDecision_Shutdown(void) { }
void jkGuiDecision_Startup(void) { }
void jkGuiDialog_ErrorDialog(unsigned short * a0, unsigned short * a1) { }
void jkGuiDialog_Shutdown(void) { }
void jkGuiDialog_Startup(void) { }
void jkGuiDisplay_Shutdown(void) { }
void jkGuiDisplay_Startup(void) { }
void jkGuiEsc_Show(void) { }
void jkGuiEsc_Shutdown(void) { }
void jkGuiEsc_Startup(void) { }
void jkGuiForce_Shutdown(void) { }
void jkGuiForce_Startup(void) { }
void jkGuiGameplay_Shutdown(void) { }
void jkGuiGameplay_Startup(void) { }
void jkGuiGeneral_Shutdown(void) { }
void jkGuiGeneral_Startup(void) { }
void jkGuiJoystick_Shutdown(void) { }
void jkGuiJoystick_Startup(void) { }
void jkGuiKeyboard_Shutdown(void) { }
void jkGuiKeyboard_Startup(void) { }
void jkGuiMain_Shutdown(void) { }
void jkGuiMain_Startup(void) { }
void jkGuiMap_Shutdown(void) { }
void jkGuiMap_Startup(void) { }
void jkGuiMods_Shutdown(void) { }
void jkGuiMods_Startup(void) { }
void jkGuiMouse_Shutdown(void) { }
void jkGuiMouse_Startup(void) { }
void jkGuiObjectives_Shutdown(void) { }
void jkGuiObjectives_Startup(void) { }
void jkGuiPlayer_Shutdown(void) { }
void jkGuiRend_Shutdown(void) { }
void jkGuiRend_Startup(void) { }
void jkGuiSaveLoad_Shutdown(void) { }
void jkGuiSaveLoad_Startup(void) { }
void jkGuiSetup_Shutdown(void) { }
void jkGuiSetup_Startup(void) { }
void jkGuiSingleTally_Shutdown(void) { }
void jkGuiSingleTally_Startup(void) { }
void jkGuiSingleplayer_Shutdown(void) { }
void jkGuiSingleplayer_Startup(void) { }
void jkGuiSound_Shutdown(void) { }
void jkGuiSound_Startup(void) { }
void jkGuiTitle_ShowLoading(char * a0, unsigned short * a1) { }
void jkGuiTitle_Shutdown(void) { }
void jkGuiTitle_Startup(void) { }
void jkGui_Shutdown(void) { }
void jkGui_copies_string(char * a0) { }
void jkQuakeConsole_Shutdown(void) { }
void jkQuakeConsole_Startup(void) { }
void jkSmack_Shutdown(void) { }
void jkSmack_Startup(void) { }
/* rdMath_CalcSurfaceNormal, ClampVector, ClampVectorRange — now in rdMath.c */
/* rdPrimit3_ClearFrameCounters/ClipFace/ClipFaceRGB/ClipFaceRGBLevel/NoClipFace/NoClipFaceRGB — now in rdPrimit3.c */
void rdPrimit2_DrawCircle(struct rdCanvas *a0, int a1, int a2, float a3, float a4, unsigned short a5, int a6) { }
void rdRaster_Startup(void) { }
/* rdThing_AccumulateMatrices — now in rdThing.c */
/* rdThing_FreeEntry — now in rdThing.c */
/* sithActor_MoveJointsForEyePYR, _Remove, _RemoveCorpse,
 * _SetMaxHeathForDifficulty, _Tick — real impls in src/World/sithActor.c
 * (now in build) */
void sithComm_Shutdown(void) { }
void sithDSSThing_SendDeath(struct sithThing * a0, struct sithThing * a1, char a2, int a3, int a4) { }
void sithDSSThing_SendFullDesc(struct sithThing * a0, int a1, int a2) { }
void sithDSSThing_SendPlaySound(struct sithThing * a0, struct rdVector3 * a1, struct sithSound * a2, float a3, float a4, int a5, int a6, int a7, int a8) { }
void sithDSSThing_SendPos(struct sithThing * a0, int a1, int a2) { }
void sithDSSThing_SendSyncThing(struct sithThing * a0, int a1, int a2) { }
void sithDSS_SendAIStatus(struct sithActor * a0, int a1, int a2) { }
void sithDSS_SendSectorFlags(struct sithSector * a0, int a1, int a2) { }
void sithDSS_SendSectorStatus(struct sithSector * a0, int a1, int a2) { }
void sithDSS_SendSurface(struct rdSurface * a0, int a1, int a2) { }
void sithDSS_SendSurfaceStatus(struct sithSurface * a0, int a1, int a2) { }
void sithDSS_SendSyncPuppet(struct sithThing * a0, int a1, int a2) { }
/* sithTime_Startup, sithTime_Tick — real impls in src/Gameplay/sithTime.c (now in build) */
/* std3D_EndScene provided by std3D.c */
/* stdConffile_Close — now compiled from src\General\stdConffile.c */
void stdDisplay_VBufferFree(struct stdVBuffer * vbuf) { if (vbuf) free(vbuf); }
void stdDisplay_VBufferUnlock(struct stdVBuffer * vbuf) { if (vbuf) vbuf->bSurfaceLocked = 0; }
void stdMci_SetVolume(float a0) { }
void stdMci_Shutdown(void) { }
void stdMci_Stop(void) { }
/* xbox_debug_Print/Printf — real implementations in xbox_debug.c */

/* =========================================================================
 * Level loading chain stubs — COG system, AI, sprites, events
 * These are safe to stub for "see the level and move around" testing.
 * ====================================================================== */

/* COG parser & execution — real impls in src/Cog/sithCogParse.c,
 * src/Cog/sithCogExec.c, src/Cog/sithCogFunction*.c (now in build). */
int sithDSSCog_SendSendTrigger(sithCog *,int,int,int,int,int,int,float,float,float,float,int) { return 0; }

/* Events */

/* AI */

/* Sprites */
void rdSprite_FreeEntry(rdSprite *) { }
int rdSprite_NewEntry(rdSprite *sprite,char *name,int type,char *mat,float w,float h,int geoMode,int lightMode,int texMode,float extralight,rdVector3 *off) {
    (void)type; (void)mat; (void)w; (void)h; (void)geoMode; (void)lightMode; (void)texMode; (void)extralight; (void)off;
    if (sprite && name) { strncpy(sprite->path, name, 31); sprite->path[31] = 0; }
    return 1;
}

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
void  __cdecl sithDSSThing_SendCreateThing(sithThing*, sithThing*, sithThing*, sithSector*,
                                            rdVector3*, rdVector3*, int, int)  { }
void  __cdecl sithDSSThing_SendDamage(sithThing*, sithThing*, float, short, int, int) { }
void  __cdecl sithDSSThing_SendDestroyThing(int, int)                 { }
void  __cdecl sithDSSThing_SendMOTSNew1(sithThing*, sithThing*, sithThing*, sithSector*,
                                         rdVector3*, rdVector3*, int, int) { }
void  __cdecl sithDSSThing_SendPathMove(sithThing*, short, float, int, int, int) { }
struct rdKeyframe;
void  __cdecl sithDSSThing_SendPlayKey(sithThing*, rdKeyframe*, int, short, int, int, int) { }
void  __cdecl sithDSSThing_SendPlayKeyMode(sithThing*, short, int, int, int) { }
void  __cdecl sithDSSThing_SendPlaySoundMode(sithThing*, short, int, float) { }
void  __cdecl sithDSSThing_SendSetThingModel(sithThing*, int)         { }
void  __cdecl sithDSSThing_SendStopKey(sithThing*, int, float, int, int) { }
struct sithPlayingSound;
void  __cdecl sithDSSThing_SendStopSound(sithPlayingSound*, float, int, int) { }
void  __cdecl sithDSSThing_SendSyncThingAttachment(sithThing*, int, int, int) { }

/* sithMulti — multiplayer; C-linked because sithCog references _sithMulti_SendChat */
extern "C" {
int sithMulti_SendChat(unsigned short*)                               { return 0; }
int sithMulti_SyncScores(void)                                        { return 0; }
}

/* sithEvent, sithItem, sithPuppet, stdStrTable — real impls in src/Gameplay,
 * src/World, src/Engine, src/General (now in build). */
