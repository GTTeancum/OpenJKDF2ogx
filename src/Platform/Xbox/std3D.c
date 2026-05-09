/*
 * std3D.c  —  OpenJKDF2 Xbox renderer adapter.
 *
 * The renderer is xquake's FakeGL (src/Platform/Xbox/fakeglx.cpp), copied
 * verbatim from Microsoft's xquake port to the original Xbox.  This file
 * does NOT contain rendering code.  It is purely an adapter that exposes
 * OpenJKDF2's std3D_* API and translates each call into the gl* / wgl* /
 * FakeSwapBuffers entry points implemented by fakeglx.cpp.
 *
 * No D3D types or D3DDevice_* / Direct3D_* / D3DTexture_* calls live here.
 * If you need to touch GPU state, do it via gl* — that is the contract.
 */

#include "platform_xbox.h"   /* Must come first — sets up Xbox defines. */
#include "xbox_debug.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * Renderer entry points (defined in fakeglx.cpp).
 *
 * Forward-declared here so this TU does not pull in gl/gl.h, windows.h,
 * or xtl.h.  Calling convention: APIENTRY = __stdcall on Win/Xbox.
 * ---------------------------------------------------------------------- */
typedef unsigned int GLenum;
typedef unsigned int GLbitfield;
typedef float        GLfloat;
typedef double       GLdouble;
typedef int          GLint;
typedef int          GLsizei;
typedef unsigned int GLuint;
typedef unsigned char GLubyte;
typedef int          GLsizeiptr_int; /* placeholder, unused */

void * __stdcall wglCreateContext(void *hdc);
int    __stdcall wglMakeCurrent(void *hdc, void *hglrc);
void             FakeSwapBuffers(void);

void __stdcall glClear        (GLbitfield mask);
void __stdcall glClearColor   (GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void __stdcall glBegin        (GLenum mode);
void __stdcall glEnd          (void);
void __stdcall glColor4f      (GLfloat r, GLfloat g, GLfloat b, GLfloat a);
void __stdcall glColor3ubv    (const GLubyte *v);
void __stdcall glTexCoord2f   (GLfloat s, GLfloat t);
void __stdcall glVertex3f     (GLfloat x, GLfloat y, GLfloat z);
void __stdcall glViewport     (GLint x, GLint y, GLsizei w, GLsizei h);
void __stdcall glMatrixMode   (GLenum mode);
void __stdcall glLoadIdentity (void);
void __stdcall glLoadMatrixf  (const GLfloat *m);
void __stdcall glOrtho        (GLdouble l, GLdouble r, GLdouble b,
                               GLdouble t, GLdouble n, GLdouble f);
void __stdcall glEnable       (GLenum cap);
void __stdcall glDisable      (GLenum cap);

#ifdef __cplusplus
} /* extern "C" */
#endif

#define GL_TRIANGLES           0x0004
#define GL_COLOR_BUFFER_BIT    0x00004000
#define GL_DEPTH_BUFFER_BIT    0x00000100
#define GL_STENCIL_BUFFER_BIT  0x00000400
#define GL_PROJECTION          0x1701
#define GL_MODELVIEW           0x1700
#define GL_TEXTURE_2D          0x0DE1
#define GL_DEPTH_TEST          0x0B71
#define GL_CULL_FACE           0x0B44
#define GL_BLEND               0x0BE2
#define GL_ALPHA_TEST          0x0BC0
#define GL_RGBA                0x1908
#define GL_UNSIGNED_BYTE       0x1401

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * Engine-side type forward declarations.
 *
 * These match the layouts the engine writes through the std3D_* API.
 * Kept local to this TU so we don't need to drag in types.h's full
 * std3D.h chain (which assumes the SDL2_RENDER renderer exists).
 * ---------------------------------------------------------------------- */
typedef struct
{
    float        x, y, z;
    float        nx, ny, nz;
    float        tu, tv;
    unsigned int color;
    float        lightLevel;
} D3DVERTEX;

typedef struct rdDDrawSurface_tag
{
    int  texture_loaded;
    int  texture_id;
    int  is_16bit;
    void *palette;
} rdDDrawSurface;

typedef struct
{
    int             v1, v2, v3;
    int             flags;
    rdDDrawSurface *texture;
} rdTri;

typedef struct
{
    int v1, v2;
    int flags;
} rdLine;

typedef struct stdVBuffer_tag stdVBuffer;  /* opaque — only addressed by pointer */

#ifndef STD3D_MAX_VERTICES
#define STD3D_MAX_VERTICES   8192
#endif
#ifndef STD3D_MAX_TRIS
#define STD3D_MAX_TRIS       4096
#endif
#ifndef STD3D_MAX_TEXTURES
#define STD3D_MAX_TEXTURES   2048
#endif

/* ----------------------------------------------------------------------
 * Module state.  Just bookkeeping — no GPU resources owned here.
 * ---------------------------------------------------------------------- */
static int  g_initialized = 0;
static int  g_sceneOpen   = 0;

/* Engine-side staging buffers.  rdCache writes into these, then calls
 * std3D_DrawRenderList which translates to glBegin/glEnd loops.  For the
 * current diagnostic build, std3D_DrawRenderList draws a single hardcoded
 * test triangle and discards the engine's staged data. */
static D3DVERTEX GL_tmpVertices[STD3D_MAX_VERTICES];
static int       GL_numVertices  = 0;
static int       GL_verticesDone = 0;

static rdTri     GL_tmpTris[STD3D_MAX_TRIS];
static int       GL_numTris      = 0;

static rdLine    GL_tmpLines[1024];
static int       GL_numLines     = 0;

/* ====================================================================== */
/* std3D_Startup                                                          */
/* ====================================================================== */
int std3D_Startup(void)
{
    XDBG("std3D_Startup: enter\n");

    if (g_initialized)
    {
        XDBG("std3D_Startup: already initialized, skipping\n");
        return 1;
    }

    /* Renderer init — FakeGL constructor calls Direct3DCreate8 +
     * CreateDevice with the canonical xquake d3dpp.  HDC/HGLRC are
     * unused on the Xbox path of fakeglx.cpp; pass NULL. */
    XDBG("std3D_Startup: wglCreateContext\n");
    {
        void *ctx = wglCreateContext((void*)0);
        XDBGF("std3D_Startup: wglCreateContext ctx=%p\n", ctx);
        if (!ctx)
        {
            XDBG("std3D_Startup: wglCreateContext failed, continuing headless\n");
            return 1;
        }
        wglMakeCurrent((void*)0, ctx);
        XDBG("std3D_Startup: wglMakeCurrent done\n");
    }

    /* Initial GL state — pixel-space ortho, all enables off.
     *
     * Z range is the same wide span xquake uses for its 2D HUD setup
     * (gl_draw.c:920 GL_Set2D).  The near/far values here aren't a literal
     * world-space depth range — they parametrize D3DXMatrixOrthoOffCenterRH's
     * z mapping.  With (n, f) = (-99999, 99999) the matrix's z scale and
     * bias produce a near-identity remap that keeps any reasonable input
     * z inside D3D's [0, 1] clip range.  Narrow ranges like (0, 1) flip
     * positive view-z into negative NDC-z, front-clipping the geometry. */
    glViewport(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 640.0, 480.0, 0.0, -99999.0, 99999.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);
    XDBG("std3D_Startup: initial GL state set\n");

    /* Stability loop — confirms FakeGL's frame loop is alive. */
    {
        int frame;
        XDBG("std3D_Startup: STABILITY loop start (60 frames)\n");
        for (frame = 0; frame < 60; ++frame)
        {
            glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
            FakeSwapBuffers();
            if ((frame % 20) == 0)
                XDBGF("std3D_Startup: STABILITY frame %d done\n", frame);
        }
        XDBG("std3D_Startup: STABILITY loop complete\n");
    }

    g_initialized = 1;
    XDBG("std3D_Startup: done\n");
    return 1;
}

/* ====================================================================== */
/* std3D_Shutdown                                                         */
/* ====================================================================== */
void std3D_Shutdown(void)
{
    XDBG("std3D_Shutdown: enter\n");
    /* FakeGL has no explicit teardown call; wglDeleteContext is the
     * conventional one but nothing in xquake's Xbox path actually
     * shuts down (game just exits).  Mirror that behaviour. */
    g_initialized = 0;
}

/* ====================================================================== */
/* std3D_StartScene                                                       */
/* ====================================================================== */
int std3D_StartScene(void)
{
    if (!g_initialized || g_sceneOpen) return 1;

    /* glClear flushes pending vertex stream + applies dirty state +
     * issues D3D Clear (gl_fakegl.cpp:1823-1849).  This is the canonical
     * frame-start in xquake; FakeGL's lazy BeginScene fires on the
     * subsequent glBegin. */
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);

    /* Diagnostic per-frame triangle.  The engine's render path stops
     * submitting DrawRenderList after level load (faces=0 from sithRender),
     * so a triangle drawn from DrawRenderList is cleared next frame and
     * disappears.  Drawing it here forces a fresh triangle every frame,
     * isolating "is the renderer producing visible pixels?" from "is the
     * engine submitting geometry?". */
    glBegin(GL_TRIANGLES);
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);  glVertex3f(160.0f, 240.0f, 0.0f);
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);  glVertex3f(320.0f,  60.0f, 0.0f);
    glColor4f(0.0f, 0.0f, 1.0f, 1.0f);  glVertex3f(480.0f, 240.0f, 0.0f);
    glEnd();

    g_sceneOpen     = 1;
    GL_numVertices  = 0;
    GL_verticesDone = 0;
    GL_numTris      = 0;
    GL_numLines     = 0;
    return 1;
}

/* ====================================================================== */
/* std3D_EndScene                                                         */
/* ====================================================================== */
int std3D_EndScene(void)
{
    if (!g_initialized || !g_sceneOpen) return 1;

    /* No EndScene call here — FakeSwapBuffers does internalEnd +
     * EndScene + Present in one step (gl_fakegl.cpp:2557-2570). */
    g_sceneOpen     = 0;
    GL_numVertices  = 0;
    GL_verticesDone = 0;
    GL_numTris      = 0;
    GL_numLines     = 0;
    return 1;
}

/* ====================================================================== */
/* std3D_Present                                                          */
/* ====================================================================== */
void std3D_Present(void)
{
    if (!g_initialized) { XDBG("std3D_Present: not initialized\n"); return; }
    { static int _sw=0; if(_sw<3){ XDBG("std3D_Present: FakeSwapBuffers\n"); } _sw++; }
    FakeSwapBuffers();
    { static int _sr=0; if(_sr<3){ XDBG("std3D_Present: returned\n"); } _sr++;
      if(_sr==100||_sr==1000||(_sr%5000)==0){ XDBGF("std3D_Present: milestone tick %d\n",_sr); } }
}

/* ====================================================================== */
/* Render-list staging (engine fills these; std3D_DrawRenderList consumes) */
/* ====================================================================== */
int std3D_AddRenderListVertices(D3DVERTEX *verts, int count)
{
    { static int _av=0; if(_av<5){ XDBGF("ARV: count=%d GL_nverts=%d\n", count, GL_numVertices); _av++; } }
    if (!verts || count <= 0) return 1;
    if (GL_numVertices + count >= STD3D_MAX_VERTICES) return 0;
    memcpy(&GL_tmpVertices[GL_numVertices], verts, count * sizeof(D3DVERTEX));
    GL_numVertices += count;
    return 1;
}

void std3D_RenderListVerticesFinish(void)
{
    { static int _vf=0; if(_vf<5){ XDBGF("VF: done nverts=%d\n", GL_numVertices); _vf++; } }
    GL_verticesDone = 1;
}

void std3D_AddRenderListTris(rdTri *tris, unsigned int num_tris)
{
    if (!tris || num_tris == 0) return;
    if (GL_numTris + (int)num_tris >= STD3D_MAX_TRIS)
        num_tris = STD3D_MAX_TRIS - GL_numTris;
    if ((int)num_tris <= 0) return;
    memcpy(&GL_tmpTris[GL_numTris], tris, num_tris * sizeof(rdTri));
    GL_numTris += num_tris;
}

void std3D_AddRenderListLines(rdLine *lines, unsigned int num_lines)
{
    if (!lines || num_lines == 0) return;
    if (GL_numLines + (int)num_lines >= 1024)
        num_lines = 1024 - GL_numLines;
    if ((int)num_lines <= 0) return;
    memcpy(&GL_tmpLines[GL_numLines], lines, num_lines * sizeof(rdLine));
    GL_numLines += num_lines;
}

void std3D_AddRenderListNGons(void *ngons, unsigned int n)
{
    (void)ngons; (void)n;
}

void std3D_ResetRenderList(void)
{
    GL_numVertices  = 0;
    GL_verticesDone = 0;
    GL_numTris      = 0;
    GL_numLines     = 0;
}

/* ====================================================================== */
/* std3D_DrawRenderList                                                   */
/* Diagnostic build: discard engine geometry, draw a single RGB triangle. */
/* ====================================================================== */
void std3D_DrawRenderList(void)
{
    if (!g_initialized || !g_sceneOpen) return;

    if (GL_numTris > 0)
    {
        static int _fl = 0;
        if (_fl < 3) { XDBGF("DIAG: nTris=%d\n", GL_numTris); _fl++; }
    }

    /* Discard staged engine geometry — DIAG draws a hardcoded triangle. */
    GL_numVertices = 0; GL_verticesDone = 0; GL_numTris = 0; GL_numLines = 0;

    { static int _dc=0; if(_dc<3){ XDBG("DIAG: glBegin TRIANGLES\n"); _dc++; } }
    glBegin(GL_TRIANGLES);
    glColor4f(1.0f, 0.0f, 0.0f, 1.0f);  glVertex3f(160.0f, 240.0f, 0.0f);
    glColor4f(0.0f, 1.0f, 0.0f, 1.0f);  glVertex3f(320.0f,  60.0f, 0.0f);
    glColor4f(0.0f, 0.0f, 1.0f, 1.0f);  glVertex3f(480.0f, 240.0f, 0.0f);
    glEnd();
    { static int _dd=0; if(_dd<3){ XDBG("DIAG: glEnd done\n"); _dd++; } }
}

/* ====================================================================== */
/* Texture cache — stubs for now.  Once first pixel is on screen we'll
 * wire glTexImage2D + glBindTexture into AddToTextureCache.  For the
 * current DIAG path no real textures are used, so engine calls succeed
 * silently and the engine carries on.
 * ====================================================================== */
int std3D_AddToTextureCache(stdVBuffer *vbuf, rdDDrawSurface *texture,
                             int is_alpha_tex, int no_mip)
{
    (void)vbuf; (void)is_alpha_tex; (void)no_mip;
    if (texture)
    {
        texture->texture_loaded = 1;
        texture->texture_id     = 1;  /* any non-zero slot id */
    }
    return 1;
}

void std3D_UnloadAllTextures(void)              {}
int  std3D_SetCurrentPalette(void *p, int a)    { (void)p; (void)a; return 1; }
void std3D_GetValidDimension(unsigned int inW, unsigned int inH,
                              unsigned int *outW, unsigned int *outH)
{
    /* NV2A requires power-of-two textures; round up. */
    unsigned int w = 1, h = 1;
    while (w < inW) w <<= 1;
    while (h < inH) h <<= 1;
    if (outW) *outW = w;
    if (outH) *outH = h;
}

int std3D_HasAlpha(void)             { return 1; }
int std3D_HasAlphaFlatStippled(void) { return 0; }
int std3D_HasModulateAlpha(void)     { return 1; }

/* ----------------------------------------------------------------------
 * std3D global flags / no-op API surface that the engine references.
 * ---------------------------------------------------------------------- */
int std3D_bReinitHudElements = 0;
int std3D_bInitted           = 0;

void std3D_ResetUIRenderList(void)            {}
void std3D_DrawMenu(void)                     {}
void std3D_DrawSceneFbo(void)                 {}
void std3D_DrawOverlay(void)                  {}
void std3D_FreeResources(void)                {}
void std3D_UpdateFrameCount(rdDDrawSurface *p)
                                              { (void)p; }
void std3D_RemoveTextureFromCacheList(rdDDrawSurface *p)
                                              { (void)p; }
void std3D_AddTextureToCacheList(rdDDrawSurface *p)
                                              { (void)p; }
void std3D_PurgeEntireTextureCache(void)      {}
void std3D_PurgeUIEntry(int i, int idx)       { (void)i; (void)idx; }
void std3D_PurgeSurfaceRefs(rdDDrawSurface *p) { (void)p; }
void std3D_UpdateSettings(void)               {}
/* Engine asks us to wipe the depth buffer mid-frame.  In FakeGL that's
 * just glClear(GL_DEPTH_BUFFER_BIT); the surrounding scene state stays
 * intact. */
void std3D_ClearZBuffer(void)
{
    glClear(GL_DEPTH_BUFFER_BIT);
}

#ifdef __cplusplus
} /* extern "C" */
#endif
