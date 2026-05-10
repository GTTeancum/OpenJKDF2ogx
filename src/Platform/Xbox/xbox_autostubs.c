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
float sithActor_Hit(struct sithThing * a0, struct sithThing * a1, float a2, int a3) { return 0.0f; }
int jkCredits_Show(void) { return 0; }
int jkCredits_Skip(void) { return 0; }
int jkCredits_Tick(void) { return 0; }
int jkCutscene_PauseShow(int a0) { return 0; }
int jkCutscene_smack_related_loops(void) { return 0; }
int jkCutscene_sub_421310(char * a0) { return 0; }
int jkCutscene_sub_421410(void) { return 0; }
int jkDSS_wrap_SendSaberInfo_alt(void) { return 0; }
int jkDev_Open(void) { return 0; }
int jkEpisode_EndLevel(struct jkEpisodeLoad * a0, int a1) { return 0; }
int jkEpisode_Load(struct jkEpisodeLoad * a0) { return 0; }
int jkEpisode_idk4(struct jkEpisodeLoad * a0, char * a1) { return 0; }
int jkGuiDialog_YesNoDialog(unsigned short * a0, unsigned short * a1) { return 0; }
int jkGuiForce_Show(int a0, int a1, int a2, unsigned short * a3, int * a4, int a5) { return 0; }
int jkGuiMultiTally_Show(int a0) { return 0; }
int jkGuiSingleTally_Show(void) { return 0; }
int jkGui_SetModeMenu(void const * a0) { return 0; }
int jkHudCameraView_Open(void) { return 0; }
int jkHudInv_InitItems(void) { return 0; }
int jkHudScope_Open(void) { return 0; }
int jkPlayer_SetAmmoMaximums(int a0) { return 0; }
int jkPlayer_WriteConfSwap(struct jkPlayerInfo * a0, int a1, char * a2) { return 0; }
int jkSmack_SmackPlay(char const * a0) { return 0; }
/* rdMath_PointsCollinear — now in rdMath.c */
/* rdThing_Draw, NewEntry, SetModel3, SetSprite3, SetParticleCloud — now in rdThing.c */
/* rdThing_Draw dispatches to these — stub the ones not yet compiled */
int rdSprite_Draw(struct rdThing * a0, struct rdMatrix34 * a1) { return 0; }
int rdParticle_Draw(struct rdThing * a0, struct rdMatrix34 * a1) { return 0; }
int rdPolyLine_Draw(struct rdThing * a0, struct rdMatrix34 * a1) { return 0; }
int sithAIAwareness_AddEntry(struct sithSector * a0, struct rdVector3 * a1, int a2, float a3, struct sithThing * a4) { return 0; }
int sithAIAwareness_Startup(void) { return 0; }
int sithAI_LoadThingActorParams(struct stdConffileArg * a0, struct sithThing * a1, int a2) { return 0; }
int sithAI_Open(void) { return 0; }
int sithAI_Startup(void) { return 0; }
int sithActor_ActorActorCollide(struct sithThing * a0, struct sithThing * a1, struct sithCollisionSearchEntry * a2, int a3) { return 0; }
int sithActor_LoadParams(struct stdConffileArg * a0, struct sithThing * a1, unsigned int a2) { return 0; }
int sithActor_sub_4ED1D0(struct sithThing * a0, struct sithSurface * a1, struct sithCollisionSearchEntry * a2) { return 0; }
int sithArchLighting_ParseSection(struct sithWorld * a0, int a1) { return 0; }
int sithComm_Startup(void) { return 0; }
int sithComm_Sync(void) { return 0; }
int sithEvent_Startup(void) { return 0; }
int sithExplosion_LoadThingParams(struct stdConffileArg * a0, struct sithThing * a1, int a2) { return 0; }
int sithGamesave_Flush(void) { return 0; }
int sithGamesave_Load(char * a0, int a1, int a2) { return 0; }
int sithGamesave_Write(char * a0, int a1, int a2, unsigned short * a3) { return 0; }
int sithItem_Collide(struct sithThing * a0, struct sithThing * a1, struct sithCollisionSearchEntry * a2, int a3) { return 0; }
int sithItem_LoadThingParams(struct stdConffileArg * a0, struct sithThing * a1, int a2) { return 0; }
int sithParticle_LoadThingParams(struct stdConffileArg * a0, struct sithThing * a1, int a2) { return 0; }
int sithParticle_Startup(void) { return 0; }
int sithPuppet_Startup(void) { return 0; }
int sithPuppet_StopKey(struct rdPuppet * a0, int a1, float a2) { return 0; }
int sithStrTable_Startup(void) { return 0; }
int sithTrackThing_LoadPathParams(struct stdConffileArg * a0, struct sithThing * a1, int a2) { return 0; }
int sithWeapon_Collide(struct sithThing * a0, struct sithThing * a1, struct sithCollisionSearchEntry * a2, int a3) { return 0; }
int sithWeapon_HitDebug(struct sithThing * a0, struct sithSurface * a1, struct sithCollisionSearchEntry * a2) { return 0; }
int sithWeapon_LoadParams(struct stdConffileArg * a0, struct sithThing * a1, int a2) { return 0; }
int stdBitmap_EnsureData(struct stdBitmap * a0) { return 0; }
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
int jkPlayer_bJankyPhysics = {0};
int jkPlayer_bUseOldPlayerPhysics = {0};
int jkPlayer_enableOrigAspect = {0};
int jkPlayer_fov = 90;
int jkPlayer_fovIsVertical = {0};
/* sithCog_actionCogIdk — defined in src/Cog/sithCog.c (now in build) */
struct jkBubbleInfo * jkPlayer_aBubbleInfo = {0};
struct jkEpisodeEntry * jkEpisode_GetCurrentEpisodeEntry(struct jkEpisodeLoad * a0) { return 0; }
struct jkEpisodeEntry * jkEpisode_GetNextEntryInDecisionPath(struct jkEpisodeLoad * a0, int a1) { return 0; }
struct rdParticle * sithParticle_LoadEntry(char const * a0) { return 0; }
/* sithCog_pActionCog — defined in src/Cog/sithCog.c (now in build) */
struct sithPuppet * sithPuppet_NewEntry(struct sithThing * a0) { return 0; }
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
unsigned short * sithStrTable_GetUniStringWithFallback(char * a0) { return 0; }
void Video_Shutdown(void) { }
void Windows_Shutdown(void) { }
void Windows_Startup(void) { }
void jkAI_Startup(void) { }
/* jkCog_Shutdown — defined in src/Cog/jkCog.c (now in build) */
void jkCredits_Shutdown(void) { }
void jkCredits_Startup(char * a0) { }
void jkCutscene_Shutdown(void) { }
void jkCutscene_Startup(char * a0) { }
void jkDSS_Shutdown(void) { }
void jkDev_Shutdown(void) { }
void jkDev_Startup(void) { }
void jkGame_Shutdown(void) { }
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
void jkPlayer_Close(void) { }
void jkPlayer_MpcInitBins(struct sithPlayerInfo * a0) { }
void jkPlayer_SetPovModel(struct jkPlayerInfo * a0, struct rdModel3 * a1) { }
void jkPlayer_Shutdown(void) { }
void jkPlayer_Startup(void) { }
void jkPlayer_nullsub_1(struct jkPlayerInfo * a0) { }
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
void sithAIAwareness_Shutdown(void) { }
void sithAI_Close(void) { }
void sithAI_FreeEntry(struct sithThing * a0) { }
void sithAI_NewEntry(struct sithThing * a0) { }
void sithAI_Shutdown(void) { }
void sithAI_Tick(struct sithThing * a0, float a1) { }
void sithAI_TickAll(void) { }
void sithActor_MoveJointsForEyePYR(struct sithThing * a0, struct rdVector3 const * a1) { }
void sithActor_Remove(struct sithThing * a0) { }
void sithActor_RemoveCorpse(struct sithThing * a0) { }
void sithActor_SetMaxHeathForDifficulty(struct sithThing * a0) { }
void sithActor_Tick(struct sithThing * a0, int a1) { }
void sithArchLighting_Free(struct sithWorld * a0) { }
void sithComm_Shutdown(void) { }
void sithConsole_AdvanceLogBuf(void) { }
void sithConsole_AlertSound(void) { }
void sithConsole_Print(char const * a0) { }
void sithConsole_PrintUniStr(unsigned short const * a0) { }
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
void sithEvent_Advance(void) { }
void sithEvent_Close(void) { }
void sithEvent_Open(void) { }
void sithEvent_Shutdown(void) { }
void sithExplosion_CreateThing(struct sithThing * a0) { }
void sithExplosion_Tick(struct sithThing * a0) { }
void sithInventory_ClearInventory(struct sithThing * a0) { }
void sithInventory_Reset(struct sithThing * a0) { }
void sithInventory_SendFire(struct sithThing * a0) { }
void sithInventory_SendKilledMessageToAll(struct sithThing * a0, struct sithThing * a1) { }
void sithInventory_SetCarries(struct sithThing * a0, int a1, int a2) { }
void sithItem_New(struct sithThing * a0) { }
void sithItem_Remove(struct sithThing * a0) { }
void sithOverlayMap_FuncDecrease(void) { }
void sithOverlayMap_FuncIncrease(void) { }
void sithOverlayMap_ToggleMapDrawn(void) { }
void sithParticle_CreateThing(struct sithThing * a0) { }
void sithParticle_Free(struct sithWorld * a0) { }
void sithParticle_FreeEntry(struct sithThing * a0) { }
void sithParticle_Remove(struct sithThing * a0) { }
void sithParticle_Shutdown(void) { }
void sithParticle_Tick(struct sithThing * a0, float a1) { }
void sithPlayerActions_Activate(struct sithThing * a0) { }
void sithPlayerActions_JumpWithVel(struct sithThing * a0, float a1) { }
void sithPlayerActions_WarpToCheckpoint(struct sithThing * a0, int a1) { }
void sithPuppet_FreeEntry(struct sithThing * a0) { }
void sithPuppet_Shutdown(void) { }
void sithPuppet_Tick(struct sithThing * a0, float a1) { }
void sithPuppet_sub_4E4760(struct sithThing * a0, int a1) { }
void sithStrTable_Shutdown(void) { }
/* sithTime_Startup, sithTime_Tick — real impls in src/Gameplay/sithTime.c (now in build) */
void sithTrackThing_Tick(struct sithThing * a0, float a1) { }
void sithTrackThing_idkpathmove(struct sithThing * a0, struct sithThing * a1, struct rdVector3 * a2) { }
void sithWeapon_Remove(struct sithThing * a0) { }
void sithWeapon_SetTimeLeft(struct sithThing * a0, struct sithThing * a1, float a2) { }
void sithWeapon_ShutdownEntry(void) { }
void sithWeapon_Startup(void) { }
void sithWeapon_StartupEntry(void) { }
void sithWeapon_SyncPuppet(struct sithThing * a0) { }
void sithWeapon_Tick(struct sithThing * a0, float a1) { }
void sithWeapon_handle_inv_msgs(struct sithThing * a0) { }
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
int sithEvent_RegisterFunc(int,int (__cdecl*)(int,sithEventInfo *),int,int) { return 0; }

/* AI */
sithAICommand * sithAI_FindCommand(char const *) { return 0; }

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
float jkPlayer_canonicalCogTickrate = 50.0f;
float jkPlayer_aMultiParams[8] = {0,0,0,0,0,0,0,0};
/* jkStrings_tableExtOver — referenced as C symbol _jkStrings_tableExtOver */
extern "C" {
unsigned int jkStrings_tableExtOver = 0;
}
/* sithAIAlign defined in types.h:2737 — don't redefine */
sithAIAlign* sithAI_aAlignments = 0;

/* jkPlayer */
struct jkPlayerInfo;
jkPlayerInfo* __cdecl jkPlayer_FUN_00404fe0(sithThing*)               { return 0; }
int   __cdecl jkPlayer_GetJediRank(void)                              { return 0; }
int   __cdecl jkPlayer_SyncForcePowers(int, int)                      { return 0; }
void  __cdecl jkPlayer_SetWaggle(sithThing*, rdVector3*, float)       { }

/* jkEpisode */
int   __cdecl jkEpisode_GetBubbleInfo(sithThing*, unsigned int*, sithThing**, float*) { return 0; }
void  __cdecl jkEpisode_CreateBubble(sithThing*, float, unsigned int) { }
void  __cdecl jkEpisode_DestroyBubble(sithThing*)                     { }

/* jkHud */
void  __cdecl jkHud_EndTarget(void)                                   { }
void  __cdecl jkHud_SetTarget(sithThing*)                             { }
void  __cdecl jkHud_SetTargetColors(int*)                             { }

/* jkSaber */
void  __cdecl jkSaber_Disable(sithThing*)                             { }
void  __cdecl jkSaber_Enable(sithThing*, float, float, float)         { }
void  __cdecl jkSaber_InitializeSaberInfo(sithThing*, char*, char*, float, float, float,
                                          sithThing*, sithThing*, sithThing*) { }

/* jkDSS — multiplayer state-sync packets, no-op on Xbox single-player */
void  __cdecl jkDSS_SendJKEnableSaber(sithThing*)                     { }
void  __cdecl jkDSS_SendJKPrintUniString(int, unsigned int)           { }
void  __cdecl jkDSS_SendJKSetWeaponMesh(sithThing*)                   { }
void  __cdecl jkDSS_SendSetSaberInfo(sithThing*)                      { }
void  __cdecl jkDSS_SendSetSaberInfo2(sithThing*)                     { }
void  __cdecl jkDSS_SendSetSaberInfoMots(sithThing*, int)             { }

/* sithInventory */
struct sithItemDescriptor;
sithItemDescriptor* __cdecl sithInventory_GetItemDesc(sithThing*, int){ return 0; }
sithThing* __cdecl sithInventory_CreateBackpack(sithThing*)           { return 0; }
float __cdecl sithInventory_ChangeInv(sithThing*, int, float)         { return 0.0f; }
float __cdecl sithInventory_DeactivateBin(sithThing*, sithCog*, int)  { return 0.0f; }
float __cdecl sithInventory_GetBinAmount(sithThing*, int)             { return 0.0f; }
float __cdecl sithInventory_GetMax(sithThing*, int)                   { return 0.0f; }
float __cdecl sithInventory_GetMin(sithThing*, int)                   { return 0.0f; }
float __cdecl sithInventory_NthBackpackValue(sithThing*, int)         { return 0.0f; }
float __cdecl sithInventory_SetBinAmount(sithThing*, int, float)      { return 0.0f; }
int   __cdecl sithInventory_ActivateBin(sithThing*, sithCog*, float, int)  { return 0; }
int   __cdecl sithInventory_GetActivate(sithThing*, int)              { return 0; }
int   __cdecl sithInventory_GetAvailable(sithThing*, int)             { return 0; }
int   __cdecl sithInventory_GetCurItem(sithThing*)                    { return 0; }
int   __cdecl sithInventory_GetCurWeapon(sithThing*)                  { return 0; }
int   __cdecl sithInventory_NthBackpackBin(sithThing*, int)           { return 0; }
int   __cdecl sithInventory_NumBackpackItems(sithThing*)              { return 0; }
int   __cdecl sithInventory_SelectWeaponFollowing(int)                { return 0; }
int   __cdecl sithInventory_SelectWeaponPrior(int)                    { return 0; }
void  __cdecl sithInventory_PickupBackpack(sithThing*, sithThing*)    { }
void  __cdecl sithInventory_SetActivate(sithThing*, int, int)         { }
void  __cdecl sithInventory_SetAvailable(sithThing*, int, int)        { }
void  __cdecl sithInventory_SetBinWait(sithThing*, int, float)        { }
void  __cdecl sithInventory_SetCurWeapon(sithThing*, int)             { }
void  __cdecl sithInventory_SetFlags(sithThing*, int, int)            { }

/* sithWeapon */
float __cdecl sithWeapon_Deactivate(sithThing*, sithCog*, int)        { return 0.0f; }
float __cdecl sithWeapon_GetPriority(sithThing*, int, int)            { return 0.0f; }
int   __cdecl sithWeapon_AutoSelect(sithThing*, int)                  { return 0; }
int   __cdecl sithWeapon_GetCurWeaponMode(void)                       { return 0; }
int   __cdecl sithWeapon_SelectWeapon(sithThing*, int, int)           { return 0; }
sithThing* __cdecl sithWeapon_FireProjectile(sithThing*, sithThing*, sithSound*, int,
                                              rdVector3*, rdVector3*, float, short,
                                              float, float, int)      { return 0; }
void  __cdecl sithWeapon_Activate(sithThing*, sithCog*, float, int)   { }
void  __cdecl sithWeapon_SetFireRate(sithThing*, float)               { }
void  __cdecl sithWeapon_SetFireWait(sithThing*, float)               { }
void  __cdecl sithWeapon_SetMountWait(sithThing*, float)              { }

/* sithTrackThing */
int   __cdecl sithTrackThing_PathMovePause(sithThing*)                { return 0; }
int   __cdecl sithTrackThing_PathMoveResume(sithThing*)               { return 0; }
void  __cdecl sithTrackThing_MoveToFrame(sithThing*, int, float)      { }
void  __cdecl sithTrackThing_Rotate(sithThing*, rdVector3*)           { }
void  __cdecl sithTrackThing_RotatePivot(sithThing*, rdVector3*, rdVector3*, float) { }
void  __cdecl sithTrackThing_SkipToFrame(sithThing*, unsigned int, float)   { }
void  __cdecl sithTrackThing_Stop(sithThing*)                         { }

/* sithAI extras (sithAIClass.c is in build but these specific are in sithAI.c) */
int   __cdecl sithAI_FirstThingInCone(sithSector*, rdMatrix34*, float, float, int,
                                       sithThing**, int, float)       { return 0; }
int   __cdecl sithAI_FirstThingInView(sithSector*, rdMatrix34*, float, float, int,
                                       sithThing**, int, float)       { return 0; }
void  __cdecl sithAI_AddAlignmentPriority(float)                      { }
struct sithActor;
void  __cdecl sithAI_Jump(sithActor*, rdVector3*, float)              { }
void  __cdecl sithAI_SetActorFireTarget(sithActor*, int, int)         { }
void  __cdecl sithAI_SetDistractor(sithThing*)                        { }
void  __cdecl sithAI_SetLookFrame(sithActor*, rdVector3*)             { }
void  __cdecl sithAI_SetMoveThing(sithActor*, rdVector3*, float)      { }

/* sithActor */
void  __cdecl sithActor_SpawnDeadBodyMaybe(sithThing*, sithThing*, int) { }

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

/* sithEvent */
int   __cdecl sithEvent_Set(int, sithEventInfo*, unsigned int)        { return 0; }
struct sithEvent;
void  __cdecl sithEvent_Kill(sithEvent*)                              { }

/* sithItem */
void  __cdecl sithItem_Take(sithThing*, sithThing*, int)              { }

/* sithPuppet */
struct rdPuppet;
int   __cdecl sithPuppet_StartKey(rdPuppet*, rdKeyframe*, int, int, int,
                                   void(__cdecl*)(sithThing*, int, unsigned int)) { return 0; }
void  __cdecl sithPuppet_SetArmedMode(sithThing*, int)                { }

/* stdStrTable */
struct stdStrTable;
void  __cdecl stdStrTable_Free(stdStrTable*)                          { }
int   __cdecl stdStrTable_Load(stdStrTable*, char*)                   { return 0; }
unsigned short* __cdecl stdStrTable_GetUniString(stdStrTable*, const char*) { return 0; }
