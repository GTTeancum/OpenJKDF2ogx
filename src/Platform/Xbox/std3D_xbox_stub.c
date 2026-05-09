/*
 * std3D_xbox_stub.c — D3D stub for headless operation
 * Location: src/Platform/Xbox/std3D_xbox_stub.c
 *
 * All functions return safely. No D3D headers needed.
 * Replaces std3D.c when d3d8-xbox.lib is not linked.
 */

#include "platform_xbox.h"
#include "xbox_debug.h"
#include <string.h>

#ifdef __cplusplus
extern "C" {
#endif

int  std3D_bReinitHudElements = 0;
int  std3D_bInitted           = 0;

int  std3D_Startup(void)             { XDBG("std3D_Startup: stub (no D3D)\n"); return 1; }
void std3D_Shutdown(void)            {}
int  std3D_StartScene(void)          { return 1; }
int  std3D_EndScene(void)            { return 1; }
void std3D_ResetRenderList(void)     {}
void std3D_ResetUIRenderList(void)   {}
int  std3D_RenderListVerticesFinish(void) { return 1; }
void std3D_DrawRenderList(void)      {}
void std3D_DrawMenu(void)            {}
void std3D_DrawSceneFbo(void)        {}
void std3D_FreeResources(void)       {}
int  std3D_ClearZBuffer(void)        { return 1; }

int  std3D_SetCurrentPalette(void *a1, int a2)
         { (void)a1; (void)a2; return 1; }
void std3D_GetValidDimension(unsigned int inW, unsigned int inH,
                              unsigned int *outW, unsigned int *outH)
         { *outW = inW; *outH = inH; }
int  std3D_DrawOverlay(void)         { return 1; }
void std3D_UnloadAllTextures(void)   {}
void std3D_AddRenderListTris(void *tris, unsigned int n)
         { (void)tris; (void)n; }
void std3D_AddRenderListLines(void *lines, unsigned int n)
         { (void)lines; (void)n; }
int  std3D_AddRenderListVertices(void *v, int n)
         { (void)v; (void)n; return 1; }
void std3D_UpdateFrameCount(void *p)  { (void)p; }
void std3D_RemoveTextureFromCacheList(void *p) { (void)p; }
void std3D_AddTextureToCacheList(void *p)      { (void)p; }
int  std3D_PurgeTextureCache(size_t s)          { (void)s; return 1; }
void std3D_PurgeEntireTextureCache(void)        {}
int  std3D_AddToTextureCache(void *v, void *t, int a, int b)
         { (void)v;(void)t;(void)a;(void)b; return 1; }
int  std3D_AddBitmapToTextureCache(void *b, int m, int a, int n)
         { (void)b;(void)m;(void)a;(void)n; return 1; }
void std3D_PurgeUIEntry(int i, int idx)    { (void)i;(void)idx; }
void std3D_PurgeTextureEntry(int i)        { (void)i; }
void std3D_PurgeBitmapRefs(void *p)   { (void)p; }
void std3D_PurgeSurfaceRefs(void *p) { (void)p; }
void std3D_UpdateSettings(void)            {}
void std3D_Screenshot(const char *p)       { (void)p; }
void std3D_InitializeViewport(void *r)   { (void)r; }
int  std3D_GetValidDimensions(int a,int b,int c,int d)
         { (void)a;(void)b;(void)c;(void)d; return 1; }
int  std3D_FindClosestDevice(unsigned int i, int a) { (void)i;(void)a; return 0; }
int  std3D_SetRenderList(intptr_t a)       { (void)a; return 1; }
intptr_t std3D_GetRenderList(void)         { return 0; }
int  std3D_CreateExecuteBuffer(void)       { return 1; }
int  std3D_IsReady(void)                   { return 1; }
int  std3D_HasAlpha(void)                  { return 1; }
int  std3D_HasModulateAlpha(void)          { return 1; }
int  std3D_HasAlphaFlatStippled(void)      { return 0; }
void std3D_DrawUIBitmapRGBA(void *p, int m, float x, float y,
     void *r, float sx, float sy, int b,
     unsigned char cr, unsigned char cg, unsigned char cb, unsigned char ca)
     { (void)p;(void)m;(void)x;(void)y;(void)r;
       (void)sx;(void)sy;(void)b;(void)cr;(void)cg;(void)cb;(void)ca; }
void std3D_DrawUIBitmap(void *p, int m, float x, float y,
     void *r, float s, int b)
     { (void)p;(void)m;(void)x;(void)y;(void)r;(void)s;(void)b; }
void std3D_DrawUIClearedRect(unsigned char idx, void *r)
     { (void)idx;(void)r; }
void std3D_DrawUIClearedRectRGBA(unsigned char rr,unsigned char g,unsigned char b,
     unsigned char a, void *rc)
     { (void)rr;(void)g;(void)b;(void)a;(void)rc; }
void *std3D_GetDevice(void) { return 0; }

#ifdef __cplusplus
}
#endif
