#include "jkGUIBuildMulti.h"

#include "General/Darray.h"
#include "General/stdBitmap.h"
#include "General/stdString.h"
#include "General/stdFont.h"
#include "Engine/rdMaterial.h" // TODO move stdVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIDialog.h"
#ifdef TARGET_XBOX
#include "Gui/jkGUIXboxKeyboard.h"
#endif
#include "General/stdFileUtil.h"
#include "General/stdFnames.h"
#include "Main/jkStrings.h"
#include "World/jkPlayer.h"
#include "General/util.h"
#include "Gui/jkGUITitle.h"
#include "Engine/rdColormap.h"
#include "Win95/stdDisplay.h"
#include "Engine/rdroid.h"
#include "Gui/jkGUIForce.h"
#include "Platform/std3D.h"
#include "Platform/stdControl.h"
#include "Main/jkRes.h"
#include "General/stdStrTable.h"
#include "Main/jkEpisode.h"
#include "Platform/std3D.h"
#include "Win95/Window.h"
#include "Engine/rdKeyframe.h"
#include "General/stdConffile.h"
#ifdef TARGET_XBOX
#include "Platform/Xbox/xbox_debug.h"
#endif

#include "jk.h"
#include "types.h"
#include "types_enums.h"

// MOTS added
int jkGuiBuildMulti_jediRank = 0;
int32_t jkGuiBuildMulti_bRendering = 0;

#ifdef TARGET_XBOX
enum
{
    JKGUIMULTI_XBTN_WHITE = 0,
    JKGUIMULTI_XBTN_BLACK,
    JKGUIMULTI_XBTN_LT,
    JKGUIMULTI_XBTN_RT,
    JKGUIMULTI_XBTN_COUNT
};

#define JKGUIMULTI_PREVIEW_X 315
#define JKGUIMULTI_PREVIEW_Y 115
#define JKGUIMULTI_PREVIEW_W 260
#define JKGUIMULTI_PREVIEW_H 260
#define JKGUIMULTI_PORTRAIT_X (JKGUIMULTI_PREVIEW_X + 94)
#define JKGUIMULTI_PORTRAIT_Y (JKGUIMULTI_PREVIEW_Y + 8)
#define JKGUIMULTI_PORTRAIT_W 72
#define JKGUIMULTI_PORTRAIT_H 72
#define JKGUIMULTI_PORTRAIT_CACHE_W 74
#define JKGUIMULTI_PORTRAIT_CACHE_H 74
#define JKGUIMULTI_PORTRAIT_CACHE_PIXELS (JKGUIMULTI_PORTRAIT_CACHE_W * JKGUIMULTI_PORTRAIT_CACHE_H)
#define JKGUIMULTI_PORTRAIT_CACHE_MAGIC 0x31504B4A
#define JKGUIMULTI_PORTRAIT_CACHE_VERSION 1

static const char *jkGuiBuildMulti_xboxButtonPaths[JKGUIMULTI_XBTN_COUNT] = {
    "ui\\bm\\xbtn_white.bm",
    "ui\\bm\\xbtn_black.bm",
    "ui\\bm\\xbtn_lt.bm",
    "ui\\bm\\xbtn_rt.bm",
};
static stdBitmap *jkGuiBuildMulti_xboxButtonBitmaps[JKGUIMULTI_XBTN_COUNT];
static int jkGuiBuildMulti_xboxButtonLogged[JKGUIMULTI_XBTN_COUNT];
static unsigned int jkGuiBuildMulti_xboxGlyphDrawCalls;
static uint8_t jkGuiBuildMulti_xboxLatestPortrait[JKGUIMULTI_PORTRAIT_CACHE_PIXELS];
static uint8_t jkGuiBuildMulti_xboxPortraitCandidate[JKGUIMULTI_PORTRAIT_CACHE_PIXELS];
static int jkGuiBuildMulti_xboxLatestPortraitValid;
static stdVBuffer* jkGuiBuildMulti_pVBuf1 = NULL;
static stdVBuffer* jkGuiBuildMulti_pVBuf2 = NULL;

static void jkGuiBuildMulti_XboxWritePortraitCache(const wchar_t *characterName);
static void jkGuiBuildMulti_XboxCaptureLatestPortrait(void);
static int jkGuiBuildMulti_XboxCaptureLatestPortraitFromVBuffer(stdVBuffer *srcVbuf, const char *label);
static void jkGuiBuildMulti_XboxCaptureVisiblePortraitBooth(void);
static void jkGuiBuildMulti_XboxCapturePortraitBooth(void);
static void jkGuiBuildMulti_XboxDeletePortraitCache(const wchar_t *characterName);
static int jkGuiBuildMulti_XboxWriteLatestOrGeneratePortraitCache(const wchar_t *characterName);

static void jkGuiBuildMulti_XboxPortraitPath(const wchar_t *characterName, char *outPath, int outSize)
{
    char mpcPath[128];
    char portraitBase[128];

    if (!outPath || outSize <= 0)
        return;
    outPath[0] = 0;
    if (!characterName || !characterName[0])
        return;

    jkPlayer_MPCMakePath(mpcPath, sizeof(mpcPath), jkPlayer_playerShortName, (wchar_t*)characterName);
    stdString_SafeStrCopy(portraitBase, mpcPath, sizeof(portraitBase));
    stdFnames_StripExtAndDot(portraitBase);
    stdString_snprintf(outPath, outSize, "%s.jkp", portraitBase);
}

int jkGuiBuildMulti_XboxPortraitBitmapHasContent(stdBitmap *bitmap)
{
    stdVBuffer *vbuf;
    int x, y;
    int brightPixels = 0;
    int nonZeroPixels = 0;
    int totalPixels = 0;

    if (!bitmap || !bitmap->mipSurfaces || !bitmap->mipSurfaces[0] || !bitmap->palette)
        return 0;

    vbuf = bitmap->mipSurfaces[0];
    if (vbuf->format.width <= 0 || vbuf->format.height <= 0)
        return 0;

    stdBitmap_EnsureData(bitmap);
    stdDisplay_VBufferLock(vbuf);
    for (y = 0; y < vbuf->format.height; y++)
    {
        uint8_t *row = (uint8_t*)vbuf->surface_lock_alloc + y * vbuf->format.width_in_bytes;
        for (x = 0; x < vbuf->format.width; x++)
        {
            rdColor24 *c = &((rdColor24*)bitmap->palette)[row[x]];
            int luma = (int)c->r + (int)c->g + (int)c->b;
            if (luma > 72)
                brightPixels++;
            if (row[x])
                nonZeroPixels++;
            totalPixels++;
        }
    }
    stdDisplay_VBufferUnlock(vbuf);

    return totalPixels > 0 && nonZeroPixels > (totalPixels / 32) && brightPixels > (totalPixels / 32);
}

static uint8_t jkGuiBuildMulti_XboxNearestMenuColor(const rdColor24 *src)
{
    int best = 0;
    int bestDist = 0x7FFFFFFF;
    int i;

    for (i = 0; i < 256; i++)
    {
        int dr = (int)src->r - (int)stdDisplay_masterPalette[i].r;
        int dg = (int)src->g - (int)stdDisplay_masterPalette[i].g;
        int db = (int)src->b - (int)stdDisplay_masterPalette[i].b;
        int dist = dr * dr + dg * dg + db * db;
        if (dist < bestDist)
        {
            bestDist = dist;
            best = i;
            if (!dist)
                break;
        }
    }

    return (uint8_t)best;
}

static int jkGuiBuildMulti_XboxGlyphPixelTransparent(stdBitmap *bitmap, uint8_t pix)
{
    rdColor24 *pal;

    if ((bitmap->palFmt & 1) && pix == (uint8_t)bitmap->colorkey)
        return 1;

    if (!bitmap->palette)
        return 0;

    pal = &((rdColor24*)bitmap->palette)[pix];
    return pal->r < 10 && pal->g < 10 && pal->b < 10;
}

static void jkGuiBuildMulti_XboxBlitGlyphRemapped(stdVBuffer *dst, stdBitmap *bitmap, rdRect *dstRect)
{
    stdVBuffer *src;
    uint8_t remap[256];
    uint8_t *srcBase;
    uint8_t *dstBase;
    int drawW;
    int drawH;
    int drawX;
    int drawY;
    int row;

    if (!dst || !bitmap || !bitmap->mipSurfaces || !bitmap->mipSurfaces[0] || !bitmap->palette)
        return;

    src = bitmap->mipSurfaces[0];
    if (src->format.width <= 0 || src->format.height <= 0 || dstRect->width <= 0 || dstRect->height <= 0)
        return;

    drawW = dstRect->width;
    drawH = (src->format.height * drawW) / src->format.width;
    if (drawH > dstRect->height)
    {
        drawH = dstRect->height;
        drawW = (src->format.width * drawH) / src->format.height;
    }
    if (drawW <= 0 || drawH <= 0)
        return;

    drawX = dstRect->x + (dstRect->width - drawW) / 2;
    drawY = dstRect->y + (dstRect->height - drawH) / 2;

    for (int i = 0; i < 256; i++)
        remap[i] = jkGuiBuildMulti_XboxNearestMenuColor(&((rdColor24*)bitmap->palette)[i]);

    stdDisplay_VBufferLock(src);
    stdDisplay_VBufferLock(dst);

    srcBase = (uint8_t*)src->surface_lock_alloc;
    dstBase = (uint8_t*)dst->surface_lock_alloc + drawY * dst->format.width_in_bytes + drawX;
    for (row = 0; row < drawH; row++)
    {
        int sy = (row * src->format.height) / drawH;
        uint8_t *srcRow = srcBase + sy * src->format.width_in_bytes;
        uint8_t *dstRow = dstBase + row * dst->format.width_in_bytes;
        int col;
        for (col = 0; col < drawW; col++)
        {
            int sx = (col * src->format.width) / drawW;
            uint8_t pix = srcRow[sx];
            if (jkGuiBuildMulti_XboxGlyphPixelTransparent(bitmap, pix))
                continue;
            dstRow[col] = remap[pix];
        }
    }

    stdDisplay_VBufferUnlock(dst);
    stdDisplay_VBufferUnlock(src);
}

static void jkGuiBuildMulti_XboxLoadButtonGlyphs(void)
{
    int i;
    for (i = 0; i < JKGUIMULTI_XBTN_COUNT; i++)
    {
        if (!jkGuiBuildMulti_xboxButtonBitmaps[i])
            jkGuiBuildMulti_xboxButtonBitmaps[i] = stdBitmap_LoadPartial((char*)jkGuiBuildMulti_xboxButtonPaths[i], 1, 0);
    }
}

static void jkGuiBuildMulti_XboxUnloadButtonGlyphs(void)
{
    int i;
    for (i = 0; i < JKGUIMULTI_XBTN_COUNT; i++)
    {
        if (jkGuiBuildMulti_xboxButtonBitmaps[i])
        {
            stdBitmap_Free(jkGuiBuildMulti_xboxButtonBitmaps[i]);
            jkGuiBuildMulti_xboxButtonBitmaps[i] = NULL;
        }
    }
}

static void jkGuiBuildMulti_XboxButtonGlyphDraw(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, BOOL redraw)
{
    stdBitmap *bitmap;
    int idx = element->selectedTextEntry;

    if (redraw)
        jkGuiRend_CopyVBuffer(menu, &element->rect);

    if (idx < 0 || idx >= JKGUIMULTI_XBTN_COUNT)
        return;

    jkGuiBuildMulti_XboxLoadButtonGlyphs();
    bitmap = jkGuiBuildMulti_xboxButtonBitmaps[idx];
    if (!bitmap)
        return;

    stdBitmap_EnsureData(bitmap);
    if (!bitmap->mipSurfaces || !bitmap->mipSurfaces[0])
        return;

    if (!jkGuiBuildMulti_xboxButtonLogged[idx])
    {
        jkGuiBuildMulti_xboxButtonLogged[idx] = 1;
        XDBGF("MenuGlyphDbg: idx=%d path='%s' bm=%p surf=%p size=%dx%d bpp=%u palFmt=0x%x colorkey=%u srcPal=%p dstPal0=%u,%u,%u\n",
              idx,
              jkGuiBuildMulti_xboxButtonPaths[idx],
              bitmap,
              bitmap->mipSurfaces[0],
              bitmap->mipSurfaces[0]->format.width,
              bitmap->mipSurfaces[0]->format.height,
              bitmap->mipSurfaces[0]->format.format.bpp,
              bitmap->palFmt,
              bitmap->colorkey,
              bitmap->palette,
              stdDisplay_masterPalette[0].r,
              stdDisplay_masterPalette[0].g,
              stdDisplay_masterPalette[0].b);
    }

    jkGuiBuildMulti_xboxGlyphDrawCalls++;
    if (jkGuiBuildMulti_xboxGlyphDrawCalls <= 24 || (jkGuiBuildMulti_xboxGlyphDrawCalls % 240) == 0)
    {
        XDBGF("MenuGlyphDbg: draw call=%u idx=%d redraw=%d rect=%d,%d %dx%d menu=%p hover=%ld focus=%ld\n",
              jkGuiBuildMulti_xboxGlyphDrawCalls,
              idx,
              redraw,
              element->rect.x,
              element->rect.y,
              element->rect.width,
              element->rect.height,
              menu,
              menu->lastMouseOverClickable ? (long)(menu->lastMouseOverClickable - menu->paElements) : -1L,
              menu->focusedElement ? (long)(menu->focusedElement - menu->paElements) : -1L);
    }

    jkGuiBuildMulti_XboxBlitGlyphRemapped(vbuf, bitmap, &element->rect);
}

stdBitmap *jkGuiBuildMulti_XboxLoadPortraitCache(const wchar_t *characterName)
{
    stdBitmap *bitmap;
    stdVBufferTexFmt fmt;
    stdVBuffer *vbuf;
    char portraitPath[128];
    int fhand;
    uint32_t magic;
    uint32_t version;
    uint32_t width;
    uint32_t height;
    uint32_t paletteBytes;
    uint8_t *dstBase;

    if (!characterName || !characterName[0])
        return 0;

    XDBG("ProfilePortrait: raw load enter\n");
    jkGuiBuildMulti_XboxPortraitPath(characterName, portraitPath, sizeof(portraitPath));
    if (!portraitPath[0])
        return 0;

    fhand = std_pHS->fileOpen(portraitPath, "rb");
    if (!fhand)
    {
        XDBG("ProfilePortrait: raw load missing\n");
        return 0;
    }

    if (std_pHS->fileRead(fhand, &magic, sizeof(magic)) != sizeof(magic)
        || std_pHS->fileRead(fhand, &version, sizeof(version)) != sizeof(version)
        || std_pHS->fileRead(fhand, &width, sizeof(width)) != sizeof(width)
        || std_pHS->fileRead(fhand, &height, sizeof(height)) != sizeof(height)
        || std_pHS->fileRead(fhand, &paletteBytes, sizeof(paletteBytes)) != sizeof(paletteBytes))
    {
        std_pHS->fileClose(fhand);
        XDBG("ProfilePortrait: raw load header failed\n");
        return 0;
    }

    if (magic != JKGUIMULTI_PORTRAIT_CACHE_MAGIC
        || version != JKGUIMULTI_PORTRAIT_CACHE_VERSION
        || width != JKGUIMULTI_PORTRAIT_CACHE_W
        || height != JKGUIMULTI_PORTRAIT_CACHE_H
        || paletteBytes != sizeof(rdColor24) * 256)
    {
        XDBGF("ProfilePortraitDbg: invalid raw cache='%s' magic=0x%08x version=%u size=%ux%u pal=%u\n",
              portraitPath, magic, version, width, height, paletteBytes);
        std_pHS->fileClose(fhand);
        XDBG("ProfilePortrait: raw load invalid header\n");
        return 0;
    }

    bitmap = (stdBitmap*)std_pHS->alloc(sizeof(stdBitmap));
    if (!bitmap)
    {
        std_pHS->fileClose(fhand);
        XDBG("ProfilePortrait: raw load alloc bitmap failed\n");
        return 0;
    }
    memset(bitmap, 0, sizeof(*bitmap));

    bitmap->palette = std_pHS->alloc(sizeof(rdColor24) * 256);
    bitmap->mipSurfaces = (stdVBuffer**)std_pHS->alloc(sizeof(stdVBuffer*));
    if (!bitmap->palette || !bitmap->mipSurfaces)
    {
        std_pHS->fileClose(fhand);
        stdBitmap_Free(bitmap);
        XDBG("ProfilePortrait: raw load alloc parts failed\n");
        return 0;
    }
    bitmap->mipSurfaces[0] = 0;

    memset(&fmt, 0, sizeof(fmt));
    fmt.width = JKGUIMULTI_PORTRAIT_CACHE_W;
    fmt.height = JKGUIMULTI_PORTRAIT_CACHE_H;
    fmt.width_in_pixels = JKGUIMULTI_PORTRAIT_CACHE_W;
    fmt.width_in_bytes = JKGUIMULTI_PORTRAIT_CACHE_W;
    fmt.texture_size_in_bytes = JKGUIMULTI_PORTRAIT_CACHE_PIXELS;
    fmt.format.bpp = 8;
    fmt.format.is16bit = 0;
    vbuf = stdDisplay_VBufferNew(&fmt, 0, 0, 0);
    if (!vbuf)
    {
        std_pHS->fileClose(fhand);
        stdBitmap_Free(bitmap);
        XDBG("ProfilePortrait: raw load vbuf failed\n");
        return 0;
    }

    bitmap->palFmt = 2;
    bitmap->numMips = 1;
    bitmap->format.bpp = 8;
    bitmap->format.is16bit = 0;
    bitmap->mipSurfaces[0] = vbuf;

    if (std_pHS->fileRead(fhand, bitmap->palette, sizeof(rdColor24) * 256) != sizeof(rdColor24) * 256)
    {
        std_pHS->fileClose(fhand);
        stdBitmap_Free(bitmap);
        XDBG("ProfilePortrait: raw load palette failed\n");
        return 0;
    }

    stdDisplay_VBufferLock(vbuf);
    dstBase = (uint8_t*)vbuf->surface_lock_alloc;
    if (!dstBase || std_pHS->fileRead(fhand, dstBase, JKGUIMULTI_PORTRAIT_CACHE_PIXELS) != JKGUIMULTI_PORTRAIT_CACHE_PIXELS)
    {
        stdDisplay_VBufferUnlock(vbuf);
        std_pHS->fileClose(fhand);
        stdBitmap_Free(bitmap);
        XDBG("ProfilePortrait: raw load pixels failed\n");
        return 0;
    }
    stdDisplay_VBufferUnlock(vbuf);
    std_pHS->fileClose(fhand);

    if (!jkGuiBuildMulti_XboxPortraitBitmapHasContent(bitmap))
    {
        XDBGF("ProfilePortraitDbg: raw cache has no content='%s'\n", portraitPath);
        stdBitmap_Free(bitmap);
        XDBG("ProfilePortrait: raw load blank\n");
        return 0;
    }

    XDBG("ProfilePortrait: raw load ok\n");
    return bitmap;
}

static void jkGuiBuildMulti_XboxWritePortraitCache(const wchar_t *characterName)
{
    char portraitPath[128];
    char nameA[32];
    uint32_t magic = JKGUIMULTI_PORTRAIT_CACHE_MAGIC;
    uint32_t version = JKGUIMULTI_PORTRAIT_CACHE_VERSION;
    uint32_t width = JKGUIMULTI_PORTRAIT_CACHE_W;
    uint32_t height = JKGUIMULTI_PORTRAIT_CACHE_H;
    uint32_t paletteBytes = sizeof(rdColor24) * 256;
    int fhand;

    if (!characterName || !characterName[0])
        return;

    XDBG("ProfilePortrait: raw write enter\n");
    jkGuiBuildMulti_XboxPortraitPath(characterName, portraitPath, sizeof(portraitPath));
    if (!portraitPath[0])
        return;

    if (!jkGuiBuildMulti_xboxLatestPortraitValid)
        jkGuiBuildMulti_XboxCapturePortraitBooth();
    if (!jkGuiBuildMulti_xboxLatestPortraitValid)
    {
        XDBGF("ProfilePortraitDbg: no cached preview character pending\n");
        XDBG("ProfilePortrait: raw write no valid capture\n");
        return;
    }

    stdString_WcharToChar(nameA, (wchar_t*)characterName, 31);
    nameA[31] = 0;

    fhand = std_pHS->fileOpen(portraitPath, "wb");
    if (!fhand)
    {
        XDBGF("ProfilePortraitDbg: failed cache='%s' character='%s'\n", portraitPath, nameA);
        XDBG("ProfilePortrait: raw write open failed\n");
        return;
    }

    if (std_pHS->fileWrite(fhand, &magic, sizeof(magic)) != sizeof(magic)
        || std_pHS->fileWrite(fhand, &version, sizeof(version)) != sizeof(version)
        || std_pHS->fileWrite(fhand, &width, sizeof(width)) != sizeof(width)
        || std_pHS->fileWrite(fhand, &height, sizeof(height)) != sizeof(height)
        || std_pHS->fileWrite(fhand, &paletteBytes, sizeof(paletteBytes)) != sizeof(paletteBytes)
        || std_pHS->fileWrite(fhand, stdDisplay_masterPalette, sizeof(rdColor24) * 256) != sizeof(rdColor24) * 256
        || std_pHS->fileWrite(fhand, jkGuiBuildMulti_xboxLatestPortrait, JKGUIMULTI_PORTRAIT_CACHE_PIXELS) != JKGUIMULTI_PORTRAIT_CACHE_PIXELS)
    {
        XDBGF("ProfilePortraitDbg: failed raw write cache='%s' character='%s'\n", portraitPath, nameA);
        std_pHS->fileClose(fhand);
        XDBG("ProfilePortrait: raw write failed\n");
        return;
    }

    std_pHS->fileClose(fhand);
    XDBGF("ProfilePortraitDbg: wrote raw cache='%s' character='%s'\n", portraitPath, nameA);
    XDBG("ProfilePortrait: raw write ok\n");
}

static void jkGuiBuildMulti_XboxDeletePortraitCache(const wchar_t *characterName)
{
    char portraitPath[128];

    jkGuiBuildMulti_XboxPortraitPath(characterName, portraitPath, sizeof(portraitPath));
    if (!portraitPath[0])
        return;

    stdFileUtil_DelFile(portraitPath);
    XDBG("ProfilePortrait: raw cache deleted for regenerate\n");
}

static int jkGuiBuildMulti_XboxWriteLatestOrGeneratePortraitCache(const wchar_t *characterName)
{
    stdBitmap *portraitBitmap;

    if (!characterName || !characterName[0])
        return 0;

    XDBG("ProfilePortrait: write latest/generate request\n");
    jkGuiBuildMulti_XboxDeletePortraitCache(characterName);
    jkGuiBuildMulti_XboxWritePortraitCache(characterName);

    portraitBitmap = jkGuiBuildMulti_XboxLoadPortraitCache(characterName);
    if (portraitBitmap)
    {
        stdBitmap_Free(portraitBitmap);
        XDBG("ProfilePortrait: write latest verified\n");
        return 1;
    }

    XDBG("ProfilePortrait: write latest missing; ensure fallback\n");
    return jkGuiBuildMulti_XboxEnsurePortraitCache(characterName);
}

static void jkGuiBuildMulti_XboxCaptureLatestPortrait(void)
{
    unsigned char *captureRgb;
    int captureW = JKGUIMULTI_PORTRAIT_W;
    int captureH = JKGUIMULTI_PORTRAIT_H;
    int capturePitch = captureW * 3;
    int x, y;
    int brightPixels = 0;
    int nonBlackPixels = 0;

    captureRgb = (unsigned char*)pHS->alloc(capturePitch * captureH);
    if (!captureRgb)
        return;

    if (!std3D_XboxCaptureBackBufferRGB(JKGUIMULTI_PORTRAIT_X,
                                        JKGUIMULTI_PORTRAIT_Y,
                                        captureW,
                                        captureH,
                                        captureRgb,
                                        capturePitch))
    {
        pHS->free(captureRgb);
        return;
    }
    for (y = 0; y < 74; y++)
    {
        int sy0 = (y * captureH) / 74;
        int sy1 = ((y + 1) * captureH) / 74;
        if (sy1 <= sy0)
            sy1 = sy0 + 1;

        for (x = 0; x < 74; x++)
        {
            int sx0 = (x * captureW) / 74;
            int sx1 = ((x + 1) * captureW) / 74;
            int r = 0, g = 0, b = 0, count = 0;
            int sx, sy;
            rdColor24 color;
            if (sx1 <= sx0)
                sx1 = sx0 + 1;

            for (sy = sy0; sy < sy1; sy++)
            {
                const unsigned char *srcRow = captureRgb + sy * capturePitch;
                for (sx = sx0; sx < sx1; sx++)
                {
                    const unsigned char *src = srcRow + sx * 3;
                    r += src[0];
                    g += src[1];
                    b += src[2];
                    count++;
                }
            }

            if (count <= 0)
                count = 1;
            color.r = (uint8_t)(r / count);
            color.g = (uint8_t)(g / count);
            color.b = (uint8_t)(b / count);
            jkGuiBuildMulti_xboxLatestPortrait[y * 74 + x] = jkGuiBuildMulti_XboxNearestMenuColor(&color);
            if ((int)color.r + (int)color.g + (int)color.b > 72)
                brightPixels++;
            if (color.r || color.g || color.b)
                nonBlackPixels++;
        }
    }
    pHS->free(captureRgb);
    jkGuiBuildMulti_xboxLatestPortraitValid = brightPixels > ((74 * 74) / 32);
    xbox_debug_Printf("ProfilePortrait: visible capture bright=%d nonBlack=%d valid=%d\n",
                      brightPixels,
                      nonBlackPixels,
                      jkGuiBuildMulti_xboxLatestPortraitValid);
    if (jkGuiBuildMulti_xboxLatestPortraitValid)
        XDBG("ProfilePortrait: visible capture ok\n");
    else
        XDBG("ProfilePortrait: visible capture blank\n");
}

static int jkGuiBuildMulti_XboxReadVBufferPixel(stdVBuffer *srcVbuf, int sx, int sy, rdColor24 *color)
{
    uint8_t *srcRow;

    if (!srcVbuf || !srcVbuf->surface_lock_alloc || !color)
        return 0;
    if (sx < 0 || sy < 0 || sx >= srcVbuf->format.width || sy >= srcVbuf->format.height)
        return 0;

    srcRow = (uint8_t*)srcVbuf->surface_lock_alloc + sy * srcVbuf->format.width_in_bytes;
    if (srcVbuf->format.format.is16bit || srcVbuf->format.format.bpp == 16)
    {
        uint16_t pix = *((uint16_t*)(srcRow + sx * 2));
        color->r = (uint8_t)((((pix >> 11) & 0x1F) * 255) / 31);
        color->g = (uint8_t)((((pix >> 5) & 0x3F) * 255) / 63);
        color->b = (uint8_t)(((pix & 0x1F) * 255) / 31);
        return 1;
    }

    {
        uint8_t pix = srcRow[sx];
        *color = stdDisplay_masterPalette[pix];
    }
    return 1;
}

static int jkGuiBuildMulti_XboxCaptureLatestPortraitFromVBuffer(stdVBuffer *srcVbuf, const char *label)
{
    int x, y;
    int brightPixels = 0;
    int nonBlackPixels = 0;
    int totalSamples = 0;

    if (!srcVbuf || !srcVbuf->surface_lock_alloc)
        return 0;

    for (y = 0; y < 74; y++)
    {
        int sy0 = JKGUIMULTI_PORTRAIT_Y - JKGUIMULTI_PREVIEW_Y + (y * JKGUIMULTI_PORTRAIT_H) / 74;
        int sy1 = JKGUIMULTI_PORTRAIT_Y - JKGUIMULTI_PREVIEW_Y + ((y + 1) * JKGUIMULTI_PORTRAIT_H) / 74;
        if (sy1 <= sy0)
            sy1 = sy0 + 1;

        for (x = 0; x < 74; x++)
        {
            int sx0 = JKGUIMULTI_PORTRAIT_X - JKGUIMULTI_PREVIEW_X + (x * JKGUIMULTI_PORTRAIT_W) / 74;
            int sx1 = JKGUIMULTI_PORTRAIT_X - JKGUIMULTI_PREVIEW_X + ((x + 1) * JKGUIMULTI_PORTRAIT_W) / 74;
            int r = 0, g = 0, b = 0, count = 0;
            int sx, sy;
            rdColor24 color;
            uint8_t dstPix;

            if (sx1 <= sx0)
                sx1 = sx0 + 1;

            for (sy = sy0; sy < sy1; sy++)
            {
                for (sx = sx0; sx < sx1; sx++)
                {
                    rdColor24 srcColor;
                    if (!jkGuiBuildMulti_XboxReadVBufferPixel(srcVbuf, sx, sy, &srcColor))
                        continue;
                    r += srcColor.r;
                    g += srcColor.g;
                    b += srcColor.b;
                    count++;
                }
            }

            if (count <= 0)
                count = 1;
            color.r = (uint8_t)(r / count);
            color.g = (uint8_t)(g / count);
            color.b = (uint8_t)(b / count);
            dstPix = jkGuiBuildMulti_XboxNearestMenuColor(&color);
            jkGuiBuildMulti_xboxPortraitCandidate[y * 74 + x] = dstPix;
            if ((int)color.r + (int)color.g + (int)color.b > 72)
                brightPixels++;
            if (color.r || color.g || color.b)
                nonBlackPixels++;
            totalSamples++;
        }
    }

    xbox_debug_Printf("ProfilePortrait: capture src=%s bpp=%u is16=%u size=%dx%d pitch=%u bright=%d nonBlack=%d samples=%d\n",
                      label ? label : "?",
                      srcVbuf->format.format.bpp,
                      srcVbuf->format.format.is16bit,
                      srcVbuf->format.width,
                      srcVbuf->format.height,
                      srcVbuf->format.width_in_bytes,
                      brightPixels,
                      nonBlackPixels,
                      totalSamples);

    if (brightPixels > ((74 * 74) / 32) && nonBlackPixels > ((74 * 74) / 32))
    {
        memcpy(jkGuiBuildMulti_xboxLatestPortrait,
               jkGuiBuildMulti_xboxPortraitCandidate,
               sizeof(jkGuiBuildMulti_xboxLatestPortrait));
        jkGuiBuildMulti_xboxLatestPortraitValid = 1;
        XDBG("ProfilePortrait: offscreen capture ok\n");
        return brightPixels;
    }

    XDBG("ProfilePortrait: offscreen capture blank\n");
    return 0;
}

#endif

static jkGuiElement jkGuiBuildMulti_buttons[17] =
{
  { ELEMENT_TEXT, 0, 5, "GUI_EDIT_CHARACTER", 3, { 240, 20, 400, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXT, 0, 1, NULL, 3, { 240, 60, 400, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXT, 0, 0, NULL, 3, { 30, 60, 140, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXT, 0, 2, NULL, 3, { 310, 90, 270, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_PICBUTTON, 105, 0, NULL, 33, { 6, 90, 24, 24 }, 1, 0, NULL, NULL, jkGuiBuildMulti_SaberButtonClicked, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_PICBUTTON, 104, 0, NULL, 34, { 170, 90, 24, 24 }, 1, 0, NULL, NULL, jkGuiBuildMulti_SaberButtonClicked, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_CUSTOM, 0, 0, NULL, 0, { 315, 115, 260, 260 }, 1, 0, NULL, jkGuiBuildMulti_ModelDrawer, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_CUSTOM, 0, 0, NULL, 0, { 80, 115, 50, 260 }, 1, 0, NULL, jkGuiBuildMulti_SaberDrawer, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXT, 0, 0, "GUI_MODEL", 3, { 336, 380, 216, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
#ifdef TARGET_XBOX
  { ELEMENT_CUSTOM, 0, 0, NULL, JKGUIMULTI_XBTN_LT, { 290, 375, 56, 38 }, 1, 0, NULL, jkGuiBuildMulti_XboxButtonGlyphDraw, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_CUSTOM, 0, 0, NULL, JKGUIMULTI_XBTN_RT, { 542, 375, 56, 38 }, 1, 0, NULL, jkGuiBuildMulti_XboxButtonGlyphDraw, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_CUSTOM, 0, 0, NULL, JKGUIMULTI_XBTN_WHITE, { 50, 373, 40, 40 }, 1, 0, NULL, jkGuiBuildMulti_XboxButtonGlyphDraw, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_CUSTOM, 0, 0, NULL, JKGUIMULTI_XBTN_BLACK, { 110, 373, 40, 40 }, 1, 0, NULL, jkGuiBuildMulti_XboxButtonGlyphDraw, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
#else
  { ELEMENT_TEXT, 0, 2, NULL, 3, { 300, 380, 36, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXT, 0, 2, NULL, 3, { 552, 380, 36, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXT, 0, 2, NULL, 3, { 52, 380, 40, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXT, 0, 2, NULL, 3, { 112, 380, 40, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
#endif
  { ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, { 20, 430, 170, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXTBUTTON, 109, 2, "GUI_FORCEPOWERS", 3, { 290, 430, 170, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_TEXTBUTTON, 106, 2, "GUI_SAVE", 3, { 470, 430, 170, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  { ELEMENT_END, 0, 0, NULL, 0, { 0, 0, 0, 0 }, 0, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 }
};



jkGuiMenu jkGuiBuildMulti_menu =
{
    jkGuiBuildMulti_buttons, -1, 65535, 65535, 15, NULL, NULL, jkGui_stdBitmaps, jkGui_stdFonts, 0, jkGuiBuildMulti_sub_41A120, "thermloop01.wav", "thrmlpu2.wav", NULL, NULL, NULL, 0, NULL, NULL
};



static int32_t listbox_images[2] = {JKGUI_BM_UP_15, JKGUI_BM_DOWN_15};
static int32_t listbox_images2[2] = {JKGUI_BM_UP_15, JKGUI_BM_DOWN_15};

static jkGuiElement jkGuiBuildMulti_menuEditCharacter_buttons[17] =
{
/*00*/  { ELEMENT_TEXT, 0, 0, NULL, 3, { 0, 390, 640, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*01*/  { ELEMENT_TEXT, 0, 5, "GUI_EDIT_CHARACTER", 3, { 240, 20, 400, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*02*/  { ELEMENT_TEXT, 0, 1, NULL, 3, { 240, 60, 400, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*03*/  { ELEMENT_LISTBOX, 1, 0, NULL, 0, { 280, 100, 320, 251 }, 1, 0, NULL, NULL, NULL, listbox_images, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*04*/  { ELEMENT_TEXT, 0, 2, "GUI_NAME", 3, { 0, 130, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*05*/  { ELEMENT_TEXT, 0, 1, NULL, 1, { 0, 150, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  
  // 310, 330
/*06*/  { ELEMENT_TEXT, 0, 2, "GUI_RANKLABEL", 3, { 0, 190, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*07*/  { ELEMENT_TEXT, 0, 1, NULL, 1, { 0, 210, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*08*/  { ELEMENT_TEXT, 0, 2, "GUI_MODEL", 3, { 0, 250, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*09*/  { ELEMENT_TEXT, 0, 1, NULL, 1, { 0, 270, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  
/*10*/  { ELEMENT_TEXT, 0, 2, "GUI_PERSONALITY", 3, { 0, 190, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*11*/  { ELEMENT_TEXT, 0, 1, NULL, 1, { 0, 210, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },

/*12*/  { ELEMENT_TEXTBUTTON, -1, 2, "GUI_DONE", 3, { 30, 430, 128, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*13*/  { ELEMENT_TEXTBUTTON, 100, 2, "GUI_NEW", 3, { 250, 430, 128, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*14*/  { ELEMENT_TEXTBUTTON, 102, 2, "GUI_REMOVE", 3, { 380, 430, 128, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*15*/  { ELEMENT_TEXTBUTTON, 1, 2, "GUI_EDIT", 3, { 510, 430, 130, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*16*/  { ELEMENT_END, 0, 0, NULL, 0, { 0, 0, 0, 0 }, 0, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 }
};

static jkGuiMenu jkGuiBuildMulti_menuEditCharacter =
{
    jkGuiBuildMulti_menuEditCharacter_buttons, -1, 65535, 65535, 15, NULL, NULL, jkGui_stdBitmaps, jkGui_stdFonts, 0, NULL, "thermloop01.wav", "thrmlpu2.wav", NULL, NULL, NULL, 0, NULL, NULL
};

// 13 -> 16
// 12 -> 15
// 11 -> 14
// 10 -> 13
// 7 -> 8
// 6 -> 7
// 5 -> 6?
// 4 -> 5?
// 2 -> 2
static jkGuiElement jkGuiBuildMulti_menuNewCharacter_buttons[18] =
{
/*00*/  { ELEMENT_TEXT, 0, 0, NULL, 3, { 230, 410, 410, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*01*/  { ELEMENT_TEXT, 0, 5, "GUI_NEW_CHARACTER", 3, { 240, 20, 400, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*02*/  { ELEMENT_TEXT, 0, 1, NULL, 3, { 240, 60, 400, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*03*/  { ELEMENT_TEXT, 0, 2, "GUI_NEW_CHARACTER_CONFIG", 3, { 240, 130, 400, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },

/*04 dummy*/  { ELEMENT_TEXT, 0, 0, L"", 3, { 0, 0, 0, 0 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*05*/  { ELEMENT_TEXT, 0, 2, "GUI_MAXSTARS", 3, { 0, 30, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*06*/  { ELEMENT_TEXT, 0, 0, NULL, 3, { 0, 50, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },

/*07*/  { ELEMENT_TEXT, 0, 2, "GUI_RANKLABEL", 3, { 320, 240, 240, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*08*/  { ELEMENT_TEXT, 0, 0, NULL, 3, { 344, 270, 192, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*09*/  { ELEMENT_PICBUTTON, 103, 0, NULL, 33, { 320, 270, 24, 24 }, 1, 0, NULL, NULL, jkGuiBuildMulti_menuNewCharacter_rankArrowButtonClickHandler, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*10*/  { ELEMENT_PICBUTTON, 104, 0, NULL, 34, { 536, 270, 24, 24 }, 1, 0, NULL, NULL, jkGuiBuildMulti_menuNewCharacter_rankArrowButtonClickHandler, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },

/*11 dummy*/  { ELEMENT_TEXT, 0, 0, L"", 3, { 0, 0, 0, 0 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*12 dummy*/  { ELEMENT_TEXT, 0, 0, L"", 3, { 0, 0, 0, 0 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },

/*13*/  { ELEMENT_TEXT, 0, 2, "GUI_NAME", 3, { 320, 170, 240, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*14*/  { ELEMENT_TEXTBOX, 0, 0, NULL, 0, { 320, 200, 240, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*15*/  { ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, { 0, 430, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*16*/  { ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK", 3, { 460, 430, 180, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*17*/  { ELEMENT_END, 0, 0, NULL, 0, { 0, 0, 0, 0 }, 0, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 }
};

static jkGuiMenu jkGuiBuildMulti_menuNewCharacter =
{
    jkGuiBuildMulti_menuNewCharacter_buttons, -1, 65535, 65535, 15, NULL, NULL, jkGui_stdBitmaps, jkGui_stdFonts, 0, NULL, "thermloop01.wav", "thrmlpu2.wav", NULL, NULL, NULL, 0, NULL, NULL
};

static jkGuiElement jkGuiBuildMulti_menuNewCharacter_buttonsMots[18] =
{
/*00*/  { ELEMENT_TEXT,        0,    0,    NULL,    3,    { 0, 100, 200, 320 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*01*/  { ELEMENT_TEXT,        0,    5,    "GUI_NEW_CHARACTER",    3,    { 240, 15, 400, 50 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*02*/  { ELEMENT_TEXT,        0,    1,    NULL,    3,    { 240, 60, 400, 30 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*03*/  { ELEMENT_TEXT,        0,    2,    "GUI_NEW_CHARACTER_CONFIG",    3,    { 240, 110, 400, 20 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*04*/  { ELEMENT_TEXT,        0,    2,    "GUI_TYPEOFGAME",    2,    { 300, 220, 240, 30 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },

/*05*/  { ELEMENT_CHECKBOX,    0,    0,    "GUI_TYPEPERSONALITIES",    0,    { 300, 250, 340, 20 },    1,    0,    NULL,    NULL,    jkGuiBuildMulti_FUN_004209b0,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*06*/  { ELEMENT_CHECKBOX,    0,    0,    "GUI_TYPEJEDIONLY",    0,    { 300, 270, 340, 20 },    1,    0,    NULL,    NULL,    jkGuiBuildMulti_FUN_00420930,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },

// TODO jkGuiBuildMulti_waTmpRankLabel
/*07*/  { ELEMENT_TEXT,        0,    2,   "GUI_RANKLABEL",    2,    { 300, 320, 240, 30 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*08*/  { ELEMENT_TEXT,        0,    0,    NULL,    2,    { 360, 360, 192, 30 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*09*/  { ELEMENT_PICBUTTON,   103,  0,    NULL,    33,    { 300, 360, 24, 24 },    1,    0,    NULL,    NULL,    jkGuiBuildMulti_menuNewCharacter_rankArrowButtonClickHandler,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*10*/  { ELEMENT_PICBUTTON,   104,  0,    NULL,    34,    { 326, 360, 24, 24 },    1,    0,    NULL,    NULL,    jkGuiBuildMulti_menuNewCharacter_rankArrowButtonClickHandler,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },

/*11*/  { ELEMENT_TEXT,        0,    2,    "GUI_PERSONALITY",    2,    { 300, 310, 240, 30 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*12*/  { ELEMENT_LISTBOX,     1,    0,    NULL,    0,    { 300, 345, 240, 66 },    1,    0,    NULL,    NULL,    NULL,    listbox_images2,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },

/*13*/  { ELEMENT_TEXT,        0,    2,    "GUI_NAME",    2,    { 300, 145, 240, 30 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*14*/  { ELEMENT_TEXTBOX,     0,    0,    NULL,    0,    { 300, 180, 240, 20 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*15*/  { ELEMENT_TEXTBUTTON, -1,    2,    "GUI_CANCEL",    3,    { 0, 430, 200, 40 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*16*/  { ELEMENT_TEXTBUTTON,  1,    2,    "GUI_OK",    3,    { 460, 430, 180, 40 },    1,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0 },
/*17*/  { ELEMENT_END,         0,    0,    NULL,    0,    { 0, 0, 0, 0 },    0,    0,    NULL,    NULL,    NULL,    NULL,    { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } },    0}
};

static jkGuiMenu jkGuiBuildMulti_menuNewCharacterMots =
{
    jkGuiBuildMulti_menuNewCharacter_buttonsMots, -1, 65535, 65535, 15, NULL, NULL, jkGui_stdBitmaps, jkGui_stdFonts, 0, NULL, "thermloop01.wav", "thrmlpu2.wav", NULL, NULL, NULL, 0, NULL, NULL
};

static jkGuiElement* jkGuiBuildMulti_pNewCharacterElements = jkGuiBuildMulti_menuNewCharacter_buttons;
static jkGuiMenu* jkGuiBuildMulti_pNewCharacterMenu = &jkGuiBuildMulti_menuNewCharacter;

static jkGuiElement jkGuiBuildMulti_menuLoadCharacter_buttons[24] =
{
/*00*/  { ELEMENT_TEXT, 0, 0, NULL, 3, { 0, 390, 640, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*01*/  { ELEMENT_TEXT, 0, 5, "GUI_LOAD_CHARACTER", 3, { 240, 20, 400, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*02*/  { ELEMENT_TEXT, 0, 1, NULL, 3, { 240, 60, 400, 30 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*03*/  { ELEMENT_LISTBOX, 1, 0, NULL, 0, { 280, 100, 320, 251 }, 1, 0, NULL, NULL, NULL, listbox_images, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*04*/  { ELEMENT_TEXT, 0, 2, "GUI_SLEPISODE", 3, { 0, 30, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*05*/  { ELEMENT_TEXT, 0, 0, NULL, 1, { 0, 50, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*06*/  { ELEMENT_TEXT, 0, 2, "GUI_SLLEVEL", 3, { 0, 90, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*07*/  { ELEMENT_TEXT, 0, 0, NULL, 1, { 0, 110, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*08*/  { ELEMENT_TEXT, 0, 2, "GUI_MAXSTARS", 3, { 0, 150, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*09*/  { ELEMENT_TEXT, 0, 0, NULL, 1, { 0, 170, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*10*/  { ELEMENT_TEXT, 0, 2, "GUI_NAME", 3, { 0, 210, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*11*/  { ELEMENT_TEXT, 0, 1, NULL, 1, { 0, 230, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*12*/  { ELEMENT_TEXT, 0, 2, "GUI_RANKLABEL", 3, { 0, 270, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*13*/  { ELEMENT_TEXT, 0, 1, NULL, 1, { 0, 290, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*14*/  { ELEMENT_TEXT, 0, 2, "GUI_MODEL", 3, { 0, 330, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*15*/  { ELEMENT_TEXT, 0, 1, NULL, 1, { 0, 350, 200, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
  
/*16*/  { ELEMENT_TEXT, 0, 2, "GUI_PERSONALITY", 3, { 0, 390, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*17*/  { ELEMENT_TEXT, 0, 1, NULL, 1, { 0, 410, 200, 20 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },

/*18*/  { ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, { 0, 430, 128, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*19*/  { ELEMENT_TEXTBUTTON, 100, 2, "GUI_NEW", 3, { 128, 430, 128, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*20*/  { ELEMENT_TEXTBUTTON, 102, 2, "GUI_REMOVE", 3, { 256, 430, 128, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*21*/  { ELEMENT_TEXTBUTTON, 101, 2, "GUI_EDIT", 3, { 384, 430, 128, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*22*/  { ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK", 3, { 512, 430, 128, 40 }, 1, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 },
/*23*/  { ELEMENT_END, 0, 0, NULL, 0, { 0, 0, 0, 0 }, 0, 0, NULL, NULL, NULL, NULL, { 0, 0, 0, 0, 0, { 0, 0, 0, 0 } }, 0 }
};



static jkGuiMenu jkGuiBuildMulti_menuLoadCharacter =
{
    jkGuiBuildMulti_menuLoadCharacter_buttons, -1, 65535, 65535, 15, NULL, NULL, jkGui_stdBitmaps, jkGui_stdFonts, 0, NULL, "thermloop01.wav", "thrmlpu2.wav", NULL, NULL, NULL, 0, NULL, NULL
};

static int32_t jkGuiBuildMulti_bInitted = 0;
static const wchar_t jkGuiBuildMulti_wLtLabel[] = L"LT";
static const wchar_t jkGuiBuildMulti_wRtLabel[] = L"RT";
static const wchar_t jkGuiBuildMulti_wLbLabel[] = L"LB";
static const wchar_t jkGuiBuildMulti_wRbLabel[] = L"RB";
static wchar_t jkGuiBuildMulti_wPlayerShortName[64];
static jkPlayerMpcInfo jkGuiBuildMulti_aMpcInfo[32];
static wchar_t jkGuiBuildMulti_wTmp[128];
static wchar_t jkGuiBuildMulti_wTmp2[32];
static wchar_t jkGuiBuildMulti_wTmp3[32];
static wchar_t jkGuiBuildMulti_aWchar_5594C8[48];
static rdMaterialLoader_t jkGuiBuildMulti_fnMatLoader;
static model3Loader_t jkGuiBuildMulti_fnModelLoader;
static keyframeLoader_t jkGuiBuildMulti_fnKeyframeLoader;

static rdCanvas *jkGuiBuildMulti_pCanvas = NULL;
static rdCamera *jkGuiBuildMulti_pCamera = NULL;
static rdModel3 *jkGuiBuildMulti_model = NULL;
static rdModel3 *jkGuiBuildMulti_pModelGun = NULL;
static rdKeyframe *jkGuiBuildMulti_keyframe = NULL;
static rdThing *jkGuiBuildMulti_pThingCamera = NULL;
static rdThing *jkGuiBuildMulti_thing = NULL;
static rdThing *jkGuiBuildMulti_pThingGun = NULL;
static uint32_t jkGuiBuildMulti_startTimeSecs = 0; // Added: float -> u32
static rdColormap jkGuiBuildMulti_colormap;
static rdLight jkGuiBuildMulti_light;
static rdMatrix34 jkGuiBuildMulti_matrix;
static int32_t jkGuiBuildMulti_trackNum = 0;
static wchar_t jkGuiBuildMulti_waTmp[128];
static wchar_t jkGuiBuildMulti_waTmp2[32];
static stdBitmap **jkGuiBuildMulti_apSaberBitmaps = NULL;
static jkSaberInfo *jkGame_aSabers = NULL;
static int32_t jkGuiBuildMulti_bSabersLoaded = 0;
static int32_t jkGuiBuildMulti_bEditShowing = 0;
static int32_t jkGuiBuildMulti_numModels = 0;
static int32_t jkGuiBuildMulti_numSabers = 0;
static int32_t jkGuiBuildMulti_saberIdx = 0;
static int32_t jkGuiBuildMulti_modelIdx = 0;
#ifdef TARGET_XBOX
static wchar_t jkGuiBuildMulti_wNoCharacters[] = L"No characters. Choose New.";
#endif
static jkMultiModelInfo *jkGuiBuildMulti_aModels = NULL;
static int32_t jkGuiBuildMulti_renderOptions = 0x103;
static rdVector3 jkGuiBuildMulti_projectRot;
static rdVector3 jkGuiBuildMulti_projectPos;
static stdVBufferTexFmt jkGuiBuildMulti_texFmt;
static rdMatrix34 jkGuiBuildMulti_orthoProjection;
static rdVector3 jkGuiBuildMulti_lightPos;
static uint32_t jkGuiBuildMulti_lastModelDrawMs;
static int32_t jkGuiBuildMulti_savedAcceleration;
static int32_t jkGuiBuildMulti_renderOpen;

static void jkGuiBuildMulti_FreeLoadedCharacterLists(void)
{
    int32_t i;
    int32_t saberCount;

    saberCount = jkGuiBuildMulti_numSabers;

    if (jkGuiBuildMulti_aModels)
    {
        pHS->free(jkGuiBuildMulti_aModels);
        jkGuiBuildMulti_aModels = NULL;
    }
    if (jkGame_aSabers)
    {
        pHS->free(jkGame_aSabers);
        jkGame_aSabers = NULL;
    }
    if (jkGuiBuildMulti_apSaberBitmaps)
    {
        for (i = 0; i < saberCount; ++i)
        {
            if (jkGuiBuildMulti_apSaberBitmaps[i])
            {
                stdBitmap_Free(jkGuiBuildMulti_apSaberBitmaps[i]);
                jkGuiBuildMulti_apSaberBitmaps[i] = NULL;
            }
        }
        pHS->free(jkGuiBuildMulti_apSaberBitmaps);
        jkGuiBuildMulti_apSaberBitmaps = NULL;
    }

    jkGuiBuildMulti_numModels = 0;
    jkGuiBuildMulti_numSabers = 0;
    jkGuiBuildMulti_bSabersLoaded = 0;
    jkGuiBuildMulti_bEditShowing = 0;
}

static int jkGuiBuildMulti_FailEditCharacterStartup(void)
{
    jkGuiBuildMulti_FreeLoadedCharacterLists();
    jkGui_SetModeGame();
    stdBitmap_UnloadData(jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_MULTI]);
    return 0;
}
#ifdef TARGET_XBOX
static unsigned int jkGuiBuildMulti_xboxPostDrawCalls;
static unsigned int jkGuiBuildMulti_xboxModelDrawerCalls;

static void jkGuiBuildMulti_XboxCaptureVisiblePortraitBooth(void)
{
    rdPuppet savedPuppet;
    rdMatrix34 savedMatrix;
    rdVector3 portraitRot;
    int savedAcceleration;

    if (!jkGuiBuildMulti_thing || !jkGuiBuildMulti_thing->puppet || !jkGuiBuildMulti_pCamera)
    {
        XDBG("ProfilePortrait: visible booth missing resources\n");
        return;
    }

    XDBG("ProfilePortrait: visible booth enter\n");
    memcpy(&savedPuppet, jkGuiBuildMulti_thing->puppet, sizeof(savedPuppet));
    rdMatrix_Copy34(&savedMatrix, &jkGuiBuildMulti_matrix);
    savedAcceleration = rdroid_curAcceleration;

    rdPuppet_ResetTrack(jkGuiBuildMulti_thing->puppet, jkGuiBuildMulti_trackNum);
    rdPuppet_UpdateTracks(jkGuiBuildMulti_thing->puppet, 0.0f);
    jkGuiBuildMulti_thing->puppet->paused = 1;
    rdMatrix_Copy34(&jkGuiBuildMulti_matrix, &rdroid_identMatrix34);
    portraitRot.x = 0.0f;
    portraitRot.y = 28.0f;
    portraitRot.z = 0.0f;
    rdMatrix_PostRotate34(&jkGuiBuildMulti_matrix, &portraitRot);

    rdroid_curAcceleration = 1;
    std3D_XboxSetScreenSpaceRenderList(1);
    rdAdvanceFrame();
    std3D_XboxSetViewport(JKGUIMULTI_PREVIEW_X,
                          480 - JKGUIMULTI_PREVIEW_Y - JKGUIMULTI_PREVIEW_H,
                          JKGUIMULTI_PREVIEW_W,
                          JKGUIMULTI_PREVIEW_H);
    std3D_ClearZBuffer();
    rdCamera_SetCurrent(jkGuiBuildMulti_pCamera);
    rdThing_Draw(jkGuiBuildMulti_thing, &jkGuiBuildMulti_matrix);
    if (jkGuiBuildMulti_pThingGun && jkGuiBuildMulti_thing->hierarchyNodeMatrices)
        rdThing_Draw(jkGuiBuildMulti_pThingGun, jkGuiBuildMulti_thing->hierarchyNodeMatrices + 12);
    rdFinishFrame();
    jkGuiBuildMulti_XboxCaptureLatestPortrait();
    std3D_XboxSetScreenSpaceRenderList(0);
    std3D_XboxResetViewport();
    rdroid_curAcceleration = savedAcceleration;

    memcpy(jkGuiBuildMulti_thing->puppet, &savedPuppet, sizeof(savedPuppet));
    rdMatrix_Copy34(&jkGuiBuildMulti_matrix, &savedMatrix);
    XDBG("ProfilePortrait: visible booth exit\n");
}

static void jkGuiBuildMulti_XboxCapturePortraitBooth(void)
{
    rdPuppet savedPuppet;
    rdMatrix34 savedMatrix;
    rdVector3 portraitRot;
    int savedAcceleration;

    if (!jkGuiBuildMulti_thing || !jkGuiBuildMulti_thing->puppet || !jkGuiBuildMulti_pCamera || !jkGuiBuildMulti_pVBuf1)
    {
        XDBG("ProfilePortrait: booth missing resources\n");
        return;
    }

    XDBG("ProfilePortrait: booth enter\n");
    memcpy(&savedPuppet, jkGuiBuildMulti_thing->puppet, sizeof(savedPuppet));
    rdMatrix_Copy34(&savedMatrix, &jkGuiBuildMulti_matrix);
    savedAcceleration = rdroid_curAcceleration;

    rdPuppet_ResetTrack(jkGuiBuildMulti_thing->puppet, jkGuiBuildMulti_trackNum);
    rdPuppet_UpdateTracks(jkGuiBuildMulti_thing->puppet, 0.0f);
    jkGuiBuildMulti_thing->puppet->paused = 1;
    rdMatrix_Copy34(&jkGuiBuildMulti_matrix, &rdroid_identMatrix34);
    portraitRot.x = 0.0f;
    portraitRot.y = 28.0f;
    portraitRot.z = 0.0f;
    rdMatrix_PostRotate34(&jkGuiBuildMulti_matrix, &portraitRot);

    rdroid_curAcceleration = 0;
    stdDisplay_VBufferFill(jkGuiBuildMulti_pVBuf1, 0, 0);
    if (jkGuiBuildMulti_pVBuf2)
        stdDisplay_VBufferFill(jkGuiBuildMulti_pVBuf2, 0, 0);
    stdDisplay_VBufferLock(jkGuiBuildMulti_pVBuf1);
    if (jkGuiBuildMulti_pVBuf2)
        stdDisplay_VBufferLock(jkGuiBuildMulti_pVBuf2);
    rdAdvanceFrame();
    rdCamera_SetCurrent(jkGuiBuildMulti_pCamera);
    rdThing_Draw(jkGuiBuildMulti_thing, &jkGuiBuildMulti_matrix);
    rdFinishFrame();
    jkGuiBuildMulti_xboxLatestPortraitValid = 0;
    if (jkGuiBuildMulti_pVBuf2)
    {
        XDBG("ProfilePortrait: capture try vbuf2\n");
        jkGuiBuildMulti_XboxCaptureLatestPortraitFromVBuffer(jkGuiBuildMulti_pVBuf2, "vbuf2");
    }
    if (!jkGuiBuildMulti_xboxLatestPortraitValid)
    {
        XDBG("ProfilePortrait: capture try vbuf1\n");
        jkGuiBuildMulti_XboxCaptureLatestPortraitFromVBuffer(jkGuiBuildMulti_pVBuf1, "vbuf1");
    }
    if (jkGuiBuildMulti_pVBuf2)
        stdDisplay_VBufferUnlock(jkGuiBuildMulti_pVBuf2);
    stdDisplay_VBufferUnlock(jkGuiBuildMulti_pVBuf1);
    rdroid_curAcceleration = savedAcceleration;

    memcpy(jkGuiBuildMulti_thing->puppet, &savedPuppet, sizeof(savedPuppet));
    rdMatrix_Copy34(&jkGuiBuildMulti_matrix, &savedMatrix);
    XDBG("ProfilePortrait: booth exit\n");
}

int jkGuiBuildMulti_XboxEnsurePortraitCache(const wchar_t *characterName)
{
    jkPlayerMpcInfo mpcInfo;
    sithPlayerInfo playerInfo;
    stdBitmap *portraitBitmap;
    int savedRendering;
    int wrotePortrait;

    if (!characterName || !characterName[0])
        return 0;

    XDBG("ProfilePortrait: ensure enter\n");
    portraitBitmap = jkGuiBuildMulti_XboxLoadPortraitCache(characterName);
    if (portraitBitmap)
    {
        stdBitmap_Free(portraitBitmap);
        XDBG("ProfilePortrait: ensure existing ok\n");
        return 1;
    }

    memset(&mpcInfo, 0, sizeof(mpcInfo));
    memset(&playerInfo, 0, sizeof(playerInfo));
    if (!jkPlayer_MPCParse(&mpcInfo, &playerInfo, jkPlayer_playerShortName, (wchar_t*)characterName, 0))
    {
        XDBGF("ProfilePortraitDbg: unable to parse mpc character='%ls'\n", characterName);
        XDBG("ProfilePortrait: ensure parse failed\n");
        return 0;
    }

    if (jkGuiBuildMulti_renderOpen)
    {
        XDBGF("ProfilePortraitDbg: render already open, skipping cache character='%ls'\n", characterName);
        XDBG("ProfilePortrait: ensure render already open\n");
        return 0;
    }

    wrotePortrait = 0;
    savedRendering = jkGuiBuildMulti_bRendering;
    jkGuiBuildMulti_bRendering = 1;
    jkGuiBuildMulti_xboxLatestPortraitValid = 0;

    if (jkGuiBuildMulti_DisplayModel())
    {
        jkGuiBuildMulti_ThingInit(mpcInfo.model);
        if (jkGuiBuildMulti_thing && jkGuiBuildMulti_thing->puppet && jkGuiBuildMulti_pCamera)
        {
            XDBG("ProfilePortrait: ensure drawing booth\n");
            std3D_XboxSetScreenSpaceRenderList(1);
            std3D_XboxSetViewport(JKGUIMULTI_PREVIEW_X,
                                  480 - JKGUIMULTI_PREVIEW_Y - JKGUIMULTI_PREVIEW_H,
                                  JKGUIMULTI_PREVIEW_W,
                                  JKGUIMULTI_PREVIEW_H);
            rdAdvanceFrame();
            jkGuiBuildMulti_XboxCapturePortraitBooth();
            std3D_XboxSetScreenSpaceRenderList(0);
            std3D_XboxResetViewport();
            jkGuiBuildMulti_XboxWritePortraitCache(characterName);
            wrotePortrait = jkGuiBuildMulti_xboxLatestPortraitValid;
        }
        else
        {
            XDBG("ProfilePortrait: ensure thing missing\n");
        }
        jkGuiBuildMulti_ThingCleanup();
    }
    else
    {
        XDBG("ProfilePortrait: ensure display model failed\n");
    }

    jkGuiBuildMulti_CloseRender();
    jkGuiBuildMulti_bRendering = savedRendering;

    if (!wrotePortrait)
    {
        XDBG("ProfilePortrait: ensure no portrait written\n");
        return 0;
    }

    portraitBitmap = jkGuiBuildMulti_XboxLoadPortraitCache(characterName);
    if (!portraitBitmap)
    {
        XDBG("ProfilePortrait: ensure reload failed\n");
        return 0;
    }
    stdBitmap_Free(portraitBitmap);
    XDBG("ProfilePortrait: ensure ok\n");
    return 1;
}
#endif

static wchar_t jkGuiBuildMulti_waTmpRankLabel[128+1];

static rdRect jkGuiBuildMulti_rect_5353C8 = {315, 115, 260, 260};

#ifndef QOL_IMPROVEMENTS
#define BUILDMULTI_SWITCH_DELAY_MS (1000)
#else
#define BUILDMULTI_SWITCH_DELAY_MS (10)
#endif

static void jkGuiBuildMulti_MakeGeneratedCharacterName(wchar_t *out, int outLen)
{
    char modelShortName[16];
    wchar_t *modelName;

    if (outLen <= 0)
        return;

    if (jkGuiBuildMulti_aModels && jkGuiBuildMulti_modelIdx >= 0 && jkGuiBuildMulti_modelIdx < jkGuiBuildMulti_numModels)
    {
        stdFnames_CopyShortName(modelShortName, 16, jkGuiBuildMulti_aModels[jkGuiBuildMulti_modelIdx].modelFpath);
        jkGuiTitle_sub_4189A0(modelShortName);
        modelName = jkStrings_GetUniStringWithFallback(modelShortName);
    }
    else
    {
        modelName = L"Character";
    }

    jk_snwprintf(out, outLen, L"%s%d", modelName, jkPlayer_GetJediRank());
    out[outLen - 1] = 0;
}

void jkGuiBuildMulti_StartupEditCharacter()
{
    jkGui_InitMenu(&jkGuiBuildMulti_menu, jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_MULTI]);
}

void jkGuiBuildMulti_ShutdownEditCharacter()
{
    // Added: clean reset
    jkGuiBuildMulti_jediRank = 0;
    jkGuiBuildMulti_bRendering = 0;

    ;
}

rdModel3* jkGuiBuildMulti_ModelLoader(const char *pCharFpath, int unused)
{
    rdModel3 *pModel; // esi
    char fpath[128]; // [esp+4h] [ebp-80h] BYREF

    __snprintf(fpath, 128, "%s%c%s", "3do", '\\', pCharFpath); // ADDED: sprintf -> snprintf
    pModel = (rdModel3 *)pHS->alloc(sizeof(rdModel3));
    if (!pModel)
        return NULL;
    memset(pModel, 0, sizeof(rdModel3));
    if (rdModel3_Load(fpath, pModel) != 0)
        return pModel;

    rdModel3_FreeEntry(pModel);
    pHS->free(pModel);
    return NULL;
}

rdMaterial* jkGuiBuildMulti_MatLoader(const char *pMatFname, int a, int b)
{
    rdMaterial *pMaterial; // esi
    char mat_fpath[128]; // [esp+8h] [ebp-80h] BYREF
    int loaded;

    pMaterial = (rdMaterial *)pHS->alloc(sizeof(rdMaterial));
    if (!pMaterial)
        return NULL;
    memset(pMaterial, 0, sizeof(rdMaterial));
    _sprintf(mat_fpath, "3do%cmat%c%s", '\\', '\\', pMatFname);
    loaded = rdMaterial_LoadEntry(mat_fpath, pMaterial, 0, 0);
    if ( !loaded )
    {
        rdMaterial_FreeEntry(pMaterial);
        memset(pMaterial, 0, sizeof(rdMaterial));
        _sprintf(mat_fpath, "mat%c%s", '\\', pMatFname);
        loaded = rdMaterial_LoadEntry(mat_fpath, pMaterial, 0, 0);
    }
    if (!loaded)
    {
        rdMaterial_FreeEntry(pMaterial);
        pHS->free(pMaterial);
        return NULL;
    }
    rdMaterial_EnsureData(pMaterial); // Added: TWL
    return pMaterial;
}

rdKeyframe* jkGuiBuildMulti_KeyframeLoader(const char *pKeyframeFname)
{
    rdKeyframe *pKeyframe; // esi
    char key_fpath[128]; // [esp+4h] [ebp-80h] BYREF

    pKeyframe = (rdKeyframe *)pHS->alloc(sizeof(rdKeyframe));
    if (!pKeyframe)
        return NULL;
    memset(pKeyframe, 0, sizeof(rdKeyframe));
    _sprintf(key_fpath, "3do%ckey%c%s", '\\', '\\', pKeyframeFname);
    if (!rdKeyframe_LoadEntry(key_fpath, pKeyframe))
    {
        rdKeyframe_FreeEntry(pKeyframe);
        pHS->free(pKeyframe);
        return NULL;
    }
    return pKeyframe;
}

void jkGuiBuildMulti_CloseRender()
{
    if (!jkGuiBuildMulti_renderOpen)
        return;

#ifdef TARGET_XBOX
    stdDisplay_XboxSetPostMenuDrawCallback(NULL, NULL);
#endif
    rdMaterial_RegisterLoader(jkGuiBuildMulti_fnMatLoader);
    rdModel3_RegisterLoader(jkGuiBuildMulti_fnModelLoader);
    rdKeyframe_RegisterLoader(jkGuiBuildMulti_fnKeyframeLoader);
    if (jkGuiBuildMulti_pThingGun)
        rdThing_Free(jkGuiBuildMulti_pThingGun);
    if (jkGuiBuildMulti_pModelGun)
        rdModel3_Free(jkGuiBuildMulti_pModelGun);
    rdLight_FreeEntry(&jkGuiBuildMulti_light);
    if (jkGuiBuildMulti_pThingCamera)
        rdThing_Free(jkGuiBuildMulti_pThingCamera);
    if (jkGuiBuildMulti_pCanvas)
        rdCanvas_Free(jkGuiBuildMulti_pCanvas);
    if (jkGuiBuildMulti_pCamera)
        rdCamera_Free(jkGuiBuildMulti_pCamera);
    if (jkGuiBuildMulti_pVBuf1)
        stdDisplay_VBufferFree(jkGuiBuildMulti_pVBuf1);
    if (jkGuiBuildMulti_pVBuf2)
        stdDisplay_VBufferFree(jkGuiBuildMulti_pVBuf2);
    rdColormap_FreeEntry(&jkGuiBuildMulti_colormap);
    rdClose();
    rdroid_curAcceleration = jkGuiBuildMulti_savedAcceleration;

    jkGuiBuildMulti_pThingGun = NULL;
    jkGuiBuildMulti_pModelGun = NULL;
    jkGuiBuildMulti_pThingCamera = NULL;
    jkGuiBuildMulti_pCanvas = NULL;
    jkGuiBuildMulti_pCamera = NULL;
    jkGuiBuildMulti_pVBuf1 = NULL;
    jkGuiBuildMulti_pVBuf2 = NULL;
    jkGuiBuildMulti_renderOpen = 0;
#ifdef TARGET_XBOX
    jkGuiBuildMulti_xboxPostDrawCalls = 0;
    jkGuiBuildMulti_xboxModelDrawerCalls = 0;
#endif
}

void jkGuiBuildMulti_ThingInit(char *pModelFpath)
{
    rdPuppet *pPuppet; // [esp-8h] [ebp-18h]

    int32_t tmp = jkGuiBuildMulti_bRendering; // Added
    jkGuiBuildMulti_bRendering = 1; // Added

    jkGuiBuildMulti_model = rdModel3_New(pModelFpath);
#ifdef TARGET_XBOX
    jkGuiBuildMulti_xboxLatestPortraitValid = 0;
#endif
    jkGuiBuildMulti_thing = rdThing_New(0);
    rdThing_SetModel3(jkGuiBuildMulti_thing, jkGuiBuildMulti_model);
    jkGuiBuildMulti_thing->puppet = rdPuppet_New(jkGuiBuildMulti_thing);
    jkGuiBuildMulti_keyframe = rdKeyframe_Load("kyrun1.key");
    jkGuiBuildMulti_trackNum = rdPuppet_AddTrack(jkGuiBuildMulti_thing->puppet, jkGuiBuildMulti_keyframe, 0, 0);
    pPuppet = jkGuiBuildMulti_thing->puppet;
    jkGuiBuildMulti_startTimeSecs = stdPlatform_GetTimeMsec(); // Added: float -> u32, sec -> ms
    rdPuppet_PlayTrack(pPuppet, jkGuiBuildMulti_trackNum);
    rdPuppet_SetTrackSpeed(jkGuiBuildMulti_thing->puppet, jkGuiBuildMulti_trackNum, 150.0);
    _memcpy(&jkGuiBuildMulti_matrix, &rdroid_identMatrix34, sizeof(jkGuiBuildMulti_matrix));

    jkGuiBuildMulti_bRendering = tmp; // Added
}

void jkGuiBuildMulti_ThingCleanup()
{
    int32_t tmp = jkGuiBuildMulti_bRendering; // Added
    jkGuiBuildMulti_bRendering = 1; // Added

    // Added
    //std3D_PurgeTextureCache();

    if (jkGuiBuildMulti_thing && jkGuiBuildMulti_thing->puppet)
        rdPuppet_ResetTrack(jkGuiBuildMulti_thing->puppet, jkGuiBuildMulti_trackNum);
    if (jkGuiBuildMulti_keyframe)
        rdKeyframe_FreeEntry(jkGuiBuildMulti_keyframe);
    if (jkGuiBuildMulti_thing)
        rdThing_Free(jkGuiBuildMulti_thing);
    if (jkGuiBuildMulti_model)
        rdModel3_Free(jkGuiBuildMulti_model);
    jkGuiBuildMulti_keyframe = NULL;
    jkGuiBuildMulti_thing = NULL;
    jkGuiBuildMulti_model = NULL;

    jkGuiBuildMulti_bRendering = tmp; // Added
}

#ifdef TARGET_XBOX
static void jkGuiBuildMulti_XboxPostMenuDraw(void *ctx)
{
    jkGuiElement *pElement = &jkGuiBuildMulti_buttons[6];
    uint32_t v5;
    flex_d_t v6;
    flex_t a2a;
    rdVector3 rot;

    (void)ctx;

    jkGuiBuildMulti_xboxPostDrawCalls++;
    if (jkGuiBuildMulti_xboxPostDrawCalls <= 30 || (jkGuiBuildMulti_xboxPostDrawCalls % 300) == 0)
    {
        XDBGF("MenuFlickerDbg: postDraw call=%u suspended=%d renderOpen=%d rendering=%d lastSwitch=%u model=%p puppet=%p menuDrawCalls=%u\n",
              jkGuiBuildMulti_xboxPostDrawCalls,
              g_app_suspended,
              jkGuiBuildMulti_renderOpen,
              jkGuiBuildMulti_bRendering,
              jkGuiBuildMulti_lastModelDrawMs,
              jkGuiBuildMulti_thing,
              jkGuiBuildMulti_thing ? jkGuiBuildMulti_thing->puppet : NULL,
              jkGuiBuildMulti_xboxModelDrawerCalls);
    }

    if (!g_app_suspended || !jkGuiBuildMulti_renderOpen || !jkGuiBuildMulti_bRendering)
        return;

    if (jkGuiBuildMulti_lastModelDrawMs)
    {
        if (stdPlatform_GetTimeMsec() - (uint32_t)jkGuiBuildMulti_lastModelDrawMs <= BUILDMULTI_SWITCH_DELAY_MS)
            return;

        jkGuiBuildMulti_ThingCleanup();
        if (jkGuiBuildMulti_aModels && jkGuiBuildMulti_modelIdx >= 0 && jkGuiBuildMulti_modelIdx < jkGuiBuildMulti_numModels)
            jkGuiBuildMulti_ThingInit(jkGuiBuildMulti_aModels[jkGuiBuildMulti_modelIdx].modelFpath);
        else
        {
            XDBGF("BuildMulti3D: PostDraw no model after switch idx=%d num=%d models=%p\n",
                  jkGuiBuildMulti_modelIdx,
                  jkGuiBuildMulti_numModels,
                  jkGuiBuildMulti_aModels);
            return;
        }
        jkGuiBuildMulti_lastModelDrawMs = 0;
    }

    if (!jkGuiBuildMulti_thing || !jkGuiBuildMulti_thing->puppet || !jkGuiBuildMulti_pVBuf1 || !jkGuiBuildMulti_pCamera)
        return;

    stdControl_ShowCursor(1);
    std3D_XboxSetScreenSpaceRenderList(1);
    rdAdvanceFrame();
    std3D_XboxSetViewport(
        pElement->rect.x,
        480 - pElement->rect.y - pElement->rect.height,
        pElement->rect.width,
        pElement->rect.height);
    std3D_ClearZBuffer();

    v5 = stdPlatform_GetTimeMsec();
    v6 = (v5 - jkGuiBuildMulti_startTimeSecs) * 0.001;
    if (v6 < 0.0)
        a2a = 0.0;
    else if (v6 > 1.0)
        a2a = 1.0;
    else
        a2a = v6;

    rdCamera_SetCurrent(jkGuiBuildMulti_pCamera);
    rdPuppet_UpdateTracks(jkGuiBuildMulti_thing->puppet, a2a);
    jkGuiBuildMulti_startTimeSecs = v5;
    rdThing_Draw(jkGuiBuildMulti_thing, &jkGuiBuildMulti_matrix);
    if (jkGuiBuildMulti_pThingGun && jkGuiBuildMulti_thing->hierarchyNodeMatrices)
        rdThing_Draw(jkGuiBuildMulti_pThingGun, jkGuiBuildMulti_thing->hierarchyNodeMatrices + 12);
    rdFinishFrame();

    rot.x = 0.0;
    rot.z = 0.0;
    rot.y = a2a * 20.0;
    rdMatrix_PostRotate34(&jkGuiBuildMulti_matrix, &rot);

    std3D_ClearZBuffer();
    rdCamera_SetCurrent(jkGuiBuildMulti_pCamera);
    rdThing_Draw(jkGuiBuildMulti_thing, &jkGuiBuildMulti_matrix);
    if (jkGuiBuildMulti_pThingGun && jkGuiBuildMulti_thing->hierarchyNodeMatrices)
        rdThing_Draw(jkGuiBuildMulti_pThingGun, jkGuiBuildMulti_thing->hierarchyNodeMatrices + 12);
    rdFinishFrame();
    std3D_XboxSetScreenSpaceRenderList(0);
    std3D_XboxResetViewport();

    stdControl_ShowCursor(0);
}
#endif

// MOTS altered
int jkGuiBuildMulti_ShowEditCharacter(BOOL bIdk)
{
    int32_t v1; // esi
    wchar_t *v2; // eax
    wchar_t *v3; // eax
    int32_t v4; // esi
    jkSaberInfo *v5; // ecx
    jkSaberInfo *v6; // ecx
    stdBitmap *v7; // eax
    int32_t v8; // ebp
    jkSaberInfo * v9; // edi
    jkMultiModelInfo *v10; // eax
    int32_t v11; // eax
    int32_t v12; // edi
    jkMultiModelInfo *v13; // ebp
    rdPuppet *v14; // eax
    wchar_t *v15; // eax
    int32_t v16; // esi
    int32_t v17; // eax
    int32_t v18; // edi
    int32_t previewStarted; // Added
    wchar_t *v21; // [esp-4h] [ebp-190h]
    int32_t idx; // [esp+10h] [ebp-17Ch] BYREF
    int32_t _v23;
    int64_t v23; // [esp+14h] [ebp-178h]
    char v24[32]; // [esp+1Ch] [ebp-170h] BYREF
    char tmp1[32]; // [esp+2Ch] [ebp-160h] BYREF
    char tmp2[32]; // [esp+4Ch] [ebp-140h] BYREF
    char tmp3[32]; // [esp+6Ch] [ebp-120h] BYREF
    char v28[32]; // [esp+8Ch] [ebp-100h] BYREF
    char v32[32]; // [esp+ACh] [ebp-E0h] BYREF
    char v33[32]; // [esp+CCh] [ebp-C0h] BYREF
    char v34[32]; // [esp+ECh] [ebp-A0h] BYREF
    char FileName[128]; // [esp+10Ch] [ebp-80h] BYREF

    // Added
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_MULTI]);
    previewStarted = 0;

    memset(v28, 0, sizeof(v28));
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_MULTI]->palette);
    v1 = jkPlayer_GetJediRank();
    stdString_snprintf(v24, 32, "RANK_%d_L", v1);
    v21 = jkStrings_GetUniStringWithFallback(v24);
    v2 = jkStrings_GetUniStringWithFallback("GUI_RANK");
    jk_snwprintf(jkGuiBuildMulti_waTmp, 0x80u, v2, v1, v21);
    jkGuiBuildMulti_buttons[2].wstr = jkGuiBuildMulti_waTmp;

    if (jkPlayer_personality != 1) {
        jkGuiBuildMulti_buttons[2].bIsVisible = 0;
    }
    else {
        jkGuiBuildMulti_buttons[2].bIsVisible = 1; // Added: Fix an LEC bug where the rank text disappeared forever
    }

    v3 = jkStrings_GetUniStringWithFallback("GUI_S_MULTIPLAYER_CHARACTERS");
    jk_snwprintf(&jkGuiBuildMulti_waTmp[64], 0x40u, v3, jkPlayer_playerShortName);
    jkGuiBuildMulti_buttons[1].wstr = &jkGuiBuildMulti_waTmp[64];
    v4 = jkPlayer_GetMpcInfo(&jkGuiBuildMulti_waTmp[32], v28, v34, v33, v32);
    _v23 = v4;
    jkGuiBuildMulti_buttons[3].wstr = &jkGuiBuildMulti_waTmp[32];
    jkGuiBuildMulti_buttons[9].wstr = jkGuiBuildMulti_wLtLabel;
    jkGuiBuildMulti_buttons[10].wstr = jkGuiBuildMulti_wRtLabel;
    jkGuiBuildMulti_buttons[11].wstr = jkGuiBuildMulti_wLbLabel;
    jkGuiBuildMulti_buttons[12].wstr = jkGuiBuildMulti_wRbLabel;
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiBuildMulti_menu, &jkGuiBuildMulti_buttons[15]);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiBuildMulti_menu, &jkGuiBuildMulti_buttons[13]);
#ifdef TARGET_XBOX
    jkGuiBuildMulti_buttons[13].bIsVisible = 0;
    jkGuiBuildMulti_buttons[14].bIsVisible = 0;
    jkGuiBuildMulti_buttons[15].bIsVisible = 0;
#endif
    jkGuiRend_SetVisibleAndDraw(&jkGuiBuildMulti_buttons[4], &jkGuiBuildMulti_menu, 0);
    jkGuiRend_SetVisibleAndDraw(&jkGuiBuildMulti_buttons[5], &jkGuiBuildMulti_menu, 0);
    jkGuiBuildMulti_numSabers = 0;
    jkGuiBuildMulti_bEditShowing = 1;
    if ( stdConffile_OpenRead("misc\\sabers.dat") )
    {
        stdConffile_ReadLine();
        if ( _sscanf(stdConffile_aLine, "numsabers: %d", &jkGuiBuildMulti_numSabers) == 1
            && jkGuiBuildMulti_numSabers > 0 )
        {
            jkGame_aSabers = (jkSaberInfo *)pHS->alloc(sizeof(jkSaberInfo) * jkGuiBuildMulti_numSabers);
            jkGuiBuildMulti_apSaberBitmaps = (stdBitmap **)pHS->alloc(sizeof(stdBitmap*) * jkGuiBuildMulti_numSabers);
            if (!jkGame_aSabers || !jkGuiBuildMulti_apSaberBitmaps)
            {
                stdConffile_Close();
                return jkGuiBuildMulti_FailEditCharacterStartup();
            }
            memset(jkGame_aSabers, 0, sizeof(jkSaberInfo) * jkGuiBuildMulti_numSabers);
            memset(jkGuiBuildMulti_apSaberBitmaps, 0, sizeof(stdBitmap*) * jkGuiBuildMulti_numSabers);
            while ( stdConffile_ReadLine() )
            {
                if (_sscanf(stdConffile_aLine, "%d: %s %s %s", &idx, tmp3, tmp2, tmp1) != 4
                    || idx < 0
                    || idx >= jkGuiBuildMulti_numSabers)
                {
                    continue;
                }
                _strncpy(jkGame_aSabers[idx].BM, tmp3, 0x1Fu);
                v5 = jkGame_aSabers;
                jkGame_aSabers[idx].BM[31] = 0;
                _strncpy(v5[idx].sideMat, tmp2, 0x1Fu);
                v6 = jkGame_aSabers;
                jkGame_aSabers[idx].sideMat[31] = 0;
                _strncpy(v6[idx].tipMat, tmp1, 0x1Fu);
                jkGame_aSabers[idx].tipMat[31] = 0;
                stdString_snprintf(FileName, 128, "ui\\bm\\%s", tmp3);
                v7 = stdBitmap_Load(FileName, 1, 0);
                jkGuiBuildMulti_apSaberBitmaps[idx] = v7;
            }
        }
        stdConffile_Close();
    }
    else {
        return jkGuiBuildMulti_FailEditCharacterStartup(); // Added: MoTS demo has no MP assets
    }
    if (!jkGame_aSabers || !jkGuiBuildMulti_apSaberBitmaps || jkGuiBuildMulti_numSabers <= 0)
    {
        return jkGuiBuildMulti_FailEditCharacterStartup();
    }

    if ( v4 )
    {
        v8 = 0;
        idx = 0;
        if ( jkGuiBuildMulti_numSabers > 0 )
        {
            v9 = jkGame_aSabers;
            while ( strcmp(v33, v9->sideMat) || strcmp(v32, v9->tipMat) )
            {
                ++v8;
                ++v9;
                if ( v8 >= jkGuiBuildMulti_numSabers )
                {
                    v4 = _v23;
                    jkGuiBuildMulti_saberIdx = idx;
                    goto LABEL_16;
                }
            }
            idx = v8;
        }
        v4 = _v23;
        jkGuiBuildMulti_saberIdx = idx;
    }
    else
    {
        jkGuiBuildMulti_saberIdx = 0;
    }
LABEL_16:
    jkGuiBuildMulti_numModels = 0;
    jkGuiBuildMulti_bSabersLoaded = 1;
    if ( stdConffile_OpenRead("misc\\models.dat") )
    {
        stdConffile_ReadLine();
        if ( _sscanf(stdConffile_aLine, "nummodels: %d", &jkGuiBuildMulti_numModels) == 1
            && jkGuiBuildMulti_numModels > 0 )
        {
            jkGuiBuildMulti_aModels = (jkMultiModelInfo *)pHS->alloc(jkGuiBuildMulti_numModels * sizeof(jkMultiModelInfo));
            if (!jkGuiBuildMulti_aModels)
            {
                stdConffile_Close();
                return jkGuiBuildMulti_FailEditCharacterStartup();
            }
            memset(jkGuiBuildMulti_aModels, 0, jkGuiBuildMulti_numModels * sizeof(jkMultiModelInfo));
            while ( stdConffile_ReadLine() )
            {
                if ( _sscanf(stdConffile_aLine, "%d: %s %s", &idx, tmp1, tmp2) == 3
                    && idx >= 0
                    && idx < jkGuiBuildMulti_numModels )
                {
                    _strncpy(jkGuiBuildMulti_aModels[idx].modelFpath, tmp1, 0x1Fu);
                    v10 = jkGuiBuildMulti_aModels;
                    jkGuiBuildMulti_aModels[idx].modelFpath[31] = 0;
                    _strncpy(v10[idx].sndFpath, tmp2, 0x1Fu);
                    jkGuiBuildMulti_aModels[idx].sndFpath[31] = 0;
                }
            }
        }
        stdConffile_Close();
    }
    if (!jkGuiBuildMulti_aModels || jkGuiBuildMulti_numModels <= 0)
    {
        return jkGuiBuildMulti_FailEditCharacterStartup();
    }
    if ( v4 )
    {
        v11 = 0;
        v12 = 0;
        _v23 = 0;
        if ( jkGuiBuildMulti_numModels > 0 )
        {
            v13 = jkGuiBuildMulti_aModels;
            while ( strcmp(v28, v13->modelFpath) )
            {
                ++v12;
                ++v13;
                if ( v12 >= jkGuiBuildMulti_numModels )
                {
                    jkGuiBuildMulti_modelIdx = _v23;
                    goto LABEL_32;
                }
            }
            v11 = v12;
        }
        jkGuiBuildMulti_modelIdx = v11;
    }
    else
    {
        jkGuiBuildMulti_modelIdx = 0;
    }
LABEL_32:
    jkGuiBuildMulti_lastModelDrawMs = 0;
    if (jkGuiBuildMulti_numModels > 0 && jkGuiBuildMulti_DisplayModel())
    {
        jkGuiBuildMulti_ThingInit(jkGuiBuildMulti_aModels[jkGuiBuildMulti_modelIdx].modelFpath);
        previewStarted = 1;
    }

    stdFnames_CopyShortName(v24, 16, jkGuiBuildMulti_aModels[jkGuiBuildMulti_modelIdx].modelFpath);
    jkGuiTitle_sub_4189A0(v24);
    v15 = jkStrings_GetUniStringWithFallback(v24);
    jk_snwprintf(jkGuiBuildMulti_waTmp2, 0x20, L"%s", v15); // ADDED: swprintf -> snwprintf
    jkGuiBuildMulti_buttons[8].wstr = jkGuiBuildMulti_waTmp2;
    do
    {
        v16 = 0;
#ifdef TARGET_XBOX
        jkGuiRend_XboxFooterBegin(&jkGuiBuildMulti_menu);
        jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menu, JKGUI_XBOX_BTN_A, 0, L"Select");
        jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menu, JKGUI_XBOX_BTN_B, -1, L"Back");
        jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menu, JKGUI_XBOX_BTN_Y, 109, L"Force");
        jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menu, JKGUI_XBOX_BTN_START, 106, L"Save");
#endif
        v17 = jkGuiRend_DisplayAndReturnClicked(&jkGuiBuildMulti_menu);
        v18 = v17;
        switch ( v17 )
        {
            case -1:
                if ( bIdk )
                {
                    jkGuiBuildMulti_Load(FileName, 128, jkPlayer_playerShortName, &jkGuiBuildMulti_waTmp[32], 1);
                    stdFileUtil_DelFile(FileName);
                }
                break;
            case 106:
#ifdef TARGET_XBOX
                if (bIdk)
                {
                    jkGuiBuildMulti_MakeGeneratedCharacterName(&jkGuiBuildMulti_waTmp[32], 32);
                    jkGuiBuildMulti_buttons[3].wstr = &jkGuiBuildMulti_waTmp[32];
                }
#endif
                jkPlayer_SetMpcInfo(
                    &jkGuiBuildMulti_waTmp[32],
                    jkGuiBuildMulti_aModels[jkGuiBuildMulti_modelIdx].modelFpath,
                    jkGuiBuildMulti_aModels[jkGuiBuildMulti_modelIdx].sndFpath,
                    jkGame_aSabers[jkGuiBuildMulti_saberIdx].sideMat,
                    jkGame_aSabers[jkGuiBuildMulti_saberIdx].tipMat);
#ifdef TARGET_XBOX
                XDBG("ProfilePortrait: save capture visible preview\n");
                jkGuiBuildMulti_XboxCaptureVisiblePortraitBooth();
#endif
                break;
            case 109:
                jkPlayer_FixStars();
                jkGuiBuildMulti_bRendering = 0; // Added
                if (!Main_bMotsCompat || jkPlayer_personality == 1) {
                    jkGuiForce_Show(1, 1, 0, &jkGuiBuildMulti_waTmp[32], 0, 0);
                }
                else {
                    jkGuiForce_Show(0, 1, 0, &jkGuiBuildMulti_waTmp[32], 0, 0);
                }
                
                jkGuiBuildMulti_bRendering = 1; // Added
                v16 = 1;
                break;
        }
    }
    while ( v16 );
    if (previewStarted)
        jkGuiBuildMulti_ThingCleanup();
    jkGuiBuildMulti_CloseRender();
    jkGuiBuildMulti_bEditShowing = 0;
    jkGuiBuildMulti_FreeLoadedCharacterLists();
    jkGui_SetModeGame();

    // Added
    //std3D_PurgeTextureCache();

    jkGuiBuildMulti_bRendering = 0; // Added

    // Added
    stdBitmap_UnloadData(jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_MULTI]);

    return v18;
}

int jkGuiBuildMulti_DisplayModel()
{
    stdVBufferTexFmt v1; // [esp+8h] [ebp-4Ch] BYREF

    int32_t tmp = jkGuiBuildMulti_bRendering; // Added
    jkGuiBuildMulti_bRendering = 1; // Added

    jkGuiBuildMulti_savedAcceleration = rdroid_curAcceleration;
    rdOpen(1);
    rdroid_curAcceleration = 1;
    jkGuiBuildMulti_renderOpen = 1;
    rdColormap_LoadEntry("misc\\cmp\\UIColormap.cmp", &jkGuiBuildMulti_colormap);
    rdColormap_SetCurrent(&jkGuiBuildMulti_colormap);
    rdSetRenderOptions(jkGuiBuildMulti_renderOptions);
    rdSetGeometryMode(RD_GEOMODE_TEXTURED);
    rdSetLightingMode(RD_LIGHTMODE_GOURAUD);
    rdSetTextureMode(RD_TEXTUREMODE_PERSPECTIVE);
    rdSetZBufferMethod(RD_ZBUFFER_READ_WRITE);
    rdSetSortingMethod(0);
    rdSetOcclusionMethod(0);
    memset(&v1, 0, sizeof(v1));
    v1.format.bpp = 8;
    v1.width = 260;
    v1.height = 260;
    v1.width_in_pixels = 260;
    v1.width_in_bytes = 260;
    v1.texture_size_in_bytes = 260 * 260;
    v1.format.is16bit = 0;
    jkGuiBuildMulti_pVBuf1 = stdDisplay_VBufferNew(&v1, 0, 0, 0);
    if (jkGuiBuildMulti_pVBuf1)
        stdDisplay_VBufferFill(jkGuiBuildMulti_pVBuf1, 0, 0);
    _memcpy(&jkGuiBuildMulti_texFmt, &stdDisplay_pCurVideoMode->format, sizeof(jkGuiBuildMulti_texFmt));
    jkGuiBuildMulti_texFmt.format.bpp = 16;
    jkGuiBuildMulti_texFmt.format.is16bit = 1;
    if (jkGuiBuildMulti_texFmt.width_in_pixels <= 0)
        jkGuiBuildMulti_texFmt.width_in_pixels = jkGuiBuildMulti_texFmt.width;
    if (jkGuiBuildMulti_texFmt.width_in_bytes <= 0 || jkGuiBuildMulti_texFmt.width_in_bytes < (uint32_t)(jkGuiBuildMulti_texFmt.width * 2))
        jkGuiBuildMulti_texFmt.width_in_bytes = jkGuiBuildMulti_texFmt.width * 2;
    jkGuiBuildMulti_texFmt.texture_size_in_bytes = jkGuiBuildMulti_texFmt.width_in_bytes * jkGuiBuildMulti_texFmt.height;
    jkGuiBuildMulti_pVBuf2 = stdDisplay_VBufferNew(&jkGuiBuildMulti_texFmt, 0, 0, 0);
    jkGuiBuildMulti_pCanvas = rdCanvas_New(3, jkGuiBuildMulti_pVBuf1, jkGuiBuildMulti_pVBuf2, 0, 0, 259, 259, 6);
#ifdef TARGET_XBOX
    jkGuiBuildMulti_pCamera = rdCamera_New(60.0, 0.0, 0.08, 256.0, 1.0);
#else
    jkGuiBuildMulti_pCamera = rdCamera_New(60.0, 0.0, 0.08, 15.0, 1.0);
#endif
    rdCamera_SetCanvas(jkGuiBuildMulti_pCamera, jkGuiBuildMulti_pCanvas);
    jkGuiBuildMulti_pThingCamera = rdThing_New(0);
    rdThing_SetCamera(jkGuiBuildMulti_pThingCamera, jkGuiBuildMulti_pCamera);
    rdCamera_SetCurrent(jkGuiBuildMulti_pCamera);
    jkGuiBuildMulti_projectRot.x = 0.0;
    jkGuiBuildMulti_projectRot.y = 0.2;
    jkGuiBuildMulti_projectRot.z = -0.04;
    jkGuiBuildMulti_projectPos.x = 0.0;
    jkGuiBuildMulti_projectPos.y = 180.0;
    jkGuiBuildMulti_projectPos.z = 0.0;
    rdMatrix_Build34(&jkGuiBuildMulti_orthoProjection, &jkGuiBuildMulti_projectPos, &jkGuiBuildMulti_projectRot);
    rdCamera_Update(&jkGuiBuildMulti_orthoProjection);
    _memcpy(&jkGuiBuildMulti_matrix, &rdroid_identMatrix34, sizeof(jkGuiBuildMulti_matrix));
    rdCamera_ClearLights(jkGuiBuildMulti_pCamera);
    rdLight_NewEntry(&jkGuiBuildMulti_light);
    jkGuiBuildMulti_lightPos.x = 0.2;
    jkGuiBuildMulti_lightPos.y = 0.2;
    jkGuiBuildMulti_lightPos.z = 0.0;
    jkGuiBuildMulti_light.intensity = 4.0;
    rdCamera_AddLight(jkGuiBuildMulti_pCamera, &jkGuiBuildMulti_light, &jkGuiBuildMulti_lightPos);
    rdCamera_SetAmbientLight(jkGuiBuildMulti_pCamera, 0.4);
    jkGuiBuildMulti_fnMatLoader = rdMaterial_RegisterLoader(jkGuiBuildMulti_MatLoader);
    jkGuiBuildMulti_fnModelLoader = rdModel3_RegisterLoader(jkGuiBuildMulti_ModelLoader);
    jkGuiBuildMulti_fnKeyframeLoader = rdKeyframe_RegisterLoader(jkGuiBuildMulti_KeyframeLoader);
    jkGuiBuildMulti_pModelGun = rdModel3_New("bryg.3do");
    jkGuiBuildMulti_pThingGun = rdThing_New(0);
    int32_t ret = rdThing_SetModel3(jkGuiBuildMulti_pThingGun, jkGuiBuildMulti_pModelGun);

#ifdef TARGET_XBOX
    if (!jkGuiBuildMulti_pVBuf1 || !jkGuiBuildMulti_pVBuf2 || !jkGuiBuildMulti_pCanvas || !jkGuiBuildMulti_pCamera)
    {
        XDBGF("BuildMulti3D: DisplayModel resource miss vbuf1=%p vbuf2=%p canvas=%p camera=%p fmt1=(%dx%d pitch=%u size=%u) fmt2=(%dx%d pitch=%u size=%u)\n",
              jkGuiBuildMulti_pVBuf1,
              jkGuiBuildMulti_pVBuf2,
              jkGuiBuildMulti_pCanvas,
              jkGuiBuildMulti_pCamera,
              v1.width,
              v1.height,
              v1.width_in_bytes,
              v1.texture_size_in_bytes,
              jkGuiBuildMulti_texFmt.width,
              jkGuiBuildMulti_texFmt.height,
              jkGuiBuildMulti_texFmt.width_in_bytes,
              jkGuiBuildMulti_texFmt.texture_size_in_bytes);
    }
    stdDisplay_XboxSetPostMenuDrawCallback(jkGuiBuildMulti_XboxPostMenuDraw, NULL);
    XDBGF("BuildMulti3D: DisplayModel ready ret=%d accel=%d modelIdx=%d\n",
          ret,
          rdroid_curAcceleration,
          jkGuiBuildMulti_modelIdx);
#endif
    jkGuiBuildMulti_bRendering = tmp; // Added
    return ret;
}

void jkGuiBuildMulti_ModelDrawer(jkGuiElement *pElement, jkGuiMenu *pMenu, stdVBuffer *pVbuf, BOOL redraw)
{
    uint32_t v5; // st7
    flex_d_t v6; // st7
    rdPuppet *v7; // [esp-8h] [ebp-24h]
    int64_t v8; // [esp+8h] [ebp-14h]
    flex_t v9; // [esp+8h] [ebp-14h]
    rdVector3 rot; // [esp+10h] [ebp-Ch] BYREF
    flex_t a2a; // [esp+24h] [ebp+8h]
    rdRect bgRect;

    jkGuiBuildMulti_bRendering = 1;

#ifdef TARGET_XBOX
    (void)pVbuf;
    (void)redraw;
    jkGuiBuildMulti_xboxModelDrawerCalls++;
    return;
#endif

    if (!jkGuiBuildMulti_thing || !jkGuiBuildMulti_pVBuf1)
        return;

    if ( jkGuiBuildMulti_lastModelDrawMs )
    {
        if ( stdPlatform_GetTimeMsec() - (uint32_t)jkGuiBuildMulti_lastModelDrawMs <= BUILDMULTI_SWITCH_DELAY_MS ) {
#ifdef STDBITMAP_PARTIAL_LOAD
            if (pMenu->pBgBitmap) {
                stdBitmap_EnsureData(pMenu->pBgBitmap);
            }

            if (pMenu->pBgBitmap && pMenu->pBgBitmap->mipSurfaces[0]) {
                bgRect = pElement->rect;
                stdDisplay_VBufferCopy(pVbuf, pMenu->pBgBitmap->mipSurfaces[0], pElement->rect.x, pElement->rect.y, &bgRect, 0);
            }
            else if (pMenu->pTextureOverride) {
                bgRect = pElement->rect;
                stdDisplay_VBufferCopy(pVbuf, pMenu->pTextureOverride, pElement->rect.x, pElement->rect.y, &bgRect, 0);
            }
#else
            bgRect = pElement->rect;
            stdDisplay_VBufferCopy(pVbuf, pMenu->texture, pElement->rect.x, pElement->rect.y, &bgRect, 0);
#endif
            return;
        }
        jkGuiBuildMulti_ThingCleanup(); // inlined

        if (jkGuiBuildMulti_aModels && jkGuiBuildMulti_modelIdx >= 0 && jkGuiBuildMulti_modelIdx < jkGuiBuildMulti_numModels)
            jkGuiBuildMulti_ThingInit(jkGuiBuildMulti_aModels[jkGuiBuildMulti_modelIdx].modelFpath); // inlined
        else
            return;
        jkGuiBuildMulti_lastModelDrawMs = 0;
    }

    if ( g_app_suspended )
    {
        stdControl_ShowCursor(1);
#ifndef TARGET_XBOX
        stdDisplay_VBufferFill(jkGuiBuildMulti_pVBuf1, 0, 0);
        stdDisplay_VBufferLock(jkGuiBuildMulti_pVBuf1);
#endif
        rdAdvanceFrame();
#ifdef TARGET_XBOX
        std3D_XboxSetViewport(
            pElement->rect.x,
            480 - pElement->rect.y - pElement->rect.height,
            pElement->rect.width,
            pElement->rect.height);
#endif

        // Added: switched around the order of casting for this...
        v5 = stdPlatform_GetTimeMsec();
        v6 = (v5 - jkGuiBuildMulti_startTimeSecs) * 0.001;
        if ( v6 < 0.0 )
        {
            a2a = 0.0;
        }
        else if ( v6 > 1.0 )
        {
            a2a = 1.0;
        }
        else
        {
            a2a = v6;
        }
        rdPuppet_UpdateTracks(jkGuiBuildMulti_thing->puppet, a2a);
        jkGuiBuildMulti_startTimeSecs = v5;
        rdThing_Draw(jkGuiBuildMulti_thing, &jkGuiBuildMulti_matrix);
        if (jkGuiBuildMulti_pThingGun && jkGuiBuildMulti_thing->hierarchyNodeMatrices)
            rdThing_Draw(jkGuiBuildMulti_pThingGun, jkGuiBuildMulti_thing->hierarchyNodeMatrices + 12);
        rdFinishFrame();
#ifdef TARGET_XBOX
        std3D_XboxResetViewport();
#else
        stdDisplay_VBufferUnlock(jkGuiBuildMulti_pVBuf1);
#endif
        rot.x = 0.0;
        rot.z = 0.0;
        rot.y = a2a * 20.0;
        rdMatrix_PostRotate34(&jkGuiBuildMulti_matrix, &rot);
#ifndef TARGET_XBOX
        stdDisplay_VBufferCopy(pVbuf, jkGuiBuildMulti_pVBuf1, pElement->rect.x, pElement->rect.y, 0, 0);
#endif
        stdControl_ShowCursor(0);
    }
}

void jkGuiBuildMulti_SaberDrawer(jkGuiElement *pElement, jkGuiMenu *pMenu, stdVBuffer *pVbuf, BOOL redraw)
{
    stdBitmap *pSabBm; // eax
    int32_t bmWidth; // esi
    int32_t bmHeight; // esi
    rdRect rect; // [esp+4h] [ebp-10h] BYREF

    pSabBm = jkGuiBuildMulti_apSaberBitmaps[jkGuiBuildMulti_saberIdx];
#ifdef STDBITMAP_PARTIAL_LOAD
    if (pSabBm)
        stdBitmap_EnsureData(pSabBm);
#endif
    if (!pSabBm || !pSabBm->mipSurfaces || !*pSabBm->mipSurfaces)
        return;
    rect.x = 0;
    rect.y = 0;
    bmWidth = (*pSabBm->mipSurfaces)->format.width;
    rect.width = pElement->rect.width;
    if ( rect.width >= bmWidth )
        rect.width = bmWidth;
    bmHeight = (*pSabBm->mipSurfaces)->format.height;
    rect.height = pElement->rect.height;
    if ( rect.height >= bmHeight )
        rect.height = bmHeight;
    stdDisplay_VBufferCopy(pVbuf, *pSabBm->mipSurfaces, pElement->rect.x, pElement->rect.y, &rect, 0);
}

// MOTS altered
int jkGuiBuildMulti_SaberButtonClicked(jkGuiElement *pElement, jkGuiMenu *pMenu, int32_t mouseX, int32_t mouseY, BOOL redraw)
{
    int v2; // eax
    wchar_t *v3; // eax
    int v4; // eax
    wchar_t *v5; // eax
    char v7[16]; // [esp+0h] [ebp-10h] BYREF

    switch ( pElement->hoverId )
    {
        case 100:
            v2 = --jkGuiBuildMulti_modelIdx;
            if ( jkGuiBuildMulti_modelIdx < 0 )
            {
                v2 = jkGuiBuildMulti_numModels - 1;
                jkGuiBuildMulti_modelIdx = jkGuiBuildMulti_numModels - 1;
                if ( jkGuiBuildMulti_numModels - 1 < 0 )
                {
                    v2 = 0;
                    jkGuiBuildMulti_modelIdx = 0;
                }
            }
            stdFnames_CopyShortName(v7, 16, jkGuiBuildMulti_aModels[v2].modelFpath);
            jkGuiTitle_sub_4189A0(v7);
            v3 = jkStrings_GetUniStringWithFallback(v7);
            jk_snwprintf(jkGuiBuildMulti_waTmp2, 0x20, L"%s", v3); // ADDED: swprintf -> snwprintf
            jkGuiBuildMulti_buttons[8].wstr = jkGuiBuildMulti_waTmp2;
            jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_buttons[8], pMenu, 1);
            goto LABEL_9;
        case 101:
            v4 = ++jkGuiBuildMulti_modelIdx;
            if ( jkGuiBuildMulti_modelIdx >= jkGuiBuildMulti_numModels )
            {
                v4 = 0;
                jkGuiBuildMulti_modelIdx = 0;
            }
            stdFnames_CopyShortName(v7, 16, jkGuiBuildMulti_aModels[v4].modelFpath);
            jkGuiTitle_sub_4189A0(v7);
            v5 = jkStrings_GetUniStringWithFallback(v7);
            jk_snwprintf(jkGuiBuildMulti_waTmp2, 0x20, L"%s", v5); // ADDED: swprintf -> snwprintf
            jkGuiBuildMulti_buttons[8].wstr = jkGuiBuildMulti_waTmp2;
            jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_buttons[8], pMenu, 1);
LABEL_9:
            jkGuiBuildMulti_lastModelDrawMs = stdPlatform_GetTimeMsec();
            return 0;
        case 102:
            if ( --jkGuiBuildMulti_saberIdx < 0 )
                jkGuiBuildMulti_saberIdx = jkGuiBuildMulti_numSabers - 1;
            if ( jkGuiBuildMulti_numSabers < 0 )
                jkGuiBuildMulti_saberIdx = 0;
            jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_buttons[7], pMenu, 1);
            return 0;
        case 103:
            if ( ++jkGuiBuildMulti_saberIdx >= jkGuiBuildMulti_numSabers )
                jkGuiBuildMulti_saberIdx = 0;
            jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_buttons[7], pMenu, 1);
            return 0;
        default:
            return 0;
    }
}

#ifdef TARGET_XBOX
static int jkGuiBuildMulti_XboxReadEdge(int key, int *prevDown)
{
    int val = 0;
    int down = (stdControl_ReadKey(key, &val) && val) ? 1 : 0;
    int pressed = down && !*prevDown;
    *prevDown = down;
    return pressed;
}

static void jkGuiBuildMulti_XboxFocusBottom(jkGuiMenu *pMenu, int dir)
{
    static const int bottomButtons[] = {13, 14, 15};
    int cur = 1;
    int i;

    for (i = 0; i < 3; i++)
    {
        if (pMenu->lastMouseOverClickable == &jkGuiBuildMulti_buttons[bottomButtons[i]])
        {
            cur = i;
            break;
        }
    }

    if (dir == FOCUS_LEFT && cur > 0)
        cur--;
    else if (dir == FOCUS_RIGHT && cur < 2)
        cur++;

    pMenu->focusedElement = NULL;
    jkGuiRend_ClickableMouseover(pMenu, &jkGuiBuildMulti_buttons[bottomButtons[cur]]);
}

int jkGuiBuildMulti_HandleXboxController(jkGuiMenu *pMenu, int focusDir)
{
    static int prevLb = 0;
    static int prevRb = 0;
    static int prevLt = 0;
    static int prevRt = 0;
    jkGuiElement selector;

    if (pMenu == jkGuiBuildMulti_pNewCharacterMenu)
    {
        jkGuiElement *hovered = pMenu->lastMouseOverClickable;
        int rankFocused = hovered == &jkGuiBuildMulti_pNewCharacterElements[7]
            || hovered == &jkGuiBuildMulti_pNewCharacterElements[8]
            || hovered == &jkGuiBuildMulti_pNewCharacterElements[9]
            || hovered == &jkGuiBuildMulti_pNewCharacterElements[10];

        if (jkGuiBuildMulti_pNewCharacterElements[9].bIsVisible && jkGuiBuildMulti_pNewCharacterElements[10].bIsVisible)
        {
            if ((rankFocused && focusDir == FOCUS_LEFT) || jkGuiBuildMulti_XboxReadEdge(KEY_JOY1_B10, &prevLb))
            {
                jkGuiBuildMulti_menuNewCharacter_rankArrowButtonClickHandler(&jkGuiBuildMulti_pNewCharacterElements[9], pMenu, 0, 0, 1);
                pMenu->focusedElement = NULL;
                jkGuiRend_ClickableMouseover(pMenu, &jkGuiBuildMulti_pNewCharacterElements[9]);
                return 1;
            }

            if ((rankFocused && focusDir == FOCUS_RIGHT) || jkGuiBuildMulti_XboxReadEdge(KEY_JOY1_B11, &prevRb))
            {
                jkGuiBuildMulti_menuNewCharacter_rankArrowButtonClickHandler(&jkGuiBuildMulti_pNewCharacterElements[10], pMenu, 0, 0, 1);
                pMenu->focusedElement = NULL;
                jkGuiRend_ClickableMouseover(pMenu, &jkGuiBuildMulti_pNewCharacterElements[10]);
                return 1;
            }
        }

        if (pMenu != &jkGuiBuildMulti_menu)
            prevLt = prevRt = 0;
        return 0;
    }

    if (pMenu != &jkGuiBuildMulti_menu)
    {
        prevLb = prevRb = prevLt = prevRt = 0;
        return 0;
    }

    if (focusDir != FOCUS_NONE)
        jkGuiBuildMulti_XboxFocusBottom(pMenu, focusDir);

    selector = jkGuiBuildMulti_buttons[9];
    if (jkGuiBuildMulti_XboxReadEdge(KEY_JOY1_B16, &prevLt))
    {
        selector.hoverId = 100;
        jkGuiBuildMulti_SaberButtonClicked(&selector, pMenu, 0, 0, 1);
    }

    selector = jkGuiBuildMulti_buttons[10];
    if (jkGuiBuildMulti_XboxReadEdge(KEY_JOY1_B17, &prevRt))
    {
        selector.hoverId = 101;
        jkGuiBuildMulti_SaberButtonClicked(&selector, pMenu, 0, 0, 1);
    }

    selector = jkGuiBuildMulti_buttons[11];
    if (jkGuiBuildMulti_XboxReadEdge(KEY_JOY1_B10, &prevLb))
    {
        selector.hoverId = 102;
        jkGuiBuildMulti_SaberButtonClicked(&selector, pMenu, 0, 0, 1);
    }

    selector = jkGuiBuildMulti_buttons[12];
    if (jkGuiBuildMulti_XboxReadEdge(KEY_JOY1_B11, &prevRb))
    {
        selector.hoverId = 103;
        jkGuiBuildMulti_SaberButtonClicked(&selector, pMenu, 0, 0, 1);
    }

    return 1;
}
#else
int jkGuiBuildMulti_HandleXboxController(jkGuiMenu *pMenu, int focusDir)
{
    (void)pMenu;
    (void)focusDir;
    return 0;
}
#endif

void jkGuiBuildMulti_sub_41A120(jkGuiMenu *pMenu)
{
    if ( g_app_suspended )
        jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_buttons[6], pMenu, 1);
}

int jkGuiBuildMulti_Startup()
{
    if (!Main_bMotsCompat)
    {
        jkGuiBuildMulti_pNewCharacterMenu = &jkGuiBuildMulti_menuNewCharacter;
        jkGuiBuildMulti_pNewCharacterElements = jkGuiBuildMulti_menuNewCharacter_buttons;
    }
    else {
        jkGuiBuildMulti_pNewCharacterMenu = &jkGuiBuildMulti_menuNewCharacterMots;
        jkGuiBuildMulti_pNewCharacterElements = jkGuiBuildMulti_menuNewCharacter_buttonsMots;
    }
    jkGui_InitMenu(jkGuiBuildMulti_pNewCharacterMenu, jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD]);
    jkGui_InitMenu(&jkGuiBuildMulti_menuEditCharacter, jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD]);
    jkGui_InitMenu(&jkGuiBuildMulti_menuLoadCharacter, jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD]);

    jkGuiBuildMulti_bInitted = 1;
    return 1;
}

void jkGuiBuildMulti_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added
    jkGuiBuildMulti_bInitted = 0;

    // Added: clean reset
    memset(jkGuiBuildMulti_wPlayerShortName, 0, sizeof(jkGuiBuildMulti_wPlayerShortName));
    memset(jkGuiBuildMulti_aMpcInfo, 0, sizeof(jkGuiBuildMulti_aMpcInfo));
    memset(jkGuiBuildMulti_wTmp, 0, sizeof(jkGuiBuildMulti_wTmp));
    memset(jkGuiBuildMulti_wTmp2, 0, sizeof(jkGuiBuildMulti_wTmp2));
    memset(jkGuiBuildMulti_wTmp3, 0, sizeof(jkGuiBuildMulti_wTmp3));
    memset(jkGuiBuildMulti_aWchar_5594C8, 0, sizeof(jkGuiBuildMulti_aWchar_5594C8));

    jkGuiBuildMulti_fnMatLoader = NULL;
    jkGuiBuildMulti_fnModelLoader = NULL;
    jkGuiBuildMulti_fnKeyframeLoader = NULL;
    jkGuiBuildMulti_pCanvas = NULL;
    jkGuiBuildMulti_pCamera = NULL;
    jkGuiBuildMulti_model = NULL;
    jkGuiBuildMulti_pModelGun = NULL;
    jkGuiBuildMulti_keyframe = NULL;
    jkGuiBuildMulti_pThingCamera = NULL;
    jkGuiBuildMulti_thing = NULL;
    jkGuiBuildMulti_pThingGun = NULL;
    jkGuiBuildMulti_startTimeSecs = 0;

    memset(&jkGuiBuildMulti_colormap, 0, sizeof(jkGuiBuildMulti_colormap));
    memset(&jkGuiBuildMulti_light, 0, sizeof(jkGuiBuildMulti_light));
    memset(&jkGuiBuildMulti_matrix, 0, sizeof(jkGuiBuildMulti_matrix));

    jkGuiBuildMulti_pVBuf1 = NULL;
    jkGuiBuildMulti_pVBuf2 = NULL;
    jkGuiBuildMulti_trackNum = 0;
    memset(jkGuiBuildMulti_waTmp, 0, sizeof(jkGuiBuildMulti_waTmp));
    memset(jkGuiBuildMulti_waTmp2, 0, sizeof(jkGuiBuildMulti_waTmp2));

    jkGuiBuildMulti_apSaberBitmaps = NULL;
    jkGame_aSabers = NULL;
    jkGuiBuildMulti_bSabersLoaded = 0;
    jkGuiBuildMulti_bEditShowing = 0;
    jkGuiBuildMulti_numModels = 0;
    jkGuiBuildMulti_numSabers = 0;
    jkGuiBuildMulti_saberIdx = 0;
    jkGuiBuildMulti_modelIdx = 0;
    jkGuiBuildMulti_aModels = NULL;
    jkGuiBuildMulti_renderOptions = 0x103;

    memset(&jkGuiBuildMulti_projectRot, 0, sizeof(jkGuiBuildMulti_projectRot));
    memset(&jkGuiBuildMulti_projectPos, 0, sizeof(jkGuiBuildMulti_projectPos));
    memset(&jkGuiBuildMulti_texFmt, 0, sizeof(jkGuiBuildMulti_texFmt));
    memset(&jkGuiBuildMulti_orthoProjection, 0, sizeof(jkGuiBuildMulti_orthoProjection));
    memset(&jkGuiBuildMulti_lightPos, 0, sizeof(jkGuiBuildMulti_lightPos));
    jkGuiBuildMulti_lastModelDrawMs = 0;
#ifdef TARGET_XBOX
    jkGuiBuildMulti_XboxUnloadButtonGlyphs();
#endif
}

void jkGuiBuildMulti_Load(char *pPathOut, int pathOutLen, wchar_t *pPlayerName, wchar_t *pCharName, int bCharPath)
{
    char tmp1[128]; // [esp+8h] [ebp-100h] BYREF

    stdString_WcharToChar(tmp1, pPlayerName, 127);
    tmp1[127] = 0;
    stdFnames_MakePath(pPathOut, pathOutLen, "player", tmp1);
    if ( bCharPath )
    {
        jkPlayer_MPCMakePath(pPathOut, pathOutLen, pPlayerName, pCharName);
    }
    else
    {
        stdString_snprintf(pPathOut, pathOutLen, "player\\%s", tmp1);
    }
}

int jkGuiBuildMulti_Show()
{
    wchar_t *pwMultiplayerCharsStr; // eax
    int v1; // ebp
    int v2; // edi
    int v3; // esi
    jkGuiStringEntry *pEntry; // eax
    wchar_t *v6; // esi
    wchar_t *v7; // eax
    wchar_t *v8; // eax
    int v9; // [esp+10h] [ebp-3DCh]
    Darray darr; // [esp+14h] [ebp-3D8h] BYREF
    wchar_t wPlayerName[32]; // [esp+2Ch] [ebp-3C0h] BYREF
    char aMpcFPath[128]; // [esp+ECh] [ebp-300h] BYREF
    wchar_t wtmp1[256]; // [esp+1ECh] [ebp-200h] BYREF

#ifdef JKGUI_SMOL_SCREEN
    jkGuiBuildMulti_menuEditCharacter_buttons[6].rect = jkGuiBuildMulti_menuEditCharacter_buttons[6].rectOrig;
    jkGuiBuildMulti_menuEditCharacter_buttons[7].rect = jkGuiBuildMulti_menuEditCharacter_buttons[7].rectOrig;
    jkGuiBuildMulti_menuEditCharacter_buttons[6].bIsSmolDirty = 1;
    jkGuiBuildMulti_menuEditCharacter_buttons[7].bIsSmolDirty = 1;
#endif

    // MoTS added: Need to move things around for Personality
    if (!Main_bMotsCompat) {
        jkGuiBuildMulti_menuEditCharacter_buttons[10].bIsVisible = 0;
        jkGuiBuildMulti_menuEditCharacter_buttons[11].bIsVisible = 0;

        jkGuiBuildMulti_menuEditCharacter_buttons[6].rect.y = 190;
        jkGuiBuildMulti_menuEditCharacter_buttons[7].rect.y = 210;
    }
    else {
        jkGuiBuildMulti_menuEditCharacter_buttons[10].bIsVisible = 1;
        jkGuiBuildMulti_menuEditCharacter_buttons[11].bIsVisible = 1;

        jkGuiBuildMulti_menuEditCharacter_buttons[6].rect.y = 310;
        jkGuiBuildMulti_menuEditCharacter_buttons[7].rect.y = 330;
    }

#ifdef JKGUI_SMOL_SCREEN
    jkGui_SmolScreenFixup(&jkGuiBuildMulti_menuEditCharacter, 0);
#endif

    wPlayerName[0] = 0;
    memset(&wPlayerName[1], 0, 0x3Cu);
    wPlayerName[31] = 0;
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD]->palette);
    jkGuiRend_DarrayNewStr(&darr, 5, 1);
    jkGuiBuildMulti_menuEditCharacter_buttons[3].clickHandlerFunc = jkGuiBuildMulti_sub_41D830;
    jkGuiBuildMulti_menuEditCharacter_buttons[0].wstr = NULL;
    pwMultiplayerCharsStr = jkStrings_GetUniStringWithFallback("GUI_S_MULTIPLAYER_CHARACTERS");
    jk_snwprintf(jkGuiBuildMulti_wPlayerShortName, 0x40u, pwMultiplayerCharsStr, jkPlayer_playerShortName);
    jkGuiBuildMulti_menuEditCharacter_buttons[2].wstr = jkGuiBuildMulti_wPlayerShortName;
    v1 = 0;
    do
    {
        v2 = jkGuiBuildMulti_Show2(&darr, &jkGuiBuildMulti_menuEditCharacter_buttons[3], 0, 9, v1);
        jkGuiBuildMulti_sub_41D680(&jkGuiBuildMulti_menuEditCharacter, jkGuiBuildMulti_menuEditCharacter_buttons[3].selectedTextEntry);
        v3 = 1;
#ifdef TARGET_XBOX
        jkGuiBuildMulti_menuEditCharacter_buttons[12].bIsVisible = 0;
        jkGuiBuildMulti_menuEditCharacter_buttons[13].bIsVisible = 0;
        jkGuiBuildMulti_menuEditCharacter_buttons[14].bIsVisible = 0;
        jkGuiBuildMulti_menuEditCharacter_buttons[15].bIsVisible = 0;
        if (!v2)
        {
            jkGuiBuildMulti_menuEditCharacter_buttons[0].wstr = jkGuiBuildMulti_wNoCharacters;
            jkGuiBuildMulti_menuEditCharacter.lastMouseOverClickable = &jkGuiBuildMulti_menuEditCharacter_buttons[13];
        }
#endif
        if ( v2 )
        {
            jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiBuildMulti_menuEditCharacter, &jkGuiBuildMulti_menuEditCharacter_buttons[15]);
            jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiBuildMulti_menuEditCharacter, &jkGuiBuildMulti_menuEditCharacter_buttons[12]);
#ifdef TARGET_XBOX
            jkGuiRend_XboxFooterBegin(&jkGuiBuildMulti_menuEditCharacter);
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuEditCharacter, JKGUI_XBOX_BTN_A, 1, L"Edit");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuEditCharacter, JKGUI_XBOX_BTN_B, -1, L"Back");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuEditCharacter, JKGUI_XBOX_BTN_X, 102, L"Remove");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuEditCharacter, JKGUI_XBOX_BTN_Y, 100, L"New");
            jkGuiRend_XboxSetInitialFocus(&jkGuiBuildMulti_menuEditCharacter, &jkGuiBuildMulti_menuEditCharacter_buttons[3]);
#endif
            v9 = jkGuiRend_DisplayAndReturnClicked(&jkGuiBuildMulti_menuEditCharacter);
        }
        else
        {
#ifdef TARGET_XBOX
            jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiBuildMulti_menuEditCharacter, &jkGuiBuildMulti_menuEditCharacter_buttons[13]);
            jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiBuildMulti_menuEditCharacter, &jkGuiBuildMulti_menuEditCharacter_buttons[12]);
            jkGuiRend_XboxFooterBegin(&jkGuiBuildMulti_menuEditCharacter);
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuEditCharacter, JKGUI_XBOX_BTN_A, 100, L"New");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuEditCharacter, JKGUI_XBOX_BTN_B, -1, L"Back");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuEditCharacter, JKGUI_XBOX_BTN_Y, 100, L"New");
            v9 = jkGuiRend_DisplayAndReturnClicked(&jkGuiBuildMulti_menuEditCharacter);
#else
            v9 = 100;
#endif
        }
        switch ( v9 )
        {
            case -1:
                goto LABEL_8;
            case 1:
                pEntry = jkGuiRend_GetStringEntry(&darr, jkGuiBuildMulti_menuEditCharacter_buttons[3].selectedTextEntry);
                _wcsncpy(wPlayerName, pEntry->str, 0x1Fu);
                wPlayerName[31] = 0;
                v3 = 1;
                if ( jkPlayer_VerifyWcharName(wPlayerName) )
                {
                    jkPlayer_MPCParse(
                        &jkGuiBuildMulti_aMpcInfo[jkGuiBuildMulti_menuEditCharacter_buttons[3].selectedTextEntry],
                        &jkPlayer_playerInfos[playerThingIdx],
                        jkPlayer_playerShortName,
                        wPlayerName,
                        1);
                    jkGuiBuildMulti_ShowEditCharacter(0);
                    jkPlayer_MPCWrite(&jkPlayer_playerInfos[playerThingIdx], jkPlayer_playerShortName, wPlayerName);
#ifdef TARGET_XBOX
                    XDBG("ProfilePortrait: post edit regenerate request\n");
                    jkGuiBuildMulti_XboxWriteLatestOrGeneratePortraitCache(wPlayerName);
#endif
                    v1 = jkGuiBuildMulti_menuEditCharacter_buttons[3].selectedTextEntry;
                }
                else
                {
                    jkGuiBuildMulti_menuEditCharacter_buttons[0].wstr = jkStrings_GetUniStringWithFallback("ERR_BAD_PLAYER_NAME");
                }
                break;
            case 100:
                if ( jkGuiBuildMulti_ShowNewCharacter(-1, 0, 0) < 0 && !v2 ) // MOTS altered TODO
LABEL_8:
                    v3 = 0;
                break;
            case 102:
                // MOTS added a tmp array here?
                v6 = jkGuiRend_GetString(&darr, jkGuiBuildMulti_menuEditCharacter_buttons[3].selectedTextEntry);
                v7 = jkStrings_GetUniStringWithFallback("GUI_CONFIRM_REMOVE_PLAYER");
                jk_snwprintf(wtmp1, 0x100u, v7, v6);
                v8 = jkStrings_GetUniStringWithFallback("GUI_REMOVE");
                if ( jkGuiDialog_YesNoDialog(v8, wtmp1) )
                {
                    jkPlayer_MPCMakePath(aMpcFPath, 128, jkPlayer_playerShortName, v6);
                    stdFileUtil_DelFile(aMpcFPath);
                }
                v3 = 1;
                v1 = 0;
                break;
            default:
                break;
        }
    }
    while ( v3 );
    jkGuiBuildMulti_bRendering = 0; // Added
    jkGuiRend_DarrayFree(&darr);
    jkGui_SetModeGame();
    return v9;
}

int jkGuiBuildMulti_Show2(Darray *pDarray, jkGuiElement *pElement, int minIdk, int maxIdk, int idx)
{
    int v5; // ebp
    stdFileSearch *v7; // edi
    jkPlayerMpcInfo *v8; // esi
    int v9; // eax
    char a2a[32]; // [esp+14h] [ebp-1640h] BYREF
    char a1[32]; // [esp+34h] [ebp-1620h] BYREF
    wchar_t name[32]; // [esp+54h] [ebp-1600h] BYREF
    char path[128]; // [esp+94h] [ebp-15C0h] BYREF
    char fpath[128]; // [esp+114h] [ebp-1540h] BYREF
    stdFileSearchResult v16; // [esp+194h] [ebp-14C0h] BYREF
    sithPlayerInfo playerInfo; // [esp+2A0h] [ebp-13B4h] BYREF

    v5 = 0;
    stdString_WcharToChar(a1, jkPlayer_playerShortName, 31);
    a1[31] = 0;
    jkGuiRend_DarrayFreeEntry(pDarray);
    stdString_snprintf(path, 128, "player\\%s", a1);
    pElement->selectedTextEntry = idx;
    v7 = stdFileUtil_NewFind(path, 3, "mpc");
    if ( v7 )
    {
        v8 = jkGuiBuildMulti_aMpcInfo;
        while ( stdFileUtil_FindNext(v7, &v16) )
        {
            if (v8 >= &jkGuiBuildMulti_aMpcInfo[32]) break;

            stdString_snprintf(fpath, 128, "%s\\%s", path, v16.fpath);
            if ( util_FileExistsLowLevel(fpath) ) // Added: util_FileExists -> util_FileExistsLowLevel
            {
                _strncpy(a2a, v16.fpath, 0x1Fu);
                a2a[31] = 0;
                stdFnames_StripExtAndDot(a2a);
                stdString_CharToWchar(name, a2a, 31);
                name[31] = 0;
                jkPlayer_MPCParse(v8, &playerInfo, jkPlayer_playerShortName, name, 1);
                v9 = jkPlayer_GetJediRank();
                if ( v9 >= minIdk && v9 <= maxIdk )
                {
                    jkGuiRend_AddStringEntry(pDarray, a2a, 0);
                    if ( !__strcmpi(a2a, a1) )
                        pElement->selectedTextEntry = v5;
                    ++v5;
                    ++v8;
                }
            }
        }
        stdFileUtil_DisposeFind(v7);
    }
    jkGuiRend_DarrayReallocStr(pDarray, 0, 0);
    jkGuiRend_SetClickableString(pElement, pDarray);
    return v5;
}

// MOTS altered TODO
int jkGuiBuildMulti_ShowNewCharacter(int rank, int bGameFormatIsJK, int bHasNoValidChars)
{
    wchar_t *v4; // eax
    int32_t v5; // esi
    wchar_t *v6; // eax
    int v7; // esi
    int v8; // ebp
    wchar_t *v9; // eax
    wchar_t *a2a; // [esp+0h] [ebp-1A8h]
    wchar_t *a2b; // [esp+0h] [ebp-1A8h]
    char v15[32]; // [esp+18h] [ebp-190h] BYREF
    char v18[128]; // [esp+128h] [ebp-80h] BYREF

    // MOTS added
    Darray daPersonalities;
    char personalityTmp[128];

    // MOTS added
    if (Main_bMotsCompat && bGameFormatIsJK == 0) {
        jkGuiBuildMulti_jediRank = 8;
    }
    else {
        jkGuiBuildMulti_jediRank = rank;
    }

    // MOTS added
    jkGuiRend_DarrayNewStr(&daPersonalities,8,1);
    for (int i = 0; i < 8; i++)
    {
        if ((bGameFormatIsJK == 0) || (i == 0)) {
            stdString_snprintf(personalityTmp, 128, "GUI_PERSONALITY%d", i + 1); // Added: sprintf -> snprintf
            wchar_t* pwVar1 = jkStrings_GetUniString(personalityTmp);
            if (pwVar1 == NULL) break;
            jkGuiRend_DarrayReallocStr(&daPersonalities, pwVar1, 0);
        }
    }
    jkGuiRend_DarrayReallocStr(&daPersonalities,(wchar_t *)0x0,0);

    // MOTS added
    jkPlayer_personality = 1;
    if (Main_bMotsCompat) {
        jkGuiRend_SetClickableString(&jkGuiBuildMulti_pNewCharacterElements[12],&daPersonalities);
        jkGuiBuildMulti_pNewCharacterElements[12].selectedTextEntry = 0;
    }

    // MOTS: 11 -> 14
    jkGuiBuildMulti_pNewCharacterElements[14].wstr = jkGuiBuildMulti_aWchar_5594C8; // 11
    memset(jkGuiBuildMulti_aWchar_5594C8, 0, 0x20u);
    jkGuiBuildMulti_pNewCharacterElements[14].selectedTextEntry = 16; // 11
#ifdef TARGET_XBOX
    jkGuiBuildMulti_pNewCharacterElements[14].clickHandlerFunc = jkGuiXboxKeyboard_TextBoxClicked;
    jkGuiXboxKeyboard_Show(jkGuiBuildMulti_aWchar_5594C8, 16, jkStrings_GetUniStringWithFallback("GUI_NEW_CHARACTER"));
#endif
    if ( bHasNoValidChars )
    {
        jkGuiDialog_ErrorDialog(jkStrings_GetUniStringWithFallback("GUI_NOVALIDCHARTITLE"), jkStrings_GetUniStringWithFallback("GUI_NOVALIDCHARACTERS"));
    }
    jk_snwprintf(&jkGuiBuildMulti_wTmp[64], 0x40u, jkStrings_GetUniStringWithFallback("GUI_S_MULTIPLAYER_CHARACTERS"), jkPlayer_playerShortName);
    jkGuiBuildMulti_pNewCharacterElements[2].wstr = &jkGuiBuildMulti_wTmp[64]; // 2

    if (Main_bMotsCompat) {
        if ( rank < 0 )
        {
            jk_snwprintf(jkGuiBuildMulti_waTmpRankLabel, 0x80u, jkStrings_GetUniStringWithFallback("GUI_RANKLABEL"), rank);
            jkGuiBuildMulti_pNewCharacterElements[7].wstr = jkGuiBuildMulti_waTmpRankLabel;
        }
        else
        {
            jk_snwprintf(jkGuiBuildMulti_waTmpRankLabel, 0x80u, jkStrings_GetUniStringWithFallback("GUI_RANKLABELMAX"), rank);
            jkGuiBuildMulti_pNewCharacterElements[7].wstr = jkGuiBuildMulti_waTmpRankLabel;
        }
    }
    else {
        if ( rank < 0 )
        {
            jkGuiRend_SetVisibleAndDraw(&jkGuiBuildMulti_pNewCharacterElements[5], jkGuiBuildMulti_pNewCharacterMenu, 0); // 4
            jkGuiBuildMulti_pNewCharacterElements[6].wstr = NULL; // 5
        }
        else
        {
            jkGuiRend_SetVisibleAndDraw(&jkGuiBuildMulti_pNewCharacterElements[5], jkGuiBuildMulti_pNewCharacterMenu, 1); // 4
            stdString_snprintf(v15, 32, "RANK_%d_L", rank);
            a2a = jkStrings_GetUniStringWithFallback(v15);
            v4 = jkStrings_GetUniStringWithFallback("GUI_RANK");
            jk_snwprintf(&jkGuiBuildMulti_wTmp[32], 0x80u, v4, rank, a2a);
            jkGuiBuildMulti_pNewCharacterElements[6].wstr = &jkGuiBuildMulti_wTmp[32]; // 5
        }
    }
    
    v5 = rank < 0 ? 0 : rank;
    jkPlayer_SetRank(v5);
    stdString_snprintf(v15, 32, "RANK_%d_L", v5);
    a2b = jkStrings_GetUniStringWithFallback(v15);
    v6 = jkStrings_GetUniStringWithFallback("GUI_RANK");
    jk_snwprintf(jkGuiBuildMulti_wTmp, 0x80u, v6, v5, a2b);
    jkGuiBuildMulti_pNewCharacterElements[8].wstr = jkGuiBuildMulti_wTmp; // 7
    jkGuiBuildMulti_pNewCharacterElements[0].wstr = NULL; // 0

    if (Main_bMotsCompat) {
        jkGuiBuildMulti_pNewCharacterElements[5].bIsVisible = 1;
        jkGuiBuildMulti_pNewCharacterElements[6].bIsVisible = 1;
        if (bGameFormatIsJK == 0) {
            jkGuiBuildMulti_pNewCharacterElements[6].selectedTextEntry = 0;
            jkGuiBuildMulti_pNewCharacterElements[5].selectedTextEntry = 1;
            jkGuiBuildMulti_pNewCharacterElements[7].bIsVisible = 0;
            jkGuiBuildMulti_pNewCharacterElements[8].bIsVisible = 0;
            jkGuiBuildMulti_pNewCharacterElements[9].bIsVisible = 0;
            jkGuiBuildMulti_pNewCharacterElements[10].bIsVisible = 0;
            jkGuiBuildMulti_pNewCharacterElements[11].bIsVisible = 1;
            jkGuiBuildMulti_pNewCharacterElements[12].bIsVisible = 1;
            if (bHasNoValidChars != 0) {
                jkGuiBuildMulti_pNewCharacterElements[6].bIsVisible = 0;
            }
        }
        else {
            jkGuiBuildMulti_pNewCharacterElements[6].selectedTextEntry = 1;
            jkGuiBuildMulti_pNewCharacterElements[5].selectedTextEntry = 0;
            jkGuiBuildMulti_pNewCharacterElements[7].bIsVisible = 1;
            jkGuiBuildMulti_pNewCharacterElements[8].bIsVisible = 1;
            jkGuiBuildMulti_pNewCharacterElements[9].bIsVisible = 1;
            jkGuiBuildMulti_pNewCharacterElements[10].bIsVisible = 1;
            jkGuiBuildMulti_pNewCharacterElements[11].bIsVisible = 0;
            jkGuiBuildMulti_pNewCharacterElements[12].bIsVisible = 0;
            if (bHasNoValidChars != 0) {
                jkGuiBuildMulti_pNewCharacterElements[5].bIsVisible = 0;
            }
        }
    }

    do
    {
        v7 = 0;
        jkGuiRend_MenuSetReturnKeyShortcutElement(jkGuiBuildMulti_pNewCharacterMenu, &jkGuiBuildMulti_pNewCharacterElements[16]); // 13
        jkGuiRend_MenuSetEscapeKeyShortcutElement(jkGuiBuildMulti_pNewCharacterMenu, &jkGuiBuildMulti_pNewCharacterElements[15]); // 12
#ifdef TARGET_XBOX
        jkGuiBuildMulti_pNewCharacterElements[15].bIsVisible = 0;
        jkGuiBuildMulti_pNewCharacterElements[16].bIsVisible = 0;
        jkGuiRend_XboxFooterBegin(jkGuiBuildMulti_pNewCharacterMenu);
        jkGuiRend_XboxFooterAddAction(jkGuiBuildMulti_pNewCharacterMenu, JKGUI_XBOX_BTN_A, 0, L"Select");
        jkGuiRend_XboxFooterAddAction(jkGuiBuildMulti_pNewCharacterMenu, JKGUI_XBOX_BTN_B, -1, L"Cancel");
        jkGuiRend_XboxFooterAddAction(jkGuiBuildMulti_pNewCharacterMenu, JKGUI_XBOX_BTN_START, 1, L"Done");
        jkGuiRend_XboxSetInitialFocus(jkGuiBuildMulti_pNewCharacterMenu, &jkGuiBuildMulti_pNewCharacterElements[14]);
#endif
        v8 = jkGuiRend_DisplayAndReturnClicked(jkGuiBuildMulti_pNewCharacterMenu);
        if ( v8 != 1 )
            goto LABEL_16;
        if ( jkGuiBuildMulti_aWchar_5594C8[0] )
        {
            if ( jkPlayer_VerifyWcharName(jkGuiBuildMulti_aWchar_5594C8) )
            {
                jkPlayer_MPCMakePath(v18, 128, jkPlayer_playerShortName, jkGuiBuildMulti_aWchar_5594C8);
                if ( !util_FileExistsLowLevel(v18) ) // Added: util_FileExists -> util_FileExistsLowLevel
                    goto LABEL_16;
                v7 = 1;
                v9 = jkStrings_GetUniStringWithFallback("ERR_PLAYER_ALREADY_EXISTS");
            }
            else
            {
                v7 = 1;
                memset(jkGuiBuildMulti_aWchar_5594C8, 0, 0x20u);
                v9 = jkStrings_GetUniStringWithFallback("ERR_BAD_PLAYER_NAME");
            }
        }
        else
        {
            v7 = 1;
            v9 = jkStrings_GetUniStringWithFallback("ERR_NO_PLAYER_NAME");
        }
        jkGuiBuildMulti_pNewCharacterElements[0].wstr = v9; // 8
LABEL_16:
        if ( v8 == -1 ) {
            jkGuiRend_DarrayFree(&daPersonalities); // MOTS added
            return -1;
        }
    }
    while ( v7 );
    sithPlayer_SetBinAmt(SITHBIN_SPEND_STARS, (flex_d_t)jkPlayer_GetJediRank() * 3.0);
    sithPlayer_SetBinAmt(SITHBIN_NEW_STARS, 0.0);
    if (Main_bMotsCompat) {
        if (jkGuiBuildMulti_pNewCharacterElements[5].selectedTextEntry == 0) {
            jkPlayer_personality = 1;
        }
        else {
            jkPlayer_SetRank(7);
            jkPlayer_personality = jkGuiBuildMulti_pNewCharacterElements[12].selectedTextEntry + 1;
        }
        jkPlayer_SetAmmoMaximums(jkPlayer_personality);
    }
    jkPlayer_ResetPowers();
    if (Main_bMotsCompat) {
        jkPlayer_SyncForcePowers(jkPlayer_GetJediRank(), 1);
    }
    jkPlayer_SetPlayerName(jkGuiBuildMulti_aWchar_5594C8);
    jkPlayer_mpcInfoSet = 0;
    jkGuiBuildMulti_ShowEditCharacter(1);
    jkPlayer_MPCWrite(&jkPlayer_playerInfos[playerThingIdx], jkPlayer_playerShortName, jkGuiBuildMulti_aWchar_5594C8);
#ifdef TARGET_XBOX
    XDBG("ProfilePortrait: post new regenerate request\n");
    jkGuiBuildMulti_XboxWriteLatestOrGeneratePortraitCache(jkGuiBuildMulti_aWchar_5594C8);
#endif
    jkGuiRend_DarrayFree(&daPersonalities); // MOTS added
    return v8;
}


int jkGuiBuildMulti_FUN_00420930(jkGuiElement *pElement,jkGuiMenu *pMenu,int32_t mouseX,int32_t mouseY, BOOL redraw)
{
    jkGuiBuildMulti_pNewCharacterElements[6].selectedTextEntry = 1;
    jkGuiBuildMulti_pNewCharacterElements[5].selectedTextEntry = 0;
    if (pMenu != (jkGuiMenu *)0x0) {
        jkGuiRend_UpdateAndDrawClickable(pElement,pMenu,1);
    }
    jkGuiBuildMulti_pNewCharacterElements[7].bIsVisible = 1;
    jkGuiBuildMulti_pNewCharacterElements[8].bIsVisible = 1;
    jkGuiBuildMulti_pNewCharacterElements[9].bIsVisible = 1;
    jkGuiBuildMulti_pNewCharacterElements[10].bIsVisible = 1;
    jkGuiBuildMulti_pNewCharacterElements[11].bIsVisible = 0;
    jkGuiBuildMulti_pNewCharacterElements[12].bIsVisible = 0;

    // Added: Prevent infloop
    if (!pMenu->focusedElement->bIsVisible) {
        pMenu->focusedElement = &jkGuiBuildMulti_pNewCharacterElements[14];
    }

    if (pMenu != (jkGuiMenu *)0x0) {
        jkGuiRend_Paint(pMenu);
    }
    return 0;
}

int jkGuiBuildMulti_FUN_004209b0(jkGuiElement *pElement,jkGuiMenu *pMenu, int32_t mouseX, int32_t mouseY, BOOL redraw)
{
    jkGuiBuildMulti_pNewCharacterElements[6].selectedTextEntry = 0;
    jkGuiBuildMulti_pNewCharacterElements[5].selectedTextEntry = 1;
    if (pMenu != (jkGuiMenu *)0x0) {
        jkGuiRend_UpdateAndDrawClickable(pElement,pMenu,1);
    }
    jkGuiBuildMulti_pNewCharacterElements[7].bIsVisible = 0;
    jkGuiBuildMulti_pNewCharacterElements[8].bIsVisible = 0;
    jkGuiBuildMulti_pNewCharacterElements[9].bIsVisible = 0;
    jkGuiBuildMulti_pNewCharacterElements[10].bIsVisible = 0;
    jkGuiBuildMulti_pNewCharacterElements[11].bIsVisible = 1;
    jkGuiBuildMulti_pNewCharacterElements[12].bIsVisible = 1;

    // Added: Prevent infloop
    if (!pMenu->focusedElement->bIsVisible) {
        pMenu->focusedElement = &jkGuiBuildMulti_pNewCharacterElements[14];
    }

    if (pMenu != (jkGuiMenu *)0x0) {
        jkGuiRend_Paint(pMenu);
    }
    return 0;
}

int jkGuiBuildMulti_menuNewCharacter_rankArrowButtonClickHandler(jkGuiElement *pElement, jkGuiMenu *pMenu, int32_t mouseX, int32_t mouseY, BOOL a5)
{
    int32_t v2; // esi
    wchar_t *v3; // eax
    int32_t v4; // esi
    int32_t v6; // [esp-8h] [ebp-1Ch]
    wchar_t *v7; // [esp-4h] [ebp-18h]
    char tmp[32+1]; // [esp+4h] [ebp-10h] BYREF

    if ( pElement->hoverId == 103 )
    {
        v4 = jkPlayer_GetJediRank() - 1;
        if ( v4 < 0 )
            v4 = 8;
        jkPlayer_SetRank(v4);
        stdString_snprintf(tmp, 32, "RANK_%d_L", v4);
        v7 = jkStrings_GetUniStringWithFallback(tmp);
        v6 = v4;
        v3 = jkStrings_GetUniStringWithFallback("GUI_RANK");
        goto LABEL_9;
    }
    if ( pElement->hoverId == 104 )
    {
        v2 = jkPlayer_GetJediRank() + 1;
        if ( v2 > 8 )
            v2 = 0;
        jkPlayer_SetRank(v2);
        stdString_snprintf(tmp, 32, "RANK_%d_L", v2);
        v7 = jkStrings_GetUniStringWithFallback(tmp);
        v6 = v2;
        v3 = jkStrings_GetUniStringWithFallback("GUI_RANK");
LABEL_9:
        jk_snwprintf(jkGuiBuildMulti_wTmp, 0x80u, v3, v6, v7);
        jkGuiBuildMulti_pNewCharacterElements[8].wstr = jkGuiBuildMulti_wTmp;
        jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_pNewCharacterElements[8], pMenu, 1);
    }
    return 0;
}

int jkGuiBuildMulti_ShowLoad(jkPlayerMpcInfo *pPlayerMpcInfo, char *pStrEpisode, char *pJklFname, int minIdk, int rank, int bGameFormatIsJK)
{
    wchar_t *v5; // eax
    int v6; // eax
    uint32_t v7; // edi
    jkEpisode *v8; // ebp
    int v9; // esi
    wchar_t *v10; // eax
    int v11; // ebx
    int v12; // edi
    int v13; // ebp
    int v14; // esi
    jkGuiStringEntry *v16; // eax
    wchar_t *v17; // esi
    wchar_t *v18; // eax
    wchar_t *v19; // eax
    jkGuiStringEntry *v20; // eax
    wchar_t *v21; // [esp-4h] [ebp-420h]
    int v22; // [esp+10h] [ebp-40Ch]
    Darray darr; // [esp+14h] [ebp-408h] BYREF
    wchar_t name[32]; // [esp+2Ch] [ebp-3F0h] BYREF
    char tmp5[32]; // [esp+6Ch] [ebp-3B0h] BYREF
    stdStrTable strtable; // [esp+8Ch] [ebp-390h] BYREF
    char tmp2[128]; // [esp+11Ch] [ebp-300h] BYREF
    wchar_t wtmp1[256]; // [esp+21Ch] [ebp-200h] BYREF

    if (!Main_bMotsCompat) {
        jkGuiBuildMulti_menuLoadCharacter_buttons[16].bIsVisible = 0;
        jkGuiBuildMulti_menuLoadCharacter_buttons[17].bIsVisible = 0;

        jkGuiBuildMulti_menuLoadCharacter_buttons[10].rect.y = 210;
        jkGuiBuildMulti_menuLoadCharacter_buttons[11].rect.y = 230;

        jkGuiBuildMulti_menuLoadCharacter_buttons[12].rect.y = 270;
        jkGuiBuildMulti_menuLoadCharacter_buttons[13].rect.y = 290;

        jkGuiBuildMulti_menuLoadCharacter_buttons[14].rect.y = 330;
        jkGuiBuildMulti_menuLoadCharacter_buttons[15].rect.y = 350;
    }
    else {
        jkGuiBuildMulti_menuLoadCharacter_buttons[16].bIsVisible = 1;
        jkGuiBuildMulti_menuLoadCharacter_buttons[17].bIsVisible = 1;

        jkGuiBuildMulti_menuLoadCharacter_buttons[10].rect.y = 330;
        jkGuiBuildMulti_menuLoadCharacter_buttons[11].rect.y = 350;

        jkGuiBuildMulti_menuLoadCharacter_buttons[12].rect.y = 270;
        jkGuiBuildMulti_menuLoadCharacter_buttons[13].rect.y = 290;

        jkGuiBuildMulti_menuLoadCharacter_buttons[14].rect.y = 210;
        jkGuiBuildMulti_menuLoadCharacter_buttons[15].rect.y = 230;
    }

    name[0] = 0;
    memset(&name[1], 0, 0x3Cu);
    name[31] = 0;
    tmp5[0] = 0;
    memset(&tmp5[1], 0, 0x1Cu);
    tmp5[29] = 0;
    tmp5[30] = 0;
    tmp5[31] = 0;
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_BUILD_LOAD]->palette);
    jkGuiRend_DarrayNewStr(&darr, 5, 1);
    jkGuiBuildMulti_menuLoadCharacter_buttons[3].clickHandlerFunc = jkGuiBuildMulti_sub_41D830;
    jkGuiBuildMulti_menuLoadCharacter_buttons[0].unistr = 0;
    v5 = jkStrings_GetUniStringWithFallback("GUI_S_MULTIPLAYER_CHARACTERS");
    jk_snwprintf(&jkGuiBuildMulti_wTmp[64], 0x40u, v5, jkPlayer_playerShortName);
    jkGuiBuildMulti_menuLoadCharacter_buttons[2].wstr = &jkGuiBuildMulti_wTmp[64];
    jkEpisode_LoadVerify();
    v6 = -1;
    v7 = 0;
    if ( jkEpisode_var2 )
    {
        v8 = jkEpisode_aEpisodes;
        while ( strcmp(pStrEpisode, v8->name) )
        {
            ++v7;
            ++v8;
            if ( v7 >= jkEpisode_var2 )
            {
                v6 = -1;
                goto LABEL_7;
            }
        }
        v6 = v7;
    }
LABEL_7:
    if ( v6 == -1 )
        jkGuiBuildMulti_menuLoadCharacter_buttons[5].wstr = 0;
    else
        jkGuiBuildMulti_menuLoadCharacter_buttons[5].wstr = jkEpisode_aEpisodes[v6].unistr;
    jkRes_LoadGob(pStrEpisode);
    stdStrTable_Load(&strtable, "misc\\cogStrings.uni");
    v9 = rank;
    jkGuiBuildMulti_menuLoadCharacter_buttons[7].wstr = jkGuiTitle_quicksave_related_func1(&strtable, pJklFname);
    stdString_snprintf(tmp5, 32, "RANK_%d_L", rank);
    v21 = jkStrings_GetUniStringWithFallback(tmp5);
    v10 = jkStrings_GetUniStringWithFallback("GUI_RANK");
    jk_snwprintf(&jkGuiBuildMulti_wTmp[32], 0x80u, v10, rank, v21);
    jkGuiBuildMulti_menuLoadCharacter_buttons[9].wstr = &jkGuiBuildMulti_wTmp[32];
    v11 = 0;
    while ( 1 )
    {
        v12 = jkGuiBuildMulti_Show2(&darr, &jkGuiBuildMulti_menuLoadCharacter_buttons[3], minIdk, v9, v11);
        jkGuiBuildMulti_sub_41D680(&jkGuiBuildMulti_menuLoadCharacter, jkGuiBuildMulti_menuLoadCharacter_buttons[3].selectedTextEntry);
        v13 = 0;
        v14 = 1;
        if ( v12 )
        {
            jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiBuildMulti_menuLoadCharacter, &jkGuiBuildMulti_menuLoadCharacter_buttons[22]);
            jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiBuildMulti_menuLoadCharacter, &jkGuiBuildMulti_menuLoadCharacter_buttons[18]);
#ifdef TARGET_XBOX
            jkGuiBuildMulti_menuLoadCharacter_buttons[18].bIsVisible = 0;
            jkGuiBuildMulti_menuLoadCharacter_buttons[19].bIsVisible = 0;
            jkGuiBuildMulti_menuLoadCharacter_buttons[20].bIsVisible = 0;
            jkGuiBuildMulti_menuLoadCharacter_buttons[21].bIsVisible = 0;
            jkGuiBuildMulti_menuLoadCharacter_buttons[22].bIsVisible = 0;
            jkGuiRend_XboxFooterBegin(&jkGuiBuildMulti_menuLoadCharacter);
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuLoadCharacter, JKGUI_XBOX_BTN_A, 1, L"Load");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuLoadCharacter, JKGUI_XBOX_BTN_B, -1, L"Back");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuLoadCharacter, JKGUI_XBOX_BTN_X, 102, L"Remove");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuLoadCharacter, JKGUI_XBOX_BTN_Y, 100, L"New");
            jkGuiRend_XboxFooterAddAction(&jkGuiBuildMulti_menuLoadCharacter, JKGUI_XBOX_BTN_START, 101, L"Edit");
            jkGuiRend_XboxSetInitialFocus(&jkGuiBuildMulti_menuLoadCharacter, &jkGuiBuildMulti_menuLoadCharacter_buttons[3]);
#endif
            v22 = jkGuiRend_DisplayAndReturnClicked(&jkGuiBuildMulti_menuLoadCharacter);
        }
        else
        {
            v13 = 1;
            v22 = 100;
        }
        switch ( v22 )
        {
            case -1:
                goto LABEL_18;
            case 1:
                v20 = jkGuiRend_GetStringEntry(&darr, jkGuiBuildMulti_menuLoadCharacter_buttons[3].selectedTextEntry);
                _wcsncpy(name, v20->str, 0x1Fu);
                v14 = 0;
                if ( jkPlayer_VerifyWcharName(name) )
                {
                    jkPlayer_MPCParse(pPlayerMpcInfo, &jkPlayer_playerInfos[playerThingIdx], jkPlayer_playerShortName, name, 1);
                }
                else
                {
                    v14 = 1;
                    jkGuiBuildMulti_menuLoadCharacter_buttons[0].wstr = jkStrings_GetUniStringWithFallback("ERR_BAD_PLAYER_NAME");
                }
                break;
            case 100:
                if ( jkGuiBuildMulti_ShowNewCharacter(rank, bGameFormatIsJK, v13) < 0 && !v12 ) // MOTS altered TODO
LABEL_18:
                    v14 = 0;
                break;
            case 101:
                v16 = jkGuiRend_GetStringEntry(&darr, jkGuiBuildMulti_menuLoadCharacter_buttons[3].selectedTextEntry);
                _wcsncpy(name, v16->str, 0x1Fu);
                v14 = 1;
                if ( jkPlayer_VerifyWcharName(name) )
                {
                    jkPlayer_MPCParse(
                        &jkGuiBuildMulti_aMpcInfo[jkGuiBuildMulti_menuLoadCharacter_buttons[3].selectedTextEntry],
                        &jkPlayer_playerInfos[playerThingIdx],
                        jkPlayer_playerShortName,
                        name,
                        1);
                    jkGuiBuildMulti_ShowEditCharacter(0);
                    jkPlayer_MPCWrite(&jkPlayer_playerInfos[playerThingIdx], jkPlayer_playerShortName, name);
#ifdef TARGET_XBOX
                    XDBG("ProfilePortrait: post load edit regenerate request\n");
                    jkGuiBuildMulti_XboxWriteLatestOrGeneratePortraitCache(name);
#endif
                    v11 = jkGuiBuildMulti_menuLoadCharacter_buttons[3].selectedTextEntry;
                }
                else
                {
                    jkGuiBuildMulti_menuLoadCharacter_buttons[0].wstr = jkStrings_GetUniStringWithFallback("ERR_BAD_PLAYER_NAME");
                }
                break;
            case 102:
                v17 = jkGuiRend_GetString(&darr, jkGuiBuildMulti_menuLoadCharacter_buttons[3].selectedTextEntry);
                v18 = jkStrings_GetUniStringWithFallback("GUI_CONFIRM_REMOVE_PLAYER");
                jk_snwprintf(wtmp1, 0x100u, v18, v17);
                v19 = jkStrings_GetUniStringWithFallback("GUI_REMOVE");
                if ( jkGuiDialog_YesNoDialog(v19, wtmp1) )
                {
                    jkPlayer_MPCMakePath(tmp2, 128, jkPlayer_playerShortName, v17);
                    stdFileUtil_DelFile(tmp2);
                }
                v14 = 1;
                v11 = 0;
                break;
            default:
                break;
        }
        if ( !v14 )
            break;
        v9 = rank;
    }
    jkGuiRend_DarrayFree(&darr);
    stdStrTable_Free(&strtable); // Added: memleak
    jkGui_SetModeGame();
    return v22;
}


void jkGuiBuildMulti_sub_41D680(jkGuiMenu *pMenu, int idx)
{
    wchar_t *v2; // eax
    wchar_t *v3; // eax
    wchar_t *v4; // eax
    wchar_t *v5; // eax
    int v6; // [esp-8h] [ebp-1Ch]
    int v7; // [esp-8h] [ebp-1Ch]
    int v8; // [esp-4h] [ebp-18h]
    wchar_t *v9; // [esp-4h] [ebp-18h]
    int v10; // [esp-4h] [ebp-18h]
    wchar_t *v11; // [esp-4h] [ebp-18h]
    char tmp1[32]; // [esp+4h] [ebp-10h] BYREF

    if ( pMenu == &jkGuiBuildMulti_menuEditCharacter )
    {
        v8 = jkGuiBuildMulti_aMpcInfo[idx].jediRank;
        jkGuiBuildMulti_menuEditCharacter_buttons[5].wstr = jkGuiBuildMulti_aMpcInfo[idx].name;
        stdString_snprintf(tmp1, 32, "RANK_%d_L", v8);
        v9 = jkStrings_GetUniStringWithFallback(tmp1);
        v6 = jkGuiBuildMulti_aMpcInfo[idx].jediRank;
        v2 = jkStrings_GetUniStringWithFallback("GUI_RANK");
        jk_snwprintf(jkGuiBuildMulti_wTmp, 0x80u, v2, v6, v9);
        jkGuiBuildMulti_menuEditCharacter_buttons[7].wstr = jkGuiBuildMulti_wTmp;
        stdFnames_CopyShortName(tmp1, 16, jkGuiBuildMulti_aMpcInfo[idx].model);
        jkGuiTitle_sub_4189A0(tmp1);
        v3 = jkStrings_GetUniStringWithFallback(tmp1);
        jk_snwprintf(jkGuiBuildMulti_wTmp2, 0x20, L"%s", v3); // ADDED: swprintf -> snwprintf
        jkGuiBuildMulti_menuEditCharacter_buttons[9].wstr = jkGuiBuildMulti_wTmp2;

        if (Main_bMotsCompat) {
            stdString_snprintf(tmp1, 32, "GUI_PERSONALITY%d", jkGuiBuildMulti_aMpcInfo[idx].personality); // Added: sprintf -> snprintf
            v3 = jkStrings_GetUniStringWithFallback(tmp1);

            jk_snwprintf(jkGuiBuildMulti_wTmp3, 0x20, L"%s", v3); // ADDED: swprintf -> snwprintf
            jkGuiBuildMulti_menuEditCharacter_buttons[11].wstr = jkGuiBuildMulti_wTmp3;
        }
    }
    else if ( pMenu == &jkGuiBuildMulti_menuLoadCharacter )
    {
        v10 = jkGuiBuildMulti_aMpcInfo[idx].jediRank;
        jkGuiBuildMulti_menuLoadCharacter_buttons[11].wstr = jkGuiBuildMulti_aMpcInfo[idx].name;
        stdString_snprintf(tmp1, 32, "RANK_%d_L", v10);
        v11 = jkStrings_GetUniStringWithFallback(tmp1);
        v7 = jkGuiBuildMulti_aMpcInfo[idx].jediRank;
        v4 = jkStrings_GetUniStringWithFallback("GUI_RANK");
        jk_snwprintf(jkGuiBuildMulti_wTmp, 0x80u, v4, v7, v11);
        jkGuiBuildMulti_menuLoadCharacter_buttons[13].wstr = jkGuiBuildMulti_wTmp;
        stdFnames_CopyShortName(tmp1, 16, jkGuiBuildMulti_aMpcInfo[idx].model);
        jkGuiTitle_sub_4189A0(tmp1);
        v5 = jkStrings_GetUniStringWithFallback(tmp1);
        jk_snwprintf(jkGuiBuildMulti_wTmp2, 0x20, L"%s", v5); // ADDED: swprintf -> snwprintf
        jkGuiBuildMulti_menuLoadCharacter_buttons[15].wstr = jkGuiBuildMulti_wTmp2;

        if (Main_bMotsCompat) {
            stdString_snprintf(tmp1, 32, "GUI_PERSONALITY%d", jkGuiBuildMulti_aMpcInfo[idx].personality); // Added: sprintf -> snprintf
            v3 = jkStrings_GetUniStringWithFallback(tmp1);

            jk_snwprintf(jkGuiBuildMulti_wTmp3, 0x20, L"%s", v3); // ADDED: swprintf -> snwprintf
            jkGuiBuildMulti_menuLoadCharacter_buttons[17].wstr = jkGuiBuildMulti_wTmp3;
        }
    }
}

int jkGuiBuildMulti_sub_41D830(jkGuiElement *pElement, jkGuiMenu *pMenu, int32_t mouseX, int32_t mouseY, BOOL redraw)
{
    if ( mouseX != -1 || mouseY != -1 )
        jkGuiRend_ClickSound(pElement, pMenu, mouseX, mouseY, redraw);
    jkGuiBuildMulti_sub_41D680(pMenu, pElement->selectedTextEntry);
    if ( pMenu == &jkGuiBuildMulti_menuEditCharacter )
    {
        jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_menuEditCharacter_buttons[5], pMenu, 1);
        jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_menuEditCharacter_buttons[7], pMenu, 1);
        jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_menuEditCharacter_buttons[9], pMenu, 1);
        if (Main_bMotsCompat) {
            jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_menuEditCharacter_buttons[11], pMenu, 1);
        }
        return redraw != 0;
    }
    else
    {
        if ( pMenu == &jkGuiBuildMulti_menuLoadCharacter )
        {
            jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_menuLoadCharacter_buttons[11], pMenu, 1);
            jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_menuLoadCharacter_buttons[13], pMenu, 1);
            jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_menuLoadCharacter_buttons[15], pMenu, 1);
        }
        return redraw != 0;
    }
    return 0;
}
