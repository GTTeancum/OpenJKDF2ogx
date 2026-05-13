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
#include <stdio.h>           /* sprintf for HUD text formatting */
#include <string.h>           /* memcpy */
#include <math.h>            /* tan() for projection-matrix setup */

extern int jkCutscene_isRendering;

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
typedef unsigned char GLboolean;
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
void __stdcall glFrustum      (GLdouble l, GLdouble r, GLdouble b,
                               GLdouble t, GLdouble n, GLdouble f);
void __stdcall glEnable       (GLenum cap);
void __stdcall glDisable      (GLenum cap);
void __stdcall glDepthFunc    (GLenum func);
void __stdcall glDepthMask    (GLboolean flag);
void __stdcall glCullFace     (GLenum mode);
void __stdcall glAlphaFunc    (GLenum func, GLfloat ref);

/* Texture entry points exposed by FakeGL (fakeglx.cpp:3302+).
 * NOTE: glBindTexture and glTexParameteri are declared in gl/gl.h but are
 * NOT defined at file scope in fakeglx.cpp — only as internal methods of
 * the FakeGL class.  We reach glBindTexture through the documented
 * wglGetProcAddress("glBindTextureEXT") path (fakeglx.cpp:3382-3397) and
 * use glTexParameterf for parameters (the only exposed variant).  This
 * keeps the third-party graft byte-identical per RENDERER_GRAFT.md. */
void __stdcall glTexImage2D   (GLenum target, GLint level, GLint internalformat,
                               GLsizei width, GLsizei height, GLint border,
                               GLenum format, GLenum type, const void *pixels);
void __stdcall glTexParameterf(GLenum target, GLenum pname, GLfloat param);

typedef void (__stdcall *PFN_glBindTextureEXT)(GLenum target, GLuint texture);
void * __stdcall wglGetProcAddress(const char *s);

#ifdef __cplusplus
} /* extern "C" */
#endif

#define GL_LINES               0x0001
#define GL_TRIANGLES           0x0004
#define GL_COLOR_BUFFER_BIT    0x00004000
#define GL_DEPTH_BUFFER_BIT    0x00000100
#define GL_STENCIL_BUFFER_BIT  0x00000400
#define GL_PROJECTION          0x1701
#define GL_MODELVIEW           0x1700
#define GL_TEXTURE_2D          0x0DE1
#define GL_DEPTH_TEST          0x0B71
#define GL_LESS                0x0201
#define GL_LEQUAL              0x0203
#define GL_GREATER             0x0204
#define GL_ALWAYS              0x0207
#define GL_FRONT               0x0404
#define GL_BACK                0x0405
#define GL_CULL_FACE           0x0B44
#define GL_BLEND               0x0BE2
#define GL_ALPHA_TEST          0x0BC0
#define GL_RGBA                0x1908
#define GL_UNSIGNED_BYTE       0x1401
#define GL_TEXTURE_MIN_FILTER  0x2801
#define GL_TEXTURE_MAG_FILTER  0x2800
#define GL_TEXTURE_WRAP_S      0x2802
#define GL_TEXTURE_WRAP_T      0x2803
#define GL_TEXTURE_BASE_LEVEL  0x813C
#define GL_TEXTURE_MAX_LEVEL   0x813D
#define GL_CLAMP_TO_EDGE       0x812F
#define GL_NEAREST             0x2600
#define GL_LINEAR              0x2601
#define GL_CLAMP               0x2900
#define GL_REPEAT              0x2901
#define GL_ONE                 1
#define GL_TRUE                1
#define GL_FALSE               0
#define GL_SRC_ALPHA           0x0302
#define GL_ONE_MINUS_SRC_ALPHA 0x0303

#ifdef __cplusplus
extern "C" {
#endif
void __stdcall glBlendFunc(GLenum sfactor, GLenum dfactor);
#ifdef __cplusplus
}
#endif

#ifdef __cplusplus
extern "C" {
#endif

/* ----------------------------------------------------------------------
 * Engine-side type forward declarations.
 *
 * These mirror the layouts in src/types.h byte-for-byte under the build
 * configuration the Xbox uses (SDL2_RENDER defined; OPTIMIZE_AWAY_UNUSED_
 * FIELDS undefined; RDMATERIAL_MINIMIZE_STRUCTS undefined).  Kept local
 * to this TU so we don't need to drag in types.h's full chain (Win32
 * conditionals etc).
 *
 * STAY IN SYNC with src/types.h:1015-1050 (rdDDrawSurface) and
 * src/types.h:1146-1168 (stdVBuffer).  If those change, the startup
 * sizeof() debug print below will diverge from the engine's view and
 * texture_loaded / texture_id writes will silently corrupt memory.
 *
 * Expected sizes (manually computed against types.h):
 *   sizeof(rdDDrawSurface) == 0xD8 (216) bytes
 *   sizeof(stdVBuffer)     == 0xD8 (216) bytes
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
    void                       *lpVtbl;                  /* 0x00 */
    unsigned int                direct3d_tex;            /* 0x04 */
    unsigned char               surface_desc[0x6c];      /* 0x08..0x73 */
    int                         texture_id;              /* 0x74 */
    int                         texture_loaded;          /* 0x78 */
    int                         is_16bit;                /* 0x7C */
    unsigned int                width;                   /* 0x80 */
    unsigned int                height;                  /* 0x84 */
    unsigned int                textureSize;             /* 0x88 */
    unsigned int                frameNum;                /* 0x8C */
    struct rdDDrawSurface_tag  *pPrevCachedTexture;      /* 0x90 */
    struct rdDDrawSurface_tag  *pNextCachedTexture;      /* 0x94 */
    /* SDL2_RENDER fields */
    unsigned int                emissive_texture_id;     /* 0x98 */
    unsigned int                displacement_texture_id; /* 0x9C */
    float                       emissive_factor[3];      /* 0xA0..0xAB */
    float                       albedo_factor[4];        /* 0xAC..0xBB */
    float                       displacement_factor;     /* 0xBC */
    void                       *emissive_data;           /* 0xC0 */
    void                       *albedo_data;             /* 0xC4 */
    void                       *displacement_data;       /* 0xC8 */
    void                       *pDataDepthConverted;     /* 0xCC */
    int                         skip_jkgm;               /* 0xD0 */
    void                       *cache_entry;             /* 0xD4 (jkgm_cache_entry_t*) */
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

/* stdVBufferTexFmt mirror (types.h:1078-1090, no RDMATERIAL_MINIMIZE_STRUCTS).
 * The Xbox build does NOT define RDMATERIAL_MINIMIZE_STRUCTS (that's gated
 * on TARGET_TWL in engine_config.h:298-335), so the full rdTexFormat is
 * embedded here. */
typedef struct
{
    int             width;                  /* 0x00 */
    int             height;                 /* 0x04 */
    unsigned int    texture_size_in_bytes;  /* 0x08 */
    unsigned int    width_in_bytes;         /* 0x0C */
    unsigned int    width_in_pixels;        /* 0x10 */
    /* rdTexFormat (types.h:1052-1068) */
    unsigned int    fmt_is16bit;            /* 0x14 */
    unsigned int    fmt_bpp;                /* 0x18 */
    unsigned int    fmt_r_bits;             /* 0x1C */
    unsigned int    fmt_g_bits;             /* 0x20 */
    unsigned int    fmt_b_bits;             /* 0x24 */
    unsigned int    fmt_r_shift;            /* 0x28 */
    unsigned int    fmt_g_shift;            /* 0x2C */
    unsigned int    fmt_b_shift;            /* 0x30 */
    unsigned int    fmt_r_bitdiff;          /* 0x34 */
    unsigned int    fmt_g_bitdiff;          /* 0x38 */
    unsigned int    fmt_b_bitdiff;          /* 0x3C */
    unsigned int    fmt_unk_40;             /* 0x40 */
    unsigned int    fmt_unk_44;             /* 0x44 */
    unsigned int    fmt_unk_48;             /* 0x48 */
} stdVBufferTexFmt;

typedef struct stdVBuffer_tag
{
    unsigned int        bSurfaceLocked;     /* 0x00 */
    unsigned int        lock_cnt;           /* 0x04 */
    unsigned int        gap8;               /* 0x08 */
    stdVBufferTexFmt    format;             /* 0x0C..0x57 (size 0x4C) */
    void               *palette;            /* 0x58  — uint8_t* RGB24, 256 entries */
    char               *surface_lock_alloc; /* 0x5C  — raw pixel data */
    unsigned int        transparent_color;  /* 0x60 */
    void               *sdl_or_ddraw;       /* 0x64  — union { rdDDrawSurface*; SDL_Surface*; } */
    void               *ddraw_palette;      /* 0x68 */
    unsigned char       desc[0x6c];         /* 0x6C..0xD7 */
} stdVBuffer;

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

/* glBindTexture proc cache — resolved once at Startup via
 * wglGetProcAddress("glBindTextureEXT") since FakeGL doesn't expose
 * a flat `glBindTexture` symbol at file scope. */
static PFN_glBindTextureEXT g_pfnBindTexture = 0;

/* UV out-of-range sentinel — accumulates across all engine vertex
 * submissions; published to HUD slot 14 from DrawRenderList (last
 * writer per frame, beats sithRender's RLG_summary ALPHADJ slot). */
static unsigned int g_uvOOR = 0;

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

    /* Layout sanity check.  Expected:
     *   sizeof(rdDDrawSurface) == 0xD8 (216)
     *   sizeof(stdVBuffer)     == 0xD8 (216)
     * If either differs from the engine's view, texture upload writes
     * will silently corrupt memory.  Print for visual comparison against
     * a known-good build (e.g. Win32/SDL2). */
    XDBGF("std3D_Startup: sizeof(rdDDrawSurface)=%u sizeof(stdVBuffer)=%u\n",
          (unsigned)sizeof(rdDDrawSurface), (unsigned)sizeof(stdVBuffer));
    std3D_DebugLineKV(0, "RDDSSZ", (int)sizeof(rdDDrawSurface));
    std3D_DebugLineKV(1, "VBUFSZ", (int)sizeof(stdVBuffer));

    /* ----------------------------------------------------------------
     * Milestone B diagnostic: upload one 64x64 red/cyan checkerboard
     * texture to FakeGL.  When STD3D_DIAG_DRAW_TEXTURED_TRI is set, a
     * triangle covering the right half of the screen renders in
     * std3D_Present each frame using this texture.  Proves the
     * upload→bind→sample pipeline before wiring engine geometry.
     *
     * Byte order is BGRA per FakeGL's ConvertToCompatablePixels
     * (fakeglx.cpp:2843-2856).  Red appears red → byte order correct.
     * Red appears cyan → byte order reversed.
     * ---------------------------------------------------------------- */
    {
        static unsigned char diag_tex[64 * 64 * 4];
        int x, y;
        for (y = 0; y < 64; ++y) {
            for (x = 0; x < 64; ++x) {
                int idx = (y * 64 + x) * 4;
                int sq  = ((x / 8) + (y / 8)) & 1;
                if (sq) {
                    /* Red: B=0, G=0, R=255, A=255 */
                    diag_tex[idx + 0] = 0x00;
                    diag_tex[idx + 1] = 0x00;
                    diag_tex[idx + 2] = 0xFF;
                    diag_tex[idx + 3] = 0xFF;
                } else {
                    /* Cyan: B=255, G=255, R=0, A=255 */
                    diag_tex[idx + 0] = 0xFF;
                    diag_tex[idx + 1] = 0xFF;
                    diag_tex[idx + 2] = 0x00;
                    diag_tex[idx + 3] = 0xFF;
                }
            }
        }

        /* Resolve glBindTextureEXT once.  This is the only path FakeGL
         * exposes for binding (fakeglx.cpp:3382-3397). */
        g_pfnBindTexture = (PFN_glBindTextureEXT)wglGetProcAddress("glBindTextureEXT");
        XDBGF("std3D_Startup: glBindTextureEXT proc = %p\n", (void*)g_pfnBindTexture);
        if (!g_pfnBindTexture) {
            XDBG("std3D_Startup: BindTexture proc resolution FAILED\n");
            std3D_DebugLine(2, "TEX1 NOPROC");
        } else {
            XDBG("std3D_Startup: uploading diag texture id=1\n");
            g_pfnBindTexture(GL_TEXTURE_2D, 1);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,    (GLfloat)GL_CLAMP);
            glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,    (GLfloat)GL_CLAMP);
            glTexImage2D(GL_TEXTURE_2D, 0, 4, 64, 64, 0,
                         GL_RGBA, GL_UNSIGNED_BYTE, diag_tex);
            XDBG("std3D_Startup: diag texture upload returned\n");
            std3D_DebugLine(2, "TEX1 OK");
        }
    }

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
/* Debug HUD — text overlay rendered as solid quads (one per lit pixel of */
/* a 3x5 bitmap font).  No textures, no texcoords; uses only the proven   */
/* glBegin/glColor/glVertex path.                                         */
/*                                                                        */
/* API:                                                                   */
/*   std3D_DebugLine(idx, "text")        — write text to slot idx         */
/*   std3D_DebugLineKV(idx, "KEY", val)  — formatted "KEY value"          */
/*                                                                        */
/* Lines persist across frames until overwritten.  Pass NULL or empty     */
/* string to blank a slot.  Lines are drawn down the left side of the     */
/* screen at y = 4 + idx * 8 px.                                          */
/* ====================================================================== */
#define DBG_NUM_LINES   24
#define DBG_LINE_LEN    31

static char dbg_lines[DBG_NUM_LINES][DBG_LINE_LEN + 1];
static unsigned int g_dbgFrameTick = 0;

void std3D_DebugLine(int idx, const char *text)
{
    int i = 0;
    if (idx < 0 || idx >= DBG_NUM_LINES) return;
    if (text)
        for (; i < DBG_LINE_LEN && text[i]; ++i)
            dbg_lines[idx][i] = text[i];
    dbg_lines[idx][i] = 0;
}

void std3D_DebugLineKV(int idx, const char *key, int value)
{
    char buf[DBG_LINE_LEN + 1];
    /* sprintf is fine on Xbox — full CRT linked. */
    sprintf(buf, "%s %d", key ? key : "", value);
    std3D_DebugLine(idx, buf);
}

/* Compatibility shims — old flag/counter calls now write text lines. */
void std3D_DebugFlag(int idx, int on)
{
    char buf[DBG_LINE_LEN + 1];
    sprintf(buf, "F%d %s", idx, on ? "ON" : "..");
    /* Reserve lines 0..7 for flags. */
    if (idx >= 0 && idx < 8) std3D_DebugLine(idx, buf);
}

void std3D_DebugCounter(int idx, int val, int max)
{
    char buf[DBG_LINE_LEN + 1];
    (void)max;
    sprintf(buf, "C%d %d", idx, val);
    /* Reserve lines 8..11 for counters. */
    if (idx >= 0 && idx < 4) std3D_DebugLine(8 + idx, buf);
}

static void dbg_quad(float x, float y, float w, float h,
                     float r, float g, float b)
{
    /* Two-triangle quad in ortho space.  z=0 is mid-cube under our wide
     * ortho, well inside [0,1] NDC after the projection. */
    glBegin(GL_TRIANGLES);
    glColor4f(r, g, b, 1.0f);
    glVertex3f(x,     y,     0.0f);
    glVertex3f(x + w, y,     0.0f);
    glVertex3f(x + w, y + h, 0.0f);

    glColor4f(r, g, b, 1.0f);
    glVertex3f(x,     y,     0.0f);
    glVertex3f(x + w, y + h, 0.0f);
    glVertex3f(x,     y + h, 0.0f);
    glEnd();
}

/* ----------------------------------------------------------------------
 * 3x5 bitmap font.  Each glyph is 5 rows; each row's low 3 bits encode
 * which columns are lit.  Data convention (matches the visual patterns
 * in the table — "100" = leftmost lit, value 4): bit 2 = LEFT column,
 * bit 0 = RIGHT column.  The draw loop tests `bits & (4 >> col)` so
 * col=0 (leftmost) tests bit 2, col=2 (rightmost) tests bit 0.
 *
 * Drawn at scale=2 → glyph is 6x10 px on screen, with 1 px col gap.
 * ---------------------------------------------------------------------- */
static const unsigned char DBG_FONT_UNK[5] = {7,5,5,5,7}; /* fallback box */

static const unsigned char *dbg_font_glyph(char c)
{
    /* Digits */
    static const unsigned char F0[5] = {7,5,5,5,7};
    static const unsigned char F1[5] = {2,6,2,2,7};
    static const unsigned char F2[5] = {7,1,2,4,7};
    static const unsigned char F3[5] = {7,1,3,1,7};
    static const unsigned char F4[5] = {5,5,7,1,1};
    static const unsigned char F5[5] = {7,4,7,1,7};
    static const unsigned char F6[5] = {7,4,7,5,7};
    static const unsigned char F7[5] = {7,1,2,2,2};
    static const unsigned char F8[5] = {7,5,7,5,7};
    static const unsigned char F9[5] = {7,5,7,1,7};
    /* Letters */
    static const unsigned char FA[5] = {2,5,7,5,5};
    static const unsigned char FB[5] = {6,5,6,5,6};
    static const unsigned char FC[5] = {3,4,4,4,3};
    static const unsigned char FD[5] = {6,5,5,5,6};
    static const unsigned char FE[5] = {7,4,6,4,7};
    static const unsigned char FF[5] = {7,4,6,4,4};
    static const unsigned char FG[5] = {3,4,5,5,3};
    static const unsigned char FH[5] = {5,5,7,5,5};
    static const unsigned char FI[5] = {7,2,2,2,7};
    static const unsigned char FJ[5] = {1,1,1,5,2};
    static const unsigned char FK[5] = {5,6,4,6,5};
    static const unsigned char FL[5] = {4,4,4,4,7};
    static const unsigned char FM[5] = {5,7,7,5,5};
    static const unsigned char FN[5] = {6,5,5,5,5};
    static const unsigned char FO[5] = {2,5,5,5,2};
    static const unsigned char FP[5] = {6,5,6,4,4};
    static const unsigned char FQ[5] = {2,5,5,6,3};
    static const unsigned char FR[5] = {6,5,6,6,5};
    static const unsigned char FS[5] = {3,4,2,1,6};
    static const unsigned char FT[5] = {7,2,2,2,2};
    static const unsigned char FU[5] = {5,5,5,5,2};
    static const unsigned char FV[5] = {5,5,5,2,2};
    static const unsigned char FW[5] = {5,5,7,7,5};
    static const unsigned char FX[5] = {5,5,2,5,5};
    static const unsigned char FY[5] = {5,5,2,2,2};
    static const unsigned char FZ[5] = {7,1,2,4,7};
    /* Punctuation */
    static const unsigned char SPC[5]  = {0,0,0,0,0};
    static const unsigned char COL[5]  = {0,2,0,2,0};   /* :  */
    static const unsigned char DSH[5]  = {0,0,7,0,0};   /* -  */
    static const unsigned char EQ[5]   = {0,7,0,7,0};   /* =  */
    static const unsigned char SLS[5]  = {1,1,2,4,4};   /* /  */
    static const unsigned char DOT[5]  = {0,0,0,0,2};   /* .  */

    /* Lowercase folds to upper. */
    if (c >= 'a' && c <= 'z') c -= 32;

    switch (c)
    {
    case '0': return F0; case '1': return F1; case '2': return F2;
    case '3': return F3; case '4': return F4; case '5': return F5;
    case '6': return F6; case '7': return F7; case '8': return F8;
    case '9': return F9;
    case 'A': return FA; case 'B': return FB; case 'C': return FC;
    case 'D': return FD; case 'E': return FE; case 'F': return FF;
    case 'G': return FG; case 'H': return FH; case 'I': return FI;
    case 'J': return FJ; case 'K': return FK; case 'L': return FL;
    case 'M': return FM; case 'N': return FN; case 'O': return FO;
    case 'P': return FP; case 'Q': return FQ; case 'R': return FR;
    case 'S': return FS; case 'T': return FT; case 'U': return FU;
    case 'V': return FV; case 'W': return FW; case 'X': return FX;
    case 'Y': return FY; case 'Z': return FZ;
    case ' ': return SPC;
    case ':': return COL;
    case '-': return DSH;
    case '=': return EQ;
    case '/': return SLS;
    case '.': return DOT;
    default:  return DBG_FONT_UNK;
    }
}

static void dbg_text(float x, float y, float scale,
                     float r, float g, float b, const char *s)
{
    if (!s) return;
    while (*s)
    {
        const unsigned char *g_ = dbg_font_glyph(*s);
        int row, col;
        for (row = 0; row < 5; ++row)
        {
            unsigned char bits = g_[row];
            for (col = 0; col < 3; ++col)
            {
                /* col=0 = leftmost = bit 2 (4 >> 0).
                 * col=2 = rightmost = bit 0 (4 >> 2). */
                if (bits & (4 >> col))
                    dbg_quad(x + col * scale,
                             y + row * scale,
                             scale, scale, r, g, b);
            }
        }
        x += 4.0f * scale;  /* 3px glyph + 1px gap */
        ++s;
    }
}

/* FPS counter: GetTickCount-based, sampled over a 1-second window.
 * Drawn in the top-left as a single line "FPS NN".  The verbose 24-slot
 * debug HUD is suppressed by default — set STD3D_DEBUG_HUD to a non-zero
 * value to re-enable the full overlay for diagnosis. */
#ifndef STD3D_DEBUG_HUD
#define STD3D_DEBUG_HUD 0
#endif

static unsigned int g_fps_frameCount = 0;
static unsigned int g_fps_windowStartMs = 0;
static int          g_fps_current = 0;

static void std3D_DrawDebugHUD(void)
{
    char fpsBuf[16];
    unsigned int nowMs = (unsigned int)GetTickCount();
    unsigned int elapsed;

    /* Roll the 1-second sampling window. */
    if (g_fps_windowStartMs == 0) g_fps_windowStartMs = nowMs;
    g_fps_frameCount++;
    elapsed = nowMs - g_fps_windowStartMs;
    if (elapsed >= 1000) {
        g_fps_current = (int)((g_fps_frameCount * 1000) / elapsed);
        g_fps_frameCount = 0;
        g_fps_windowStartMs = nowMs;
    }

    /* Single-line FPS readout, top-left corner. */
    sprintf(fpsBuf, "FPS %d", g_fps_current);
    dbg_text(4.0f, 4.0f, 2.0f, 1.0f, 1.0f, 0.2f, fpsBuf);

#if STD3D_DEBUG_HUD
    {
        int i;
        float sweep;
        for (i = 0; i < DBG_NUM_LINES; ++i) {
            if (dbg_lines[i][0])
                dbg_text(4.0f, 24.0f + i * 12.0f, 2.0f,
                         1.0f, 1.0f, 1.0f, dbg_lines[i]);
        }
        g_dbgFrameTick++;
        sweep = (float)(g_dbgFrameTick % 120) / 120.0f;
        dbg_quad(0.0f,  24.0f + DBG_NUM_LINES * 12.0f, 220.0f, 4.0f,
                 0.1f, 0.1f, 0.1f);
        dbg_quad(sweep * 208.0f, 24.0f + DBG_NUM_LINES * 12.0f,
                 12.0f, 4.0f, 1.0f, 1.0f, 0.0f);
    }
#endif
}

/* ====================================================================== */
/* std3D_StartScene                                                       */
/* ====================================================================== */
/* The engine has nested StartScene/EndScene calls — main_xbox.c wraps an
 * outer pair around jkMain_GuiAdvance, which itself contains inner pairs
 * (rdCache, Window).  Only the OUTER (closed→open) transition should
 * glClear; inner re-entries are no-ops for state but still need the HUD
 * to advance, otherwise it freezes on the first frame. */
static unsigned int g_startCalls     = 0;
static unsigned int g_clearCalls     = 0;
static unsigned int g_endCalls       = 0;
static unsigned int g_endTransitions = 0;

/* Per-frame vertex bbox accumulator — populated by AddRenderListVertices,
 * reset by outer StartScene, published to HUD by Present. */
static float g_bboxXmin =  1e30f, g_bboxXmax = -1e30f;
static float g_bboxYmin =  1e30f, g_bboxYmax = -1e30f;
static float g_bboxZmin =  1e30f, g_bboxZmax = -1e30f;
static unsigned int g_firstColor = 0;
static int g_bboxValid = 0;

int std3D_StartScene(void)
{
    if (!g_initialized) return 1;
    g_startCalls++;

    /* Engine has nested StartScene/EndScene cycles.  Only the outer
     * (closed→open) transition issues glClear; nested calls are no-ops
     * for state.  HUD does NOT draw here — see std3D_Present. */
    if (!g_sceneOpen)
    {
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT | GL_STENCIL_BUFFER_BIT);
        g_clearCalls++;
        g_sceneOpen     = 1;
        GL_numVertices  = 0;
        GL_verticesDone = 0;
        GL_numTris      = 0;
        GL_numLines     = 0;

        /* Reset per-frame vertex-bbox accumulators on outer transition. */
        g_bboxXmin =  1e30f; g_bboxXmax = -1e30f;
        g_bboxYmin =  1e30f; g_bboxYmax = -1e30f;
        g_bboxZmin =  1e30f; g_bboxZmax = -1e30f;
        g_firstColor = 0;
        g_bboxValid  = 0;
    }
    return 1;
}

/* ====================================================================== */
/* std3D_EndScene                                                         */
/* ====================================================================== */
int std3D_EndScene(void)
{
    if (!g_initialized) return 1;
    g_endCalls++;
    if (!g_sceneOpen) return 1;

    /* No EndScene call here — FakeSwapBuffers does internalEnd +
     * EndScene + Present in one step (gl_fakegl.cpp:2557-2570). */
    g_endTransitions++;
    g_sceneOpen     = 0;
    GL_numVertices  = 0;
    GL_verticesDone = 0;
    GL_numTris      = 0;
    GL_numLines     = 0;
    return 1;
}

/* ====================================================================== */
/* std3D_Present                                                          */
/*                                                                        */
/* Drawing the debug HUD here, just before swap, decouples it entirely    */
/* from the engine's nested StartScene/EndScene flow.  The engine has     */
/* already finished submitting per-frame geometry by the time we get      */
/* here (outer EndScene closed the scene block).  We then:                */
/*                                                                        */
/*   1. Restore xquake's GL_Set2D state (gl_draw.c:920) — ortho proj,     */
/*      modelview identity, depth/cull/blend off.  The engine has likely  */
/*      changed projection to perspective for its 3D draws.               */
/*   2. Update the live STARTS/CLEARS/ENDS/ENDTR text lines.              */
/*   3. Draw the HUD as solid quads via glBegin/glEnd.                    */
/*   4. FakeSwapBuffers — does internalEnd + EndScene + Present.          */
/*                                                                        */
/* The lazy BeginScene in FakeGL fires automatically on our first glBegin */
/* if the scene block was already closed, and FakeSwapBuffers' EndScene   */
/* closes it cleanly afterwards.                                          */
/* ====================================================================== */
static unsigned int g_presentCalls = 0;

void std3D_Present(void)
{
    char buf[32];
    if (!g_initialized) { XDBG("std3D_Present: not initialized\n"); return; }
    g_presentCalls++;

    if (!jkCutscene_isRendering) {
        /* Publish vertex bbox to HUD slots 5/6/7.  Format: "VX  min max". */
        if (g_bboxValid) {
            sprintf(buf, "VX %d %d", (int)g_bboxXmin, (int)g_bboxXmax);
            std3D_DebugLine(5, buf);
            sprintf(buf, "VY %d %d", (int)g_bboxYmin, (int)g_bboxYmax);
            std3D_DebugLine(6, buf);
            sprintf(buf, "VZ %d %d", (int)g_bboxZmin, (int)g_bboxZmax);
            std3D_DebugLine(7, buf);
            sprintf(buf, "COL %X", g_firstColor);
            std3D_DebugLine(15, buf);  /* repurpose SPHEROUT slot — always 0 */
        } else {
            std3D_DebugLine(5, "VX NONE");
            std3D_DebugLine(6, "");
            std3D_DebugLine(7, "");
        }
    }

    /* Restore 2D state for HUD draw — engine left projection in
     * perspective + modelview = view matrix. */
    glViewport(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 640.0, 480.0, 0.0, -99999.0, 99999.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_BLEND);
    glDisable(GL_ALPHA_TEST);

    /* ----------------------------------------------------------------
     * STD3D_DIAG_DRAW_TEXTURED_TRI: draw a triangle covering the right
     * half of the screen using the diagnostic checkerboard texture
     * uploaded in std3D_Startup.  Verifies the upload→bind→sample
     * pipeline before wiring engine geometry through textures.
     *
     * Pairs glEnable(GL_TEXTURE_2D) + glBindTexture + glTexCoord2f as
     * a unit — the inline-mode FakeGL path drops primitives if texcoord
     * is written without a bound texture (fakeglx.cpp:1403-1409).
     * ---------------------------------------------------------------- */
#define STD3D_DIAG_DRAW_TEXTURED_TRI 0
#if STD3D_DIAG_DRAW_TEXTURED_TRI
    if (g_pfnBindTexture) {
        glEnable(GL_TEXTURE_2D);
        g_pfnBindTexture(GL_TEXTURE_2D, 1);
        glBegin(GL_TRIANGLES);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glTexCoord2f(0.5f, 0.0f);  glVertex3f(450.0f,  60.0f, 0.0f);
        glTexCoord2f(1.0f, 1.0f);  glVertex3f(620.0f, 420.0f, 0.0f);
        glTexCoord2f(0.0f, 1.0f);  glVertex3f(280.0f, 420.0f, 0.0f);
        glEnd();
        glDisable(GL_TEXTURE_2D);
    } else {
        glDisable(GL_TEXTURE_2D);
    }
#else
    glDisable(GL_TEXTURE_2D);
#endif

    if (!jkCutscene_isRendering) {
        /* Live counters into the HUD text. */
        std3D_DebugLineKV(19, "STARTS",  g_startCalls);
        std3D_DebugLineKV(20, "CLEARS",  g_clearCalls);
        std3D_DebugLineKV(21, "ENDS",    g_endCalls);
        std3D_DebugLineKV(22, "ENDTR",   g_endTransitions);
        std3D_DebugLineKV(23, "PRESENT", g_presentCalls);

        std3D_DrawDebugHUD();
    }

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
    int i;
    { static int _av=0; if(_av<5){ XDBGF("ARV: count=%d GL_nverts=%d\n", count, GL_numVertices); _av++; } }
    if (!verts || count <= 0) return 1;
    if (GL_numVertices + count >= STD3D_MAX_VERTICES) return 0;
    memcpy(&GL_tmpVertices[GL_numVertices], verts, count * sizeof(D3DVERTEX));

    /* Walk the new batch and accumulate bbox + sample first color +
     * UV out-of-range sentinel.  UVs in JK are normalized [0,1]; values
     * |tu|>2 || |tv|>2 are evidence of clip-space leakage or unit-mismatch
     * (e.g. the TWL pixel-space-UV path firing on Xbox by mistake). */
    for (i = 0; i < count; ++i)
    {
        float tu = verts[i].tu, tv = verts[i].tv;
        if (!g_bboxValid) {
            g_firstColor = verts[i].color;
            g_bboxValid  = 1;
        }
        if (verts[i].x < g_bboxXmin) g_bboxXmin = verts[i].x;
        if (verts[i].x > g_bboxXmax) g_bboxXmax = verts[i].x;
        if (verts[i].y < g_bboxYmin) g_bboxYmin = verts[i].y;
        if (verts[i].y > g_bboxYmax) g_bboxYmax = verts[i].y;
        if (verts[i].z < g_bboxZmin) g_bboxZmin = verts[i].z;
        if (verts[i].z > g_bboxZmax) g_bboxZmax = verts[i].z;
        if (tu > 2.0f || tu < -1.0f || tv > 2.0f || tv < -1.0f)
            g_uvOOR++;
    }

    GL_numVertices += count;
    std3D_DebugLineKV(8, "VERTS", GL_numVertices);
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
    std3D_DebugLineKV(9, "TRIS", GL_numTris);
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
/*                                                                        */
/* Engine flow:                                                           */
/*   rdCache stages vertices (D3DVERTEX) + tri indices (rdTri) via        */
/*   AddRenderListVertices/Tris, then calls DrawRenderList.  We translate */
/*   each tri into a glBegin(GL_TRIANGLES) ... glEnd block, emitting per- */
/*   vertex glColor4f / glTexCoord2f / glVertex3f.                        */
/*                                                                        */
/* Texture binding is currently disabled (cache is stubbed; tris draw as  */
/* colored polygons via vertex DIFFUSE).  When the texture cache is wired */
/* up to glTexImage2D + glBindTexture this becomes the path that handles  */
/* texture state changes per tri.                                         */
/* ====================================================================== */
/* Bridge to engine globals — see xbox_world_helper.cpp.  Filled by
 * the helper from rdCamera_pCurCamera each frame. */
extern int xbox_get_camera_params(float *fov, float *aspect,
                                  float *znear, float *zfar);

void std3D_DrawRenderList(void)
{
    int i;
    if (!g_initialized || !g_sceneOpen) return;
    if (GL_numTris == 0 || !GL_verticesDone) return;
    std3D_DebugLineKV(10, "DRAWN", GL_numTris);

    /* ---------------------------------------------------------------- */
    /* HW projection setup.                                              */
    /*                                                                   */
    /* The engine's rdCamera_PerspProject is now a pass-through on Xbox  */
    /* (rdCamera.c:405), so the vertices arriving here are CAMERA-space  */
    /* (not screen-space): engine convention x=right, y=depth/forward,   */
    /* z=up.  We feed them to glVertex3f with a YZ swap & Z-flip below   */
    /* to match GL convention (x=right, y=up, -z=forward), and set up    */
    /* glFrustum for the projection so D3D performs perspective-correct  */
    /* UV interpolation via the GPU's per-pixel divide.                  */
    /*                                                                   */
    /* Math derivation (matches engine's CPU projection in PerspProject  */
    /* for a 640x480 / 90deg horizontal / 4:3 camera):                   */
    /*   tan_h = tan(fov/2)                                              */
    /*   tan_v = tan_h * screenAspectRatio   (engine uses height/width)  */
    /*   right = znear * tan_h ; top = znear * tan_v                     */
    /*   glFrustum(-right, right, -top, top, znear, zfar)                */
    /* ---------------------------------------------------------------- */
    {
        float cam_fov = 90.0f, cam_aspect = 0.75f;
        float cam_znear = 1.0f/64.0f, cam_zfar = 128.0f;
        float tan_h, tan_v, half_w, half_h;
        xbox_get_camera_params(&cam_fov, &cam_aspect, &cam_znear, &cam_zfar);

        tan_h  = (float)tan((double)cam_fov * (3.14159265358979 / 360.0));
        tan_v  = tan_h * cam_aspect;
        half_w = cam_znear * tan_h;
        half_h = cam_znear * tan_v;

        glViewport(0, 0, 640, 480);
        glMatrixMode(GL_PROJECTION);
        glLoadIdentity();
        glFrustum(-half_w, half_w, -half_h, half_h, cam_znear, cam_zfar);
        glMatrixMode(GL_MODELVIEW);
        glLoadIdentity();

        { static int _pl=0; if(_pl<3){
            XDBGF("DRL projection: fov=%.2f aspect=%.4f znear=%.6f zfar=%.2f -> tan_h=%.4f tan_v=%.4f\n",
                  cam_fov, cam_aspect, cam_znear, cam_zfar, tan_h, tan_v);
            _pl++;
        }}
    }

    /* Match upstream Platform/GL/std3D.c's rdTri.flags contract:
     * 0x600 controls alpha blending, 0x800 depth test, 0x1000 depth write,
     * and 0x10000 cull orientation for double-sided lit faces. */
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_BLEND);
    glEnable(GL_CULL_FACE);
    glDepthFunc(GL_LESS);
    glDepthMask(GL_TRUE);
    glCullFace(GL_FRONT);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

    /* Alpha test: discard pixels with alpha < 128/255 so chroma-keyed
     * textures (sector-adjoin grates, fence textures, etc.) don't punch
     * their z into the depth buffer for transparent texels.  Without
     * this, depth-test + textured-with-alpha = the transparent texels
     * still occlude back-sector geometry — sector adjoins read as
     * opaque black.  ALPHA_TEST is fragment-discard; passes don't get
     * a depth write, so adjacent sectors render correctly through. */
    glEnable(GL_ALPHA_TEST);
    glAlphaFunc(GL_GREATER, 0.5f);

    {
        static int _f = 0;
        if (_f < 3)
        {
            XDBGF("DRL: nTris=%d nVerts=%d\n", GL_numTris, GL_numVertices);
            _f++;
        }
    }

    /* One glBegin/glEnd per triangle.  FakeGL's USE_BEGINEND path on Xbox
     * (gl_fakegl.cpp:1380-1432) submits each Begin/End as its own NV2A
     * inline-mode primitive — no buffering between draws.
     *
     * IMPORTANT: never batch multiple tris into a single glBegin(GL_TRIANGLES)
     * block — fakeglx.cpp:1416 has an off-by-one (`drawMode+1`) that maps
     * GL_TRIANGLES(4)→D3DPT_TRIANGLESTRIP(5).  3-vertex submissions render
     * identically under either type; longer batches would stripify wrong.
     *
     * STD3D_WIREFRAME (1=GL_LINES outline / 0=textured fill).
     * STD3D_FORCE_WHITE_UNTEX: draw untextured tris in white instead of
     * the engine's vertex DIFFUSE (often 0xFF000000 = invisible) so
     * surfaces missing textures stay diagnosable. */
#define STD3D_WIREFRAME       0
#define STD3D_FORCE_WHITE_UNTEX 1
/* Diagnostic: re-emit each tri as a green line overlay after the textured
 * pass.  Confirmed (via hardware test): warp is UV/affine, not tri-level. */
#define STD3D_WIREFRAME_OVERLAY 0

/* View-space → GL-space convention swap.
 *   engine: x=right, y=depth (forward into screen), z=up
 *   GL:     x=right, y=up,                          z=back (so -z=forward)
 * Reorder coords at the glVertex3f call site so the GPU's projection
 * matrix (set up via glFrustum above) does the right thing. */
#define V3F_ENGINE_TO_GL(v) glVertex3f((v)->x, (v)->z, -(v)->y)
    {
        int textured_tris = 0, untextured_tris = 0, bind_switches = 0;
        unsigned int last_id = 0;
        int last_flags = -1;

    for (i = 0; i < GL_numTris; ++i)
    {
        rdTri      *t  = &GL_tmpTris[i];
        D3DVERTEX  *a  = &GL_tmpVertices[t->v1];
        D3DVERTEX  *b  = &GL_tmpVertices[t->v2];
        D3DVERTEX  *c  = &GL_tmpVertices[t->v3];
#if STD3D_WIREFRAME
        glBegin(GL_LINES);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        V3F_ENGINE_TO_GL(a);
        V3F_ENGINE_TO_GL(b);
        V3F_ENGINE_TO_GL(b);
        V3F_ENGINE_TO_GL(c);
        V3F_ENGINE_TO_GL(c);
        V3F_ENGINE_TO_GL(a);
#else
        {
            int textured = (t->texture
                            && t->texture->texture_loaded
                            && t->texture->texture_id != 0
                            && g_pfnBindTexture);

            if (textured) {
                if (t->flags != last_flags) {
                    glEnable(GL_BLEND);
                    glEnable(GL_CULL_FACE);

                    if (t->flags & 0x800) glDepthFunc(GL_LESS);
                    else                  glDepthFunc(GL_ALWAYS);

                    if (t->flags & 0x1000) glDepthMask(GL_TRUE);
                    else                   glDepthMask(GL_FALSE);

                    if (t->flags & 0x10000) glCullFace(GL_BACK);
                    else                    glCullFace(GL_FRONT);

                    if (t->flags & 0x600) glBlendFunc(GL_ONE, GL_ONE_MINUS_SRC_ALPHA);
                    else                  glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);

                    last_flags = t->flags;
                }
            } else if (last_flags != -2) {
                glDisable(GL_BLEND);
                glDisable(GL_CULL_FACE);
                glDepthFunc(GL_LEQUAL);
                glDepthMask(GL_TRUE);
                last_flags = -2;
            }

            if (textured) {
                unsigned int id = (unsigned int)t->texture->texture_id;
                if (id != last_id) {
                    g_pfnBindTexture(GL_TEXTURE_2D, id);
                    last_id = id;
                    bind_switches++;
                }
                glEnable(GL_TEXTURE_2D);
                textured_tris++;
            } else {
                glDisable(GL_TEXTURE_2D);
                last_id = 0;
                untextured_tris++;
            }

            glBegin(GL_TRIANGLES);
            /* Per-vertex lighting modulation.
             *
             * The engine writes a [0..1] monochrome light value into
             * D3DVERTEX.lightLevel for each vertex (rdCache.c:823 on
             * SDL2_RENDER, mixing sector ambient / surface
             * light_level_static / per-vertex vertexIntensities[]).
             * The .color field on the same vertex SHOULD contain a
             * full RGB lit color (rdCache.c:935) but in practice it's
             * very often 0xFF000000 on JK1 surfaces — palette colormap
             * tint is identity on the basic NarShaddaa walls so the
             * R/G/B channels never get bumped above 0.  Using .color
             * directly therefore zeroes the texture out.
             *
             * Switch to multiplying TEXTURE * lightLevel (monochrome).
             * For untextured polys (no material at all) emit pure
             * lightLevel so they show up as gray-scale.  D3DTOP_MODULATE
             * is the default texture-stage op so the GPU does the
             * COLOR * TEXTURE blend per pixel for free.
             *
             * TODO when JKM_LIGHTING is on we'll want to use .color so
             * coloured lights work; for now JK1 is monochrome and the
             * .color field can't be trusted. */
            /* DIFFUSE source for the vertex.
             *
             * Untextured ("solid color") faces: rdCache writes the palette-
             * resolved ARGB into vertex.color (rdCache.c:1346 mirrors
             * upstream rdCache.c:1346).  That's how bryar bolts get their
             * red, sky polys get their colour, and any color-only .mat
             * surface gets its tint.  USE .color in this case.
             *
             * Textured faces: rdCache may leave .color at 0xFF000000 on JK1
             * because the palette tint path doesn't bump R/G/B for identity
             * colormaps (long comment at rdCache.c:351-362 explains).  In
             * that case, fall back to monochrome lightLevel so texture
             * modulation produces a lit colour.  If .color does carry real
             * lighting (MOTS JKM_LIGHTING, coloured lights), use that.
             *
             * Decode: vertex.color is ARGB8888 — see rdCache.c:1345 packing.
             */
#define V_COLOR_R(v) (float)(((v)->color >> 16) & 0xFF) / 255.0f
#define V_COLOR_G(v) (float)(((v)->color >>  8) & 0xFF) / 255.0f
#define V_COLOR_B(v) (float)( (v)->color        & 0xFF) / 255.0f
#define V_COLOR_A(v) (float)((((v)->color >> 24) & 0xFF) ? (((v)->color >> 24) & 0xFF) : 0xFF) / 255.0f
#define V_COLOR_RGB_NONZERO(v) (((v)->color & 0x00FFFFFFu) != 0u)
            if (textured) {
                if (V_COLOR_RGB_NONZERO(a)) glColor4f(V_COLOR_R(a), V_COLOR_G(a), V_COLOR_B(a), V_COLOR_A(a));
                else                        glColor4f(a->lightLevel, a->lightLevel, a->lightLevel, V_COLOR_A(a));
                glTexCoord2f(a->tu, a->tv);
                V3F_ENGINE_TO_GL(a);
                if (V_COLOR_RGB_NONZERO(b)) glColor4f(V_COLOR_R(b), V_COLOR_G(b), V_COLOR_B(b), V_COLOR_A(b));
                else                        glColor4f(b->lightLevel, b->lightLevel, b->lightLevel, V_COLOR_A(b));
                glTexCoord2f(b->tu, b->tv);
                V3F_ENGINE_TO_GL(b);
                if (V_COLOR_RGB_NONZERO(c)) glColor4f(V_COLOR_R(c), V_COLOR_G(c), V_COLOR_B(c), V_COLOR_A(c));
                else                        glColor4f(c->lightLevel, c->lightLevel, c->lightLevel, V_COLOR_A(c));
                glTexCoord2f(c->tu, c->tv);
                V3F_ENGINE_TO_GL(c);
            } else {
                if (V_COLOR_RGB_NONZERO(a)) glColor4f(V_COLOR_R(a), V_COLOR_G(a), V_COLOR_B(a), 1.0f);
                else                        glColor4f(a->lightLevel, a->lightLevel, a->lightLevel, 1.0f);
                V3F_ENGINE_TO_GL(a);
                if (V_COLOR_RGB_NONZERO(b)) glColor4f(V_COLOR_R(b), V_COLOR_G(b), V_COLOR_B(b), 1.0f);
                else                        glColor4f(b->lightLevel, b->lightLevel, b->lightLevel, 1.0f);
                V3F_ENGINE_TO_GL(b);
                if (V_COLOR_RGB_NONZERO(c)) glColor4f(V_COLOR_R(c), V_COLOR_G(c), V_COLOR_B(c), 1.0f);
                else                        glColor4f(c->lightLevel, c->lightLevel, c->lightLevel, 1.0f);
                V3F_ENGINE_TO_GL(c);
            }
#undef V_COLOR_R
#undef V_COLOR_G
#undef V_COLOR_B
#undef V_COLOR_A
#undef V_COLOR_RGB_NONZERO
        }
#endif

        glEnd();
    }
#if !STD3D_WIREFRAME
        glDisable(GL_TEXTURE_2D);
        std3D_DebugLineKV(2, "TXTRI", textured_tris);
        std3D_DebugLineKV(3, "UNTRI", untextured_tris);
        std3D_DebugLineKV(4, "BIND",  bind_switches);
        std3D_DebugLineKV(14, "UVOOR", (int)g_uvOOR);
    }
#endif

#if STD3D_WIREFRAME_OVERLAY && !STD3D_WIREFRAME
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_DEPTH_TEST);
    for (i = 0; i < GL_numTris; ++i) {
        rdTri      *t  = &GL_tmpTris[i];
        D3DVERTEX  *a  = &GL_tmpVertices[t->v1];
        D3DVERTEX  *b  = &GL_tmpVertices[t->v2];
        D3DVERTEX  *c  = &GL_tmpVertices[t->v3];
        glBegin(GL_LINES);
        glColor4f(0.0f, 1.0f, 0.0f, 1.0f);
        V3F_ENGINE_TO_GL(a); V3F_ENGINE_TO_GL(b);
        V3F_ENGINE_TO_GL(b); V3F_ENGINE_TO_GL(c);
        V3F_ENGINE_TO_GL(c); V3F_ENGINE_TO_GL(a);
        glEnd();
    }
    glEnable(GL_DEPTH_TEST);
#endif

    GL_numVertices  = 0;
    GL_verticesDone = 0;
    GL_numTris      = 0;
    GL_numLines     = 0;
}

/* ====================================================================== */
/* Texture cache.                                                         */
/*                                                                        */
/* Each call to std3D_AddToTextureCache uploads a texture to FakeGL via   */
/* glTexImage2D, allocating a fresh GL texture id from a monotonic        */
/* counter (starting at 2 — id 1 is reserved for the diag checkerboard). */
/* The id is written to texture->texture_id (offset 0x74 in the real     */
/* rdDDrawSurface layout per types.h:1022) and texture->texture_loaded   */
/* is set so the engine won't re-call AddToTextureCache for this surface.*/
/*                                                                        */
/* Byte order is RGBA in glTexImage2D (verified empirically in            */
/* Milestone B — the diag checkerboard rendered with bytes interpreted    */
/* as R,G,B,A regardless of the BGRA documentation in fakeglx.cpp:2843).  */
/*                                                                        */
/* Source data: vbuf->surface_lock_alloc (raw pixels), vbuf->format       */
/* (dimensions, bpp, 16bit flag), vbuf->palette (per-mip 8bpp palette as  */
/* rdColor24*; assigned at rdMaterial.c:787 right before this call).      */
/* If vbuf->palette is NULL we fall back to g_pCurrentPalette set via     */
/* std3D_SetCurrentPalette, mirroring Platform/GL/std3D.c:3017.           */
/* ====================================================================== */
typedef struct rdColor24_local {
    unsigned char r, g, b;
} rdColor24_local;

static unsigned int    g_nextTexId      = 2;   /* id 1 = diag checkerboard */
static unsigned int    g_texUploaded    = 0;
static unsigned int    g_texFailed      = 0;
static rdColor24_local *g_pCurrentPalette = 0;

/* Scratch buffer for per-texture RGBA conversion.  Max source we expect
 * is 256x256 (most JK textures ≤ 128x128).  Buffer is scratch — FakeGL's
 * glTexImage2D copies pixels into D3D8 texture memory, so we can reuse
 * this between uploads. */
#define STD3D_TEX_SCRATCH_W  1024
#define STD3D_TEX_SCRATCH_H  512
static unsigned char g_texScratch[STD3D_TEX_SCRATCH_W * STD3D_TEX_SCRATCH_H * 4];

int std3D_AddToTextureCache(stdVBuffer *vbuf, rdDDrawSurface *texture,
                             int is_alpha_tex, int no_mip)
{
    unsigned int       w, h, i, total;
    const unsigned char *src;
    rdColor24_local     *pal;
    unsigned int        id;
    (void)no_mip;

    /* Per-reason failure counters for diagnostic.  First N of each are
     * also logged so we can see actual values that triggered failure. */
    static unsigned int fail_nullin = 0, fail_baddim = 0, fail_16bit = 0;
    static unsigned int fail_nopal  = 0, fail_nobind = 0;
    static int          fail_logN   = 0;
    static unsigned int call_count  = 0;
    call_count++;
    if ((call_count % 200) == 1) {
        XDBGF("ATC stats: calls=%u TUP=%u TFAIL=%u (nullin=%u baddim=%u 16bit=%u nopal=%u nobind=%u)\n",
              call_count, g_texUploaded, g_texFailed,
              fail_nullin, fail_baddim, fail_16bit, fail_nopal, fail_nobind);
    }

    if (!texture || !vbuf || !vbuf->surface_lock_alloc) {
        if (fail_logN < 6) {
            XDBGF("ATC fail nullin: tex=%p vbuf=%p src=%p\n",
                  (void*)texture, (void*)vbuf,
                  vbuf ? (void*)vbuf->surface_lock_alloc : (void*)0);
            fail_logN++;
        }
        fail_nullin++; g_texFailed++;
        std3D_DebugLineKV(0, "TFAIL", g_texFailed);
        return 0;
    }

    w = vbuf->format.width_in_pixels;
    h = (unsigned int)vbuf->format.height;
    if (w == 0 || h == 0 || w > STD3D_TEX_SCRATCH_W || h > STD3D_TEX_SCRATCH_H) {
        if (fail_logN < 6) {
            XDBGF("ATC fail baddim: vbuf=%p w=%u h=%u (in_pixels=%u in_bytes=%u fmt.w=%d fmt.h=%d)\n",
                  (void*)vbuf, w, h,
                  vbuf->format.width_in_pixels, vbuf->format.width_in_bytes,
                  vbuf->format.width, vbuf->format.height);
            fail_logN++;
        }
        fail_baddim++; g_texFailed++;
        std3D_DebugLineKV(0, "TFAIL", g_texFailed);
        return 0;
    }

    src = (const unsigned char *)vbuf->surface_lock_alloc;

    /* 16-bit RGB565/RGB1555 world materials — bryar bolts, blaster lasers,
     * sky textures, particle sprites, etc.  Engine stores a magenta-as-
     * transparency value in vbuf->transparent_color.  Mirror upstream
     * (_ORIGINAL_READ_ONLY/.../Platform/GL/std3D.c:3150-3199): convert
     * per-channel 5/6-bit → 8-bit via the (val * 527 + 23) >> 6 expansion
     * and compare against transparent_color per-channel to set alpha=0. */
    if (vbuf->format.fmt_is16bit) {
        const unsigned short *src16 = (const unsigned short *)src;
        unsigned int g_bits = vbuf->format.fmt_g_bits;
        unsigned int tk     = vbuf->transparent_color;
        unsigned int j;

        if (fail_logN < 6) {
            XDBGF("ATC 16bit OK: vbuf=%p w=%u h=%u rgb=(%u,%u,%u) tkey=0x%X\n",
                  (void*)vbuf, w, h, vbuf->format.fmt_r_bits, g_bits,
                  vbuf->format.fmt_b_bits, tk);
            fail_logN++;
        }

        total = w * h;
        if (g_bits == 6) {
            /* RGB565 */
            unsigned int tk_r5 = (tk >> 11) & 0x1F;
            unsigned int tk_g6 = (tk >>  5) & 0x3F;
            unsigned int tk_b5 = (tk >>  0) & 0x1F;
            for (j = 0; j < total; ++j) {
                unsigned int px = src16[j];
                unsigned int r5 = (px >> 11) & 0x1F;
                unsigned int g6 = (px >>  5) & 0x3F;
                unsigned int b5 = (px >>  0) & 0x1F;
                unsigned int o  = j * 4;
                unsigned int a8 = 0xFF;
                if (tk && r5 == tk_r5 && g6 == tk_g6 && b5 == tk_b5) a8 = 0;
                g_texScratch[o + 0] = (unsigned char)((r5 * 527u + 23u) >> 6);
                g_texScratch[o + 1] = (unsigned char)((g6 * 259u + 33u) >> 6);
                g_texScratch[o + 2] = (unsigned char)((b5 * 527u + 23u) >> 6);
                g_texScratch[o + 3] = (unsigned char)a8;
            }
        } else {
            /* RGB1555 */
            unsigned int tk_a1 = (tk >> 15) & 0x01;
            unsigned int tk_r5 = (tk >> 10) & 0x1F;
            unsigned int tk_g5 = (tk >>  5) & 0x1F;
            unsigned int tk_b5 = (tk >>  0) & 0x1F;
            for (j = 0; j < total; ++j) {
                unsigned int px = src16[j];
                unsigned int a1 = (px >> 15) & 0x01;
                unsigned int r5 = (px >> 10) & 0x1F;
                unsigned int g5 = (px >>  5) & 0x1F;
                unsigned int b5 = (px >>  0) & 0x1F;
                unsigned int o  = j * 4;
                unsigned int a8 = a1 ? 0xFF : 0;
                if (tk && a1 == tk_a1 && r5 == tk_r5 && g5 == tk_g5 && b5 == tk_b5) a8 = 0;
                g_texScratch[o + 0] = (unsigned char)((r5 * 527u + 23u) >> 6);
                g_texScratch[o + 1] = (unsigned char)((g5 * 527u + 23u) >> 6);
                g_texScratch[o + 2] = (unsigned char)((b5 * 527u + 23u) >> 6);
                g_texScratch[o + 3] = (unsigned char)a8;
            }
        }

        /* Goto the bind+upload tail.  The 8-bit conversion below is
         * skipped via a flag — easier than restructuring the function. */
        goto std3D_atc_do_upload;
    }

    pal = (rdColor24_local *)vbuf->palette;
    if (!pal) pal = g_pCurrentPalette;
    if (!pal) {
        /* Final fallback — read sithWorld_pCurrentWorld->colormaps->colors,
         * mirroring Platform/GL/std3D.c:3017.  Required because engine
         * loads UI/menu textures BEFORE rdColormap_SetIdentity fires
         * std3D_SetCurrentPalette (the gate at rdColormap.c:21 is
         * !rdColormap_pIdentityMap which is non-null after first
         * activation).  Layout offsets from types.h:2246 (sithWorld)
         * and types.h:1177 (rdColormap, with SITH_DEBUG_STRUCT_NAMES
         * defined → colormap_fname[32] prefix).
         *
         *   sithWorld_pCurrentWorld + 0x48 → rdColormap* colormaps
         *   colormap + 0x30                → rdColor24 colors[256]
         *
         * Access via xbox_get_world_palette() in xbox_debug.c — keeps
         * the C++ linkage of the engine-side global out of this TU. */
        {
            void *world_pal = xbox_get_world_palette();
            if (world_pal) {
                pal = (rdColor24_local*)world_pal;
            }
        }
        if (!pal) {
            if (fail_logN < 6) {
                XDBGF("ATC fail nopal: vbuf=%p vbuf.pal=%p curPal=%p\n",
                      (void*)vbuf, (void*)vbuf->palette, (void*)g_pCurrentPalette);
                fail_logN++;
            }
            fail_nopal++; g_texFailed++;
            std3D_DebugLineKV(0, "TFAIL", g_texFailed);
            return 0;
        }
    }

    /* 8bpp → RGBA8 conversion.  RGBA byte order: byte0=R, byte1=G,
     * byte2=B, byte3=A.  Index 0 is the engine's transparent index for
     * alpha-keyed textures (sky/sprites). */
    total = w * h;
    for (i = 0; i < total; ++i) {
        unsigned int idx = src[i];
        unsigned int o   = i * 4;
        if (is_alpha_tex && idx == 0) {
            g_texScratch[o + 0] = 0;
            g_texScratch[o + 1] = 0;
            g_texScratch[o + 2] = 0;
            g_texScratch[o + 3] = 0;     /* transparent */
        } else {
            g_texScratch[o + 0] = pal[idx].r;
            g_texScratch[o + 1] = pal[idx].g;
            g_texScratch[o + 2] = pal[idx].b;
            g_texScratch[o + 3] = 0xFF;  /* opaque */
        }
    }

std3D_atc_do_upload:
    /* Allocate an id and upload.  glBindTexture before glTexImage2D so
     * FakeGL's TextureTable creates the entry under our chosen id. */
    id = g_nextTexId++;
    if (g_pfnBindTexture) {
        g_pfnBindTexture(GL_TEXTURE_2D, id);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_LINEAR);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,    (GLfloat)GL_REPEAT);
        glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,    (GLfloat)GL_REPEAT);
        glTexImage2D(GL_TEXTURE_2D, 0, 4, (GLsizei)w, (GLsizei)h, 0,
                     GL_RGBA, GL_UNSIGNED_BYTE, g_texScratch);
    } else {
        if (fail_logN < 6) {
            XDBG("ATC fail nobind: g_pfnBindTexture is NULL\n");
            fail_logN++;
        }
        fail_nobind++; g_texFailed++;
        std3D_DebugLineKV(0, "TFAIL", g_texFailed);
        return 0;
    }

    texture->texture_id     = (int)id;
    texture->texture_loaded = 1;
    texture->is_16bit       = 0;
    texture->width          = w;
    texture->height         = h;

    g_texUploaded++;
    std3D_DebugLineKV(1, "TUP", g_texUploaded);

    /* Log first N uploads so we can verify on hardware that pixel +
     * palette pointers look sane. */
    { static int _first = 0;
      if (_first < 24) {
          XDBGF("ATC[%u]: id=%u w=%u h=%u src=%p pal=%p alpha=%d first=(%02X,%02X,%02X)\n",
                _first, id, w, h, vbuf->surface_lock_alloc, pal,
                is_alpha_tex, pal[0].r, pal[0].g, pal[0].b);
          _first++;
      } }

    return 1;
}

/* ====================================================================== */
/* HUD bitmap upload + drawing (std3D_DrawUI* family).                    */
/*                                                                        */
/* Uploads stdBitmap mips to FakeGL texture ids stored in                 */
/* pBmp->aTextureIds[mipIdx]. Draws are immediate-mode textured quads     */
/* in 2D ortho space (matches std3D_Present's GL_Set2D setup).            */
/*                                                                        */
/* Engine-side stdBitmap layout under SDL2_RENDER (no
 * OPTIMIZE_AWAY_UNUSED_FIELDS, no STDBITMAP_PARTIAL_LOAD,
 * no RDMATERIAL_MINIMIZE_STRUCTS).  STDBITMAP_PARTIAL_LOAD and the
 * MINIMIZE define are gated on TARGET_TWL (engine_config.h:298-335);
 * Xbox doesn't define TARGET_TWL so neither applies here. */
typedef struct stdBitmap_local {
    char                 fpath[32];           /* 0x00 */
    int                  field_20;            /* 0x20 */
    int                  palFmt;              /* 0x24 */
    unsigned int         format[14];          /* 0x28 — rdTexFormat (14 u32) */
    void                *palette;             /* 0x60 */
    int                  numMips;             /* 0x64 */
    int                  field_68;            /* 0x68 */
    int                  xPos;                /* 0x6C */
    int                  yPos;                /* 0x70 */
    unsigned int         colorkey;            /* 0x74 */
    stdVBuffer         **mipSurfaces;         /* 0x78 */
    int                  is_16bit;            /* 0x7C */
    int                 *abLoadedToGPU;       /* 0x80 */
    unsigned int        *aTextureIds;         /* 0x84 */
    void               **paDataDepthConverted;/* 0x88 */
} stdBitmap_local;

typedef struct rdRect_local {
    int x;
    int y;
    int width;
    int height;
} rdRect_local;

/* Round v up to next power of two (v <= 1 → 1; v=59 → 64; v=129 → 256). */
static unsigned int xbox_next_pow2(unsigned int v)
{
    unsigned int r = 1;
    if (v == 0) return 1;
    while (r < v) r <<= 1;
    return r;
}

static int xbox_upload_bitmap_mip(stdBitmap_local *bm, int mipIdx, int is_alpha_tex)
{
    stdVBuffer          *vbuf;
    unsigned int         w, h, i, padW, padH, x, y;
    const unsigned char *src;
    rdColor24_local     *pal;
    unsigned int         id;

    if (!bm || !bm->mipSurfaces || !bm->aTextureIds || !bm->abLoadedToGPU) {
        { static int _r=0; if(_r<8){ XDBGF("xup#A: bm=%p mipS=%p aTex=%p aL=%p\n",
            (void*)bm, bm?(void*)bm->mipSurfaces:0, bm?(void*)bm->aTextureIds:0,
            bm?(void*)bm->abLoadedToGPU:0); _r++; } }
        return 0;
    }
    if (mipIdx < 0 || mipIdx >= bm->numMips) {
        { static int _r=0; if(_r<8){ XDBGF("xup#B: mipIdx=%d numMips=%d\n", mipIdx, bm->numMips); _r++; } }
        return 0;
    }
    if (bm->abLoadedToGPU[mipIdx]) return 1;

    vbuf = bm->mipSurfaces[mipIdx];
    if (!vbuf || !vbuf->surface_lock_alloc) {
        { static int _r=0; if(_r<8){ XDBGF("xup#C: vbuf=%p alloc=%p (mip=%d)\n",
            (void*)vbuf, vbuf?(void*)vbuf->surface_lock_alloc:0, mipIdx); _r++; } }
        return 0;
    }

    w = vbuf->format.width_in_pixels;
    h = (unsigned int)vbuf->format.height;
    /* FakeGL pushes pixels through XGSwizzleRect (fakeglx.cpp:2459), which
     * requires power-of-two destination dimensions.  JK1 world textures are
     * authored pow2, but HUD/UI BMs are not (statusLeft16.bm = 59x60,
     * stHealth16.bm = 18x18, etc.) — uploading them at native size lands
     * pixel rows at undefined offsets and the HUD shows scrambled rectangles
     * instead of the artwork.  Pad to pow2 here; the draw side mirrors the
     * same padding when computing UVs so only the populated sub-rect samples. */
    padW = xbox_next_pow2(w);
    padH = xbox_next_pow2(h);
    if (w == 0 || h == 0 || padW > STD3D_TEX_SCRATCH_W || padH > STD3D_TEX_SCRATCH_H) {
        { static int _r=0; if(_r<8){ XDBGF("xup#D: w=%u h=%u pad=%ux%u scratch=%dx%d\n",
            w, h, padW, padH, STD3D_TEX_SCRATCH_W, STD3D_TEX_SCRATCH_H); _r++; } }
        return 0;
    }
    src = (const unsigned char *)vbuf->surface_lock_alloc;

    /* Clear the full padded RGBA buffer to transparent black before writing
     * the populated sub-rect.  Padding pixels never sample (UVs are clamped
     * to w/padW, h/padH), but keep them deterministic. */
    {
        unsigned int total_pad = padW * padH * 4u;
        for (i = 0; i < total_pad; ++i) g_texScratch[i] = 0;
    }

    if (vbuf->format.fmt_is16bit) {
        /* 16-bit source — engine stores RGB565 (or ARGB1555) in u16 pixels.
         * Use the format-descriptor's r/g/b bits+shifts to extract channels.
         * HUD bitmaps (jkstrings text, status numbers) take this path. */
        const unsigned short *src16 = (const unsigned short *)src;
        unsigned int r_bits  = vbuf->format.fmt_r_bits;
        unsigned int g_bits  = vbuf->format.fmt_g_bits;
        unsigned int b_bits  = vbuf->format.fmt_b_bits;
        /* Mirror upstream OpenJKDF2 (Platform/GL/std3D.c:3150-3199): pixels
         * are RGB565 (or RGB1555); the engine stores a magenta-as-key in
         * vbuf->transparent_color and expects per-channel equality at the
         * channel-bit width (not scaled).  Use (r5 * 527 + 23) >> 6 for the
         * 5→8 / 6→8 scale (matches upstream Bourbie expansion).
         *
         * Shifts/bits sanity check: log showed (5,6,5) shifts (11,5,0) for
         * HUD bitmaps so this is RGB565.  We branch RGB565 vs RGB1555 on
         * g_bits. */
        unsigned int tk = vbuf->transparent_color;
        int has_color_key = (bm->palFmt & 1) != 0;

        /* Match upstream PC GL: 16-bit RGB565 UI/SFT pixels are transparent
         * only when the bitmap carries an actual color key.  Unkeyed SFT
         * font strips use 0x0000 black as real glyph/background data, not as
         * implicit transparency. */

        { static int _ru = 0; if (_ru < 4) {
            XDBGF("xup 16bit: w=%u h=%u rbg=(%u,%u,%u) tkey=0x%X palFmt=%d\n",
                  w, h, vbuf->format.fmt_r_bits, g_bits, b_bits, tk, bm->palFmt);
            _ru++;
        }}

        if (g_bits == 6) {
            /* RGB565: r at bit 11, g at bit 5, b at bit 0. */
            unsigned int tk_r5 = (tk >> 11) & 0x1F;
            unsigned int tk_g6 = (tk >>  5) & 0x3F;
            unsigned int tk_b5 = (tk >>  0) & 0x1F;
            for (y = 0; y < h; ++y) {
                for (x = 0; x < w; ++x) {
                    unsigned int px = src16[y * w + x];
                    unsigned int r5 = (px >> 11) & 0x1F;
                    unsigned int g6 = (px >>  5) & 0x3F;
                    unsigned int b5 = (px >>  0) & 0x1F;
                    unsigned int o  = (y * padW + x) * 4;
                    unsigned int r8 = (r5 * 527u + 23u) >> 6;
                    unsigned int g8 = (g6 * 259u + 33u) >> 6;
                    unsigned int b8 = (b5 * 527u + 23u) >> 6;
                    unsigned int a8 = 0xFF;
                    if (has_color_key && r5 == tk_r5 && g6 == tk_g6 && b5 == tk_b5) {
                        a8 = 0;
                    }
                    g_texScratch[o + 0] = (unsigned char)r8;
                    g_texScratch[o + 1] = (unsigned char)g8;
                    g_texScratch[o + 2] = (unsigned char)b8;
                    g_texScratch[o + 3] = (unsigned char)a8;
                }
            }
        } else {
            /* RGB1555: alpha at bit 15, r at bit 10, g at bit 5, b at bit 0. */
            unsigned int tk_a1 = (tk >> 15) & 0x01;
            unsigned int tk_r5 = (tk >> 10) & 0x1F;
            unsigned int tk_g5 = (tk >>  5) & 0x1F;
            unsigned int tk_b5 = (tk >>  0) & 0x1F;
            for (y = 0; y < h; ++y) {
                for (x = 0; x < w; ++x) {
                    unsigned int px = src16[y * w + x];
                    unsigned int a1 = (px >> 15) & 0x01;
                    unsigned int r5 = (px >> 10) & 0x1F;
                    unsigned int g5 = (px >>  5) & 0x1F;
                    unsigned int b5 = (px >>  0) & 0x1F;
                    unsigned int o  = (y * padW + x) * 4;
                    unsigned int r8 = (r5 * 527u + 23u) >> 6;
                    unsigned int g8 = (g5 * 527u + 23u) >> 6;
                    unsigned int b8 = (b5 * 527u + 23u) >> 6;
                    unsigned int a8 = a1 ? 0xFF : 0;
                    if (has_color_key && a1 == tk_a1 && r5 == tk_r5 && g5 == tk_g5 && b5 == tk_b5) {
                        a8 = 0;
                    }
                    g_texScratch[o + 0] = (unsigned char)r8;
                    g_texScratch[o + 1] = (unsigned char)g8;
                    g_texScratch[o + 2] = (unsigned char)b8;
                    g_texScratch[o + 3] = (unsigned char)a8;
                }
            }
        }
        /* 16-bit UI textures encode transparency via colorkey — force
         * alpha-blend on so the keyed pixels actually punch through. */
        is_alpha_tex = 1;
    } else {
        /* 8-bit palettized.  Look up RGB through the bitmap or world pal. */
        pal = (rdColor24_local *)vbuf->palette;
        if (!pal) pal = (rdColor24_local *)bm->palette;
        if (!pal) pal = g_pCurrentPalette;
        if (!pal) {
            void *world_pal = xbox_get_world_palette();
            if (world_pal) pal = (rdColor24_local*)world_pal;
        }
        if (!pal) {
            { static int _r=0; if(_r<8){ XDBGF("xup#F: pal NULL (vbuf.pal=%p bm.pal=%p curPal=%p)\n",
                (void*)vbuf->palette, (void*)bm->palette, (void*)g_pCurrentPalette); _r++; } }
            return 0;
        }

        for (y = 0; y < h; ++y) {
            for (x = 0; x < w; ++x) {
                unsigned int idx = src[y * w + x];
                unsigned int o   = (y * padW + x) * 4;
                /* Palette index 0 → fully transparent.  JK1 8-bit convention.
                 * Mirror upstream `_ORIGINAL_READ_ONLY/.../Platform/GL/std3D.c:3294-3296`. */
                if (idx == 0) {
                    g_texScratch[o + 0] = 0;
                    g_texScratch[o + 1] = 0;
                    g_texScratch[o + 2] = 0;
                    g_texScratch[o + 3] = 0;
                } else {
                    g_texScratch[o + 0] = pal[idx].r;
                    g_texScratch[o + 1] = pal[idx].g;
                    g_texScratch[o + 2] = pal[idx].b;
                    g_texScratch[o + 3] = 0xFF;
                }
            }
        }
    }

    if (!g_pfnBindTexture) return 0;
    id = g_nextTexId++;
    g_pfnBindTexture(GL_TEXTURE_2D, id);
    /* UI textures: NEAREST + CLAMP_TO_EDGE.  Mirror upstream
     * `_ORIGINAL_READ_ONLY/.../Platform/GL/std3D.c:3106-3115,3124-3125`.
     * LINEAR was producing blurred HUD pixels (XBOX_HACKS.md option 2);
     * NEAREST matches the 1997 JK1 look and keeps pixel art crisp. */
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S,    (GLfloat)GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T,    (GLfloat)GL_CLAMP);
    /* Do NOT set GL_TEXTURE_BASE_LEVEL / GL_TEXTURE_MAX_LEVEL — FakeGL's
     * glTexParameterf hits its default:/LocalDebugBreak path on those
     * pnames (fakeglx.cpp:2367-2399) and still calls SetRenderStateDirty +
     * DirtyTexture as side-effects.  Since each xbox_upload_bitmap_mip
     * call creates a single-level D3D texture anyway (CreateTexture with
     * levels=1), the GPU never samples a non-existent mip. */
    /* internalFormat=4 (legacy GL 1.1 "n components" form).  FakeGL on
     * Xbox handles this; switching to GL_RGBA broke the world render. */
    /* Upload at padded pow2 dimensions — required by FakeGL's
     * XGSwizzleRect (fakeglx.cpp:2459).  The first w×h pixels carry the
     * real artwork; the rest is zero-cleared and never sampled because
     * the draw clamps UVs to w/padW × h/padH. */
    glTexImage2D(GL_TEXTURE_2D, 0, 4, (GLsizei)padW, (GLsizei)padH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, g_texScratch);

    bm->aTextureIds[mipIdx]   = id;
    bm->abLoadedToGPU[mipIdx] = 1;

    { static int _n = 0;
      if (_n < 16) {
          XDBGF("UI tex upload: bm=%p mip=%d id=%u w=%u h=%u alpha=%d\n",
                (void*)bm, mipIdx, id, w, h, is_alpha_tex);
          _n++;
      } }
    return 1;
}

int std3D_AddBitmapToTextureCache(void *pBmp, int mipIdx, int is_alpha_tex, int no_mip)
{
    (void)no_mip;
    if (!g_initialized) return 1;
    if (!pBmp) return 1;
    return xbox_upload_bitmap_mip((stdBitmap_local*)pBmp, mipIdx, is_alpha_tex);
}

void std3D_PurgeBitmapRefs(void *pBmp)
{
    /* No-op: FakeGL doesn't expose glDeleteTextures via the wgl proc table
     * we use. HUD textures are few and small; leak until process exit. */
    (void)pBmp;
}

/* Switch GL state to pixel-space ortho UI mode. Called per UI draw — the
 * engine reprograms projection for 3D rendering each frame, so we restore
 * the 2D setup defensively. Cheap (a few state pokes). */
static void xbox_set_ui_state(int enable_blend)
{
    glViewport(0, 0, 640, 480);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0, 640.0, 480.0, 0.0, -99999.0, 99999.0);
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    glDisable(GL_DEPTH_TEST);
    glDisable(GL_CULL_FACE);
    glDisable(GL_ALPHA_TEST);
    if (enable_blend) {
        glEnable(GL_BLEND);
        glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    } else {
        glDisable(GL_BLEND);
    }
}

void std3D_DrawMenuVBuffer8(stdVBuffer *vbuf, const rdColor24_local *pal)
{
    unsigned int w, h, padW, padH, x, y, total;
    const unsigned char *src;
    unsigned int texId;
    float u2, v2;
    static unsigned int menuTexId = 0;

    if (!g_initialized || !vbuf || !vbuf->surface_lock_alloc)
        return;
    if (!pal)
        pal = g_pCurrentPalette;
    if (!pal)
        return;

    w = (unsigned int)vbuf->format.width;
    h = (unsigned int)vbuf->format.height;
    if (!w || !h || w > STD3D_TEX_SCRATCH_W || h > STD3D_TEX_SCRATCH_H)
        return;

    padW = 1;
    while (padW < w) padW <<= 1;
    padH = 1;
    while (padH < h) padH <<= 1;
    if (padW > STD3D_TEX_SCRATCH_W || padH > STD3D_TEX_SCRATCH_H)
        return;

    src = (const unsigned char *)vbuf->surface_lock_alloc;
    total = padW * padH * 4;
    memset(g_texScratch, 0, total);

    for (y = 0; y < h; ++y)
    {
        const unsigned char *row = src + y * vbuf->format.width_in_bytes;
        for (x = 0; x < w; ++x)
        {
            unsigned int idx = row[x];
            unsigned int o = (y * padW + x) * 4;
            g_texScratch[o + 0] = pal[idx].r;
            g_texScratch[o + 1] = pal[idx].g;
            g_texScratch[o + 2] = pal[idx].b;
            g_texScratch[o + 3] = 0xFF;
        }
    }

    if (!g_pfnBindTexture)
        return;

    if (!menuTexId)
        menuTexId = g_nextTexId++;
    texId = menuTexId;
    g_pfnBindTexture(GL_TEXTURE_2D, texId);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MIN_FILTER, (GLfloat)GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_MAG_FILTER, (GLfloat)GL_NEAREST);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, (GLfloat)GL_CLAMP);
    glTexParameterf(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, (GLfloat)GL_CLAMP);
    glTexImage2D(GL_TEXTURE_2D, 0, 4, (GLsizei)padW, (GLsizei)padH, 0,
                 GL_RGBA, GL_UNSIGNED_BYTE, g_texScratch);

    xbox_set_ui_state(0);
    glEnable(GL_TEXTURE_2D);
    g_pfnBindTexture(GL_TEXTURE_2D, texId);
    glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
    u2 = (float)w / (float)padW;
    v2 = (float)h / (float)padH;
    glBegin(GL_TRIANGLES);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f,   0.0f,   0.0f);
    glTexCoord2f(u2,   0.0f); glVertex3f(640.0f, 0.0f,   0.0f);
    glTexCoord2f(u2,   v2);   glVertex3f(640.0f, 480.0f, 0.0f);
    glTexCoord2f(0.0f, 0.0f); glVertex3f(0.0f,   0.0f,   0.0f);
    glTexCoord2f(u2,   v2);   glVertex3f(640.0f, 480.0f, 0.0f);
    glTexCoord2f(0.0f, v2);   glVertex3f(0.0f,   480.0f, 0.0f);
    glEnd();
    glDisable(GL_TEXTURE_2D);
}

void std3D_DrawUIBitmapRGBA(void *pBmp_v, int mipIdx, float dstX, float dstY,
                             rdRect_local *srcRect, float scaleX, float scaleY,
                             int bAlphaOverwrite,
                             unsigned char cr, unsigned char cg,
                             unsigned char cb, unsigned char ca)
{
    stdBitmap_local *pBmp = (stdBitmap_local *)pBmp_v;
    stdVBuffer      *vbuf;
    unsigned int     texW, texH;
    float            srcX, srcY, srcW, srcH;
    float            u1, v1, u2, v2;
    float            dstW, dstH;
    unsigned int     texId;
    int              palFmt;
    int              is_alpha_tex;

    (void)bAlphaOverwrite;

    { static int _gn = 0;
      if (_gn < 6) {
          XDBGF("UI gate: bm=%p init=%d mipSurf=%p aLoaded=%p aTex=%p numMips=%d mip=%d palFmt=%d\n",
                pBmp_v, g_initialized,
                pBmp ? (void*)pBmp->mipSurfaces : (void*)0,
                pBmp ? (void*)pBmp->abLoadedToGPU : (void*)0,
                pBmp ? (void*)pBmp->aTextureIds : (void*)0,
                pBmp ? pBmp->numMips : -1, mipIdx,
                pBmp ? pBmp->palFmt : -1);
          _gn++;
      } }

    if (!g_initialized || !pBmp || !pBmp->mipSurfaces || !pBmp->abLoadedToGPU) {
        { static int _r=0; if(_r<3){ XDBGF("UI ret#A: init=%d bmp=%p mipS=%p aL=%p\n",
            g_initialized, (void*)pBmp,
            pBmp?(void*)pBmp->mipSurfaces:0, pBmp?(void*)pBmp->abLoadedToGPU:0); _r++; } }
        return;
    }
    if (mipIdx < 0 || mipIdx >= pBmp->numMips) {
        { static int _r=0; if(_r<3){ XDBGF("UI ret#B: mip=%d numMips=%d\n", mipIdx, pBmp->numMips); _r++; } }
        return;
    }

    vbuf = pBmp->mipSurfaces[mipIdx];
    if (!vbuf) {
        { static int _r=0; if(_r<3){ XDBGF("UI ret#C: vbuf NULL for mip=%d\n", mipIdx); _r++; } }
        return;
    }

    palFmt       = pBmp->palFmt;
    is_alpha_tex = !(palFmt & 1);

    if (!pBmp->abLoadedToGPU[mipIdx]) {
        { static int _r=0; if(_r<3){ XDBGF("UI lazy upload: bm=%p mip=%d palFmt=%d alpha=%d\n",
            (void*)pBmp, mipIdx, palFmt, is_alpha_tex); _r++; } }
        if (!xbox_upload_bitmap_mip(pBmp, mipIdx, is_alpha_tex)) {
            { static int _r=0; if(_r<3){ XDBGF("UI ret#D: upload failed mip=%d\n", mipIdx); _r++; } }
            return;
        }
    }
    texId = pBmp->aTextureIds[mipIdx];
    if (!texId || !g_pfnBindTexture) {
        { static int _r=0; if(_r<3){ XDBGF("UI ret#E: texId=%u bindFn=%p\n", texId, (void*)g_pfnBindTexture); _r++; } }
        return;
    }

    texW = vbuf->format.width_in_pixels;
    texH = (unsigned int)vbuf->format.height;
    if (!texW || !texH) {
        { static int _r=0; if(_r<3){ XDBGF("UI ret#F: texW=%u texH=%u\n", texW, texH); _r++; } }
        return;
    }
    /* The GPU texture is padded to pow2 (see xbox_upload_bitmap_mip — FakeGL
     * requires pow2 dims for XGSwizzleRect).  Compute UVs against the
     * padded dims so they map only the populated sub-rect. */
    {
        unsigned int padTexW = xbox_next_pow2(texW);
        unsigned int padTexH = xbox_next_pow2(texH);

        { static int _ok=0; if(_ok<6){ XDBGF("UI proceed: bm=%p texId=%u w=%u h=%u pad=%ux%u\n",
            (void*)pBmp, texId, texW, texH, padTexW, padTexH); _ok++; } }

        if (srcRect) {
            srcX = (float)srcRect->x;
            srcY = (float)srcRect->y;
            srcW = (float)srcRect->width;
            srcH = (float)srcRect->height;
        } else {
            srcX = 0.0f; srcY = 0.0f;
            srcW = (float)texW; srcH = (float)texH;
        }
        u1 = srcX / (float)padTexW;
        v1 = srcY / (float)padTexH;
        u2 = (srcX + srcW) / (float)padTexW;
        v2 = (srcY + srcH) / (float)padTexH;
    }

    dstW = srcW * scaleX;
    dstH = srcH * scaleY;

    /* UI bitmaps that arrived as 16-bit (RGB565) encode transparency via
     * vbuf->transparent_color (typically magenta).  Their palFmt bit
     * doesn't reflect that, so always enable alpha blend when the source
     * is 16-bit — otherwise alpha=0 keyed pixels rasterize as solid
     * magenta. */
    {
        int need_blend = is_alpha_tex || ca < 0xFF;
        if (vbuf && vbuf->format.fmt_is16bit) need_blend = 1;
        xbox_set_ui_state(need_blend);
    }

    /* Order matters: world DRL (std3D.c:1023-1030) binds the texture
     * BEFORE glEnable(GL_TEXTURE_2D).  FakeGL's dirty-state mechanism
     * requires bind-first so the next SetGLRenderState commits the right
     * texture stage source. */
    g_pfnBindTexture(GL_TEXTURE_2D, texId);
    glEnable(GL_TEXTURE_2D);

    /* CRITICAL: textured UI MUST be one-tri-per-glBegin, matching the
     * world DRL loop (std3D.c:1000+).  FakeGL's inline-mode off-by-one
     * (fakeglx.cpp:1416 -- `drawMode+1`) maps GL_TRIANGLES(4) to
     * D3DPT_TRIANGLESTRIP(5).  3-vertex submissions render identically
     * under either type; 6-vert submissions misinterpolate UVs and
     * render as transparent / fuchsia.
     *
     * Also: glColor4f goes INSIDE glBegin once per vertex, matching the
     * world DRL's per-vertex DIFFUSE writes.  If color is set outside
     * glBegin, FakeGL's m_OGLPrimitiveVertexBuffer.SetColor doesn't bind
     * to any vertex and the per-tri DIFFUSE state stays at the
     * leftover/default — which is typically fuchsia for unset vertex
     * data on NV2A. */
    {
        float r = (float)cr / 255.0f;
        float g = (float)cg / 255.0f;
        float b = (float)cb / 255.0f;
        float a = (float)ca / 255.0f;

        glBegin(GL_TRIANGLES);
        glColor4f(r, g, b, a); glTexCoord2f(u1, v1); glVertex3f(dstX,        dstY,        0.0f);
        glColor4f(r, g, b, a); glTexCoord2f(u2, v1); glVertex3f(dstX + dstW, dstY,        0.0f);
        glColor4f(r, g, b, a); glTexCoord2f(u2, v2); glVertex3f(dstX + dstW, dstY + dstH, 0.0f);
        glEnd();

        glBegin(GL_TRIANGLES);
        glColor4f(r, g, b, a); glTexCoord2f(u1, v1); glVertex3f(dstX,        dstY,        0.0f);
        glColor4f(r, g, b, a); glTexCoord2f(u2, v2); glVertex3f(dstX + dstW, dstY + dstH, 0.0f);
        glColor4f(r, g, b, a); glTexCoord2f(u1, v2); glVertex3f(dstX,        dstY + dstH, 0.0f);
        glEnd();
    }


    glDisable(GL_TEXTURE_2D);

    { static int _n = 0;
      if (_n < 8) {
          XDBGF("UI draw: bm=%p mip=%d id=%u dst=(%d,%d) sz=(%d,%d) tint=%02X%02X%02X%02X\n",
                (void*)pBmp, mipIdx, texId, (int)dstX, (int)dstY,
                (int)dstW, (int)dstH, cr, cg, cb, ca);
          _n++;
      } }
}

void std3D_DrawUIBitmap(void *pBmp, int mipIdx, float dstX, float dstY,
                        rdRect_local *srcRect, float scale, int bAlphaOverwrite)
{
    std3D_DrawUIBitmapRGBA(pBmp, mipIdx, dstX, dstY, srcRect, scale, scale,
                            bAlphaOverwrite, 0xFF, 0xFF, 0xFF, 0xFF);
}

void std3D_DrawUIClearedRectRGBA(unsigned char cr, unsigned char cg,
                                  unsigned char cb, unsigned char ca,
                                  rdRect_local *dstRect)
{
    float x, y, w, h;
    if (!g_initialized || !dstRect) return;

    x = (float)dstRect->x;
    y = (float)dstRect->y;
    w = (float)dstRect->width;
    h = (float)dstRect->height;
    if (w <= 0.0f || h <= 0.0f) return;

    xbox_set_ui_state(ca < 0xFF);
    glDisable(GL_TEXTURE_2D);

    glBegin(GL_TRIANGLES);
    glColor4f((float)cr / 255.0f, (float)cg / 255.0f, (float)cb / 255.0f, (float)ca / 255.0f);
    glVertex3f(x,     y,     0.0f);
    glVertex3f(x + w, y,     0.0f);
    glVertex3f(x + w, y + h, 0.0f);
    glVertex3f(x,     y,     0.0f);
    glVertex3f(x + w, y + h, 0.0f);
    glVertex3f(x,     y + h, 0.0f);
    glEnd();
}

void std3D_DrawUIClearedRect(unsigned char palIdx, rdRect_local *dstRect)
{
    /* Engine passes palette index for the colour. Resolve via current
     * palette (set by std3D_SetCurrentPalette) or world palette. */
    rdColor24_local *pal = g_pCurrentPalette;
    unsigned char r = 0xFF, g = 0xFF, b = 0xFF;

    if (!pal) {
        void *world_pal = xbox_get_world_palette();
        if (world_pal) pal = (rdColor24_local *)world_pal;
    }
    if (pal) {
        r = pal[palIdx].r;
        g = pal[palIdx].g;
        b = pal[palIdx].b;
    }
    std3D_DrawUIClearedRectRGBA(r, g, b, 0xFF, dstRect);
}

void std3D_UnloadAllTextures(void)              {}
int  std3D_SetCurrentPalette(void *p, int a)
{
    static int _logN = 0;
    (void)a;
    g_pCurrentPalette = (rdColor24_local *)p;
    if (_logN < 5) {
        XDBGF("SetCurrentPalette: p=%p (first=%02X,%02X,%02X)\n", p,
              p ? ((unsigned char*)p)[0] : 0,
              p ? ((unsigned char*)p)[1] : 0,
              p ? ((unsigned char*)p)[2] : 0);
        _logN++;
    }
    return 1;
}
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
int std3D_IsReady(void)              { return g_initialized; }

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
