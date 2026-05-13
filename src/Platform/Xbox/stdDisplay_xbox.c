#include "platform_xbox.h"
#include "xbox_debug.h"

#include "Win95/stdDisplay.h"
#include "Win95/Video.h"
#include "Win95/Window.h"
#include "stdPlatform.h"
#include "jk.h"

#include <string.h>

extern "C" void std3D_DrawMenuVBuffer8(stdVBuffer *vbuf, const rdColor24 *pal);
extern "C" int  std3D_StartScene(void);
extern "C" int  std3D_EndScene(void);
extern "C" void std3D_Present(void);
extern int jkGame_isDDraw;
extern int jkCutscene_isRendering;

uint32_t Video_menuTexId = 0;
uint32_t Video_overlayTexId = 0;
rdColor24 stdDisplay_masterPalette[256];

static int stdDisplay_xboxModeSet = 0;

static void stdDisplay_xbox_InitFmt(stdVBufferTexFmt *fmt, int w, int h, int bpp)
{
    memset(fmt, 0, sizeof(*fmt));
    fmt->width = w;
    fmt->height = h;
    fmt->width_in_pixels = w;
    fmt->width_in_bytes = w * ((bpp == 16) ? 2 : 1);
    fmt->texture_size_in_bytes = fmt->width_in_bytes * h;
    fmt->format.bpp = bpp;
    fmt->format.is16bit = (bpp == 16);
}

static int stdDisplay_xbox_EnsureStaticBuffer(stdVBuffer *buf, int w, int h, int bpp, const void *palette)
{
    unsigned int bytes;
    if (!buf) return 0;

    bytes = (unsigned int)(w * h * ((bpp == 16) ? 2 : 1));
    if (!buf->surface_lock_alloc || buf->format.texture_size_in_bytes != bytes)
    {
        if (buf->surface_lock_alloc)
            std_pHS->free(buf->surface_lock_alloc);
        buf->surface_lock_alloc = (char *)std_pHS->alloc(bytes);
        if (!buf->surface_lock_alloc)
            return 0;
    }

    stdDisplay_xbox_InitFmt(&buf->format, w, h, bpp);
    buf->palette = (void *)palette;
    return 1;
}

int stdDisplay_Startup()
{
    stdDisplay_bStartup = 1;
    stdDisplay_numVideoModes = 1;
    stdDisplay_pCurDevice = &stdDisplay_aDevices[0];
    stdDisplay_aDevices[0].video_device[0].windowedMaybe = 0;
    return 1;
}

int stdDisplay_FindClosestDevice(void *a)
{
    (void)a;
    Video_dword_866D78 = 0;
    return 0;
}

int stdDisplay_Open(int a)
{
    (void)a;
    stdDisplay_pCurDevice = &stdDisplay_aDevices[0];
    stdDisplay_aDevices[0].video_device[0].windowedMaybe = 0;
    stdDisplay_bOpen = 1;
    return 1;
}

void stdDisplay_Close()
{
    stdDisplay_bOpen = 0;
}

int stdDisplay_FindClosestMode(render_pair *a1, struct stdVideoMode *render_surface, unsigned int max_modes)
{
    (void)a1;
    (void)render_surface;
    (void)max_modes;
    Video_curMode = 0;
    stdDisplay_bPaged = 1;
    stdDisplay_bModeSet = 1;
    return 0;
}

MATH_FUNC int stdDisplay_SetMode(unsigned int modeIdx, const void *palette, int paged)
{
    const int w = 640;
    const int h = 480;
    (void)paged;

    if (modeIdx >= 9)
        modeIdx = 0;

    stdDisplay_pCurVideoMode = &Video_renderSurface[modeIdx];
    stdDisplay_xbox_InitFmt(&stdDisplay_pCurVideoMode->format, w, h, 8);

    if (palette)
        stdDisplay_SetMasterPalette((uint8_t *)palette);

    if (!stdDisplay_xbox_EnsureStaticBuffer(&Video_menuBuffer, w, h, 8, stdDisplay_masterPalette))
        return 0;
    if (!stdDisplay_xbox_EnsureStaticBuffer(&Video_otherBuf, w, h, 8, stdDisplay_masterPalette))
        return 0;

    Video_pMenuBuffer = &Video_menuBuffer;
    Video_pOtherBuf = &Video_otherBuf;
    if (!Video_pVbufIdk)
        Video_pVbufIdk = &Video_bufIdk;

    Video_curMode = (int)modeIdx;
    stdDisplay_bModeSet = 1;
    stdDisplay_xboxModeSet = 1;
    return 1;
}

int stdDisplay_SetMasterPalette(uint8_t *pal)
{
    if (!pal) return 0;
    memcpy(stdDisplay_masterPalette, pal, sizeof(stdDisplay_masterPalette));
    Video_menuBuffer.palette = stdDisplay_masterPalette;
    Video_otherBuf.palette = stdDisplay_masterPalette;
    return 1;
}

uint8_t *stdDisplay_GetPalette()
{
    return (uint8_t *)stdDisplay_masterPalette;
}

int stdDisplay_ClearRect(stdVBuffer *buf, int fillColor, rdRect *rect)
{
    return stdDisplay_VBufferFill(buf, fillColor, rect);
}

int stdDisplay_DDrawGdiSurfaceFlip()
{
    if (!stdDisplay_xboxModeSet && !Video_menuBuffer.surface_lock_alloc)
        return 0;

    if (jkGame_isDDraw && !jkCutscene_isRendering)
        return 1;

    if (jkCutscene_isRendering)
        std3D_StartScene();

    std3D_DrawMenuVBuffer8(&Video_menuBuffer, stdDisplay_masterPalette);
    std3D_EndScene();
    std3D_Present();

    if (!jkCutscene_isRendering)
        std3D_StartScene();

    return 1;
}

int stdDisplay_ddraw_waitforvblank()
{
    return 1;
}

stdVBuffer *stdDisplay_VBufferNew(stdVBufferTexFmt *fmt, int create_ddraw_surface, int gpu_mem, const void *palette)
{
    stdVBuffer *out;
    unsigned int bytes;
    int bppBytes;
    (void)create_ddraw_surface;
    (void)gpu_mem;

    if (!fmt) return NULL;
    out = (stdVBuffer *)std_pHS->alloc(sizeof(stdVBuffer));
    if (!out) return NULL;

    memset(out, 0, sizeof(*out));
    out->format = *fmt;
    bppBytes = (fmt->format.bpp == 16 || fmt->format.is16bit) ? 2 : 1;
    if (out->format.width_in_pixels <= 0) out->format.width_in_pixels = out->format.width;
    if (out->format.width_in_bytes <= 0) out->format.width_in_bytes = out->format.width * bppBytes;
    bytes = out->format.width_in_bytes * out->format.height;
    out->format.texture_size_in_bytes = bytes;
    out->palette = (void *)palette;
    out->surface_lock_alloc = (char *)std_pHS->alloc(bytes);
    if (!out->surface_lock_alloc)
    {
        std_pHS->free(out);
        return NULL;
    }
    memset(out->surface_lock_alloc, 0, bytes);
    return out;
}

int stdDisplay_VBufferLock(stdVBuffer *buf)
{
    if (!buf || !buf->surface_lock_alloc) return 0;
    buf->bSurfaceLocked = 1;
    return 1;
}

void stdDisplay_VBufferUnlock(stdVBuffer *buf)
{
    if (buf) buf->bSurfaceLocked = 0;
}

int stdDisplay_VBufferSetColorKey(stdVBuffer *vbuf, int color)
{
    if (!vbuf) return 0;
    vbuf->transparent_color = (uint32_t)color;
    return 1;
}

int stdDisplay_VBufferCopy(stdVBuffer *dst, stdVBuffer *src, unsigned int blit_x, int blit_y, rdRect *rect, int alpha_maybe)
{
    rdRect srcRect;
    int bppBytes;
    int hasAlpha;
    int y;

    if (!dst || !src || !dst->surface_lock_alloc || !src->surface_lock_alloc)
        return 0;

    if (rect)
        srcRect = *rect;
    else
    {
        srcRect.x = 0;
        srcRect.y = 0;
        srcRect.width = src->format.width;
        srcRect.height = src->format.height;
    }

    bppBytes = (dst->format.format.bpp == 16 || dst->format.format.is16bit) ? 2 : 1;
    hasAlpha = (alpha_maybe & 1) && bppBytes == 1;

    for (y = 0; y < srcRect.height; ++y)
    {
        int dy = blit_y + y;
        int sy = srcRect.y + y;
        int x;
        if (dy < 0 || dy >= dst->format.height || sy < 0 || sy >= src->format.height)
            continue;

        for (x = 0; x < srcRect.width; ++x)
        {
            int dx = (int)blit_x + x;
            int sx = srcRect.x + x;
            unsigned char *s;
            unsigned char *d;
            if (dx < 0 || dx >= dst->format.width || sx < 0 || sx >= src->format.width)
                continue;

            s = (unsigned char *)src->surface_lock_alloc + sy * src->format.width_in_bytes + sx * bppBytes;
            if (hasAlpha && *s == 0)
                continue;
            d = (unsigned char *)dst->surface_lock_alloc + dy * dst->format.width_in_bytes + dx * bppBytes;
            if (bppBytes == 2)
            {
                d[0] = s[0];
                d[1] = s[1];
            }
            else
            {
                d[0] = s[0];
            }
        }
    }
    return 1;
}

int stdDisplay_VBufferFill(stdVBuffer *vbuf, int fillColor, rdRect *rect)
{
    rdRect r;
    int bppBytes;
    int y;

    if (!vbuf || !vbuf->surface_lock_alloc)
        return 0;

    if (rect)
        r = *rect;
    else
    {
        r.x = 0;
        r.y = 0;
        r.width = vbuf->format.width;
        r.height = vbuf->format.height;
    }

    bppBytes = (vbuf->format.format.bpp == 16 || vbuf->format.format.is16bit) ? 2 : 1;
    for (y = 0; y < r.height; ++y)
    {
        int dy = r.y + y;
        int x;
        if (dy < 0 || dy >= vbuf->format.height)
            continue;
        for (x = 0; x < r.width; ++x)
        {
            int dx = r.x + x;
            unsigned char *d;
            if (dx < 0 || dx >= vbuf->format.width)
                continue;
            d = (unsigned char *)vbuf->surface_lock_alloc + dy * vbuf->format.width_in_bytes + dx * bppBytes;
            d[0] = (unsigned char)fillColor;
            if (bppBytes == 2)
                d[1] = (unsigned char)((unsigned int)fillColor >> 8);
        }
    }
    return 1;
}

void stdDisplay_VBufferFree(stdVBuffer *vbuf)
{
    if (!vbuf) return;
    if (vbuf == &Video_menuBuffer || vbuf == &Video_otherBuf || vbuf == &Video_bufIdk)
        return;
    if (vbuf->surface_lock_alloc)
        std_pHS->free(vbuf->surface_lock_alloc);
    std_pHS->free(vbuf);
}

void stdDisplay_ddraw_surface_flip2() {}
void stdDisplay_RestoreDisplayMode() {}
stdVBuffer *stdDisplay_VBufferConvertColorFormat(void *a, stdVBuffer *b) { (void)a; return b; }
int stdDisplay_GammaCorrect3(int a1) { (void)a1; return 1; }
int stdDisplay_SetCooperativeLevel(uint32_t a) { (void)a; return 1; }
int stdDisplay_DrawAndFlipGdi(uint32_t a) { (void)a; return stdDisplay_DDrawGdiSurfaceFlip(); }
void stdDisplay_422A50() {}
