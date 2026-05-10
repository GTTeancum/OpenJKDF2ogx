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

#define GL_LINES               0x0001
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

static void std3D_DrawDebugHUD(void)
{
    int i;
    float sweep;

    /* Background tint behind text area for legibility. */
    dbg_quad(0.0f, 0.0f, 220.0f, 4.0f + DBG_NUM_LINES * 12.0f,
             0.0f, 0.0f, 0.0f);

    for (i = 0; i < DBG_NUM_LINES; ++i)
    {
        if (dbg_lines[i][0])
            dbg_text(4.0f, 4.0f + i * 12.0f, 2.0f,
                     1.0f, 1.0f, 1.0f, dbg_lines[i]);
    }

    /* Sweeper bar at the bottom of the panel — proves StartScene is
     * being called every frame.  Increment per call. */
    g_dbgFrameTick++;
    sweep = (float)(g_dbgFrameTick % 120) / 120.0f;
    dbg_quad(0.0f,  4.0f + DBG_NUM_LINES * 12.0f, 220.0f, 4.0f,
             0.1f, 0.1f, 0.1f);
    dbg_quad(sweep * 208.0f, 4.0f + DBG_NUM_LINES * 12.0f,
             12.0f, 4.0f, 1.0f, 1.0f, 0.0f);
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
    if ((g_presentCalls % 60) == 1) {
        XDBGF("Scene: STARTS=%u CLEARS=%u ENDS=%u ENDTR=%u PRES=%u\n",
              g_startCalls, g_clearCalls, g_endCalls,
              g_endTransitions, g_presentCalls);
        if (g_bboxValid) {
            XDBGF("Vert bbox: x[%d..%d] y[%d..%d] z[%d..%d] color=%08X\n",
                  (int)g_bboxXmin, (int)g_bboxXmax,
                  (int)g_bboxYmin, (int)g_bboxYmax,
                  (int)g_bboxZmin, (int)g_bboxZmax,
                  g_firstColor);
        }
    }

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
    glDisable(GL_TEXTURE_2D);
    glDisable(GL_ALPHA_TEST);

    /* Live counters into the HUD text. */
    std3D_DebugLineKV(19, "STARTS",  g_startCalls);
    std3D_DebugLineKV(20, "CLEARS",  g_clearCalls);
    std3D_DebugLineKV(21, "ENDS",    g_endCalls);
    std3D_DebugLineKV(22, "ENDTR",   g_endTransitions);
    std3D_DebugLineKV(23, "PRESENT", g_presentCalls);

    std3D_DrawDebugHUD();

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

    /* Walk the new batch and accumulate bbox + sample first color. */
    for (i = 0; i < count; ++i)
    {
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
void std3D_DrawRenderList(void)
{
    int i;
    if (!g_initialized || !g_sceneOpen) return;
    if (GL_numTris == 0 || !GL_verticesDone) return;
    std3D_DebugLineKV(10, "DRAWN", GL_numTris);

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
     * No glTexCoord2f calls yet: the texture cache is stubbed (no real
     * textures registered with FakeGL) and the working diagnostic in
     * StartScene also omitted texcoords.  Adding them when no texture is
     * bound caused the per-tri triangle to disappear; reinstate when the
     * texture-cache path actually uploads textures via glTexImage2D.
     *
     * STD3D_WIREFRAME (set to 1): draw each tri as 3 white line segments
     * (GL_LINES with 6 vertices: a-b, b-c, c-a).  Diagnostic only — turn
     * off once textures are wired up and we want filled rendering. */
#define STD3D_WIREFRAME 1
    for (i = 0; i < GL_numTris; ++i)
    {
        rdTri      *t  = &GL_tmpTris[i];
        D3DVERTEX  *a  = &GL_tmpVertices[t->v1];
        D3DVERTEX  *b  = &GL_tmpVertices[t->v2];
        D3DVERTEX  *c  = &GL_tmpVertices[t->v3];

#if STD3D_WIREFRAME
        glBegin(GL_LINES);
        glColor4f(1.0f, 1.0f, 1.0f, 1.0f);
        glVertex3f(a->x, a->y, a->z);
        glVertex3f(b->x, b->y, b->z);
        glVertex3f(b->x, b->y, b->z);
        glVertex3f(c->x, c->y, c->z);
        glVertex3f(c->x, c->y, c->z);
        glVertex3f(a->x, a->y, a->z);
#else
        glBegin(GL_TRIANGLES);

        glColor4f(((a->color >> 16) & 0xFF) / 255.0f,
                  ((a->color >>  8) & 0xFF) / 255.0f,
                  ((a->color      ) & 0xFF) / 255.0f,
                  ((a->color >> 24) & 0xFF) / 255.0f);
        glVertex3f(a->x, a->y, a->z);

        glColor4f(((b->color >> 16) & 0xFF) / 255.0f,
                  ((b->color >>  8) & 0xFF) / 255.0f,
                  ((b->color      ) & 0xFF) / 255.0f,
                  ((b->color >> 24) & 0xFF) / 255.0f);
        glVertex3f(b->x, b->y, b->z);

        glColor4f(((c->color >> 16) & 0xFF) / 255.0f,
                  ((c->color >>  8) & 0xFF) / 255.0f,
                  ((c->color      ) & 0xFF) / 255.0f,
                  ((c->color >> 24) & 0xFF) / 255.0f);
        glVertex3f(c->x, c->y, c->z);
#endif

        glEnd();
    }

    /* Reset staging — engine may build a new batch in the same frame. */
    GL_numVertices  = 0;
    GL_verticesDone = 0;
    GL_numTris      = 0;
    GL_numLines     = 0;
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
