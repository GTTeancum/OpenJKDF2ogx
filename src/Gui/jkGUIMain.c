#include "jkGUIMain.h"

#include "General/stdBitmap.h"
#include "General/stdFont.h"
#include "Engine/rdMaterial.h" // TODO move stdVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "World/jkPlayer.h"
#include "Main/jkStrings.h"
#include "General/stdFnames.h"
#include "General/Darray.h"
#include "Gui/jkGUITitle.h"
#include "Gui/jkGUISingleplayer.h"
#include "Gui/jkGUIMultiplayer.h"
#include "Gui/jkGUIBuildMulti.h"
#include "Gui/jkGUIDialog.h"
#include "Gui/jkGUIPlayer.h"
#include "Gui/jkGUISetup.h"
#include "Gui/jkGUIMods.h"
#include "Win95/stdComm.h"
#include "Win95/stdDisplay.h"
#include "Win95/stdGdi.h"
#include "Win95/Windows.h"
#include "Main/Main.h"
#include "Main/jkMain.h"
#include "Main/jkRes.h"
#include "General/stdString.h"
#include "Platform/stdControl.h"
#include "General/util.h"
#include "General/stdFnames.h"
#include "Main/sithCvar.h"
#include "stdPlatform.h"
#ifdef TARGET_XBOX
#include "Platform/Xbox/xbox_debug.h"
#include "Platform/Xbox/xbox_splitscreen.h"
#include "Platform/Xbox/xbox_systemlink_probe.h"
#endif

// Added
extern int jkCredits_cdOverride;
static wchar_t jkGuiMain_versionBuffer[64];

static int jkGuiMain_bIdk = 1;
static int jkGuiCutscenes_initted;

#ifdef TARGET_XBOX
static int jkGuiMain_XboxReadSmokeAutostartLevel(char *out, size_t outSize)
{
    FILE *f;
    size_t len;

    if (!out || outSize == 0)
        return 0;

    out[0] = 0;
    f = fopen("D:\\xbox_smoke_autostart_level.txt", "rb");
    if (!f)
        return 0;

    len = fread(out, 1, outSize - 1, f);
    fclose(f);
    out[len] = 0;

    while (len && (out[len - 1] == '\r' || out[len - 1] == '\n' || out[len - 1] == ' ' || out[len - 1] == '\t'))
    {
        out[len - 1] = 0;
        len--;
    }

    return len != 0;
}
#endif

static int32_t jkGuiMain_listboxIdk[2] = {0xd, 0xe};

static jkGuiElement jkGuiMain_cutscenesElements[5] = {
    {ELEMENT_TEXT, 0, 5, "GUI_VIEWCUTSCENES", 3, {0, 50, 640, 60}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 2, 0, 0, {160, 135, 320, 240}, 1, 0, 0, 0, 0, jkGuiMain_listboxIdk, {0}, 0},
    {ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK", 3, {340, 400, 140, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, {150, 400, 180, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0}
};

static jkGuiMenu jkGuiMain_cutscenesMenu = {jkGuiMain_cutscenesElements, -1, 0xFFFF, 0xFFFF, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

static jkGuiElement jkGuiMain_elements[11] = {
#if !defined(TARGET_NO_MULTIPLAYER_MENUS) || defined(TARGET_XBOX)
    {ELEMENT_TEXTBUTTON, 10, 5, "GUI_SINGLEPLAYER", 3, {0, 160, 640, 60}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 11, 5, "GUI_MULTIPLAYER", 3, {0, 220, 640, 60}, 1, 0, 0, 0, 0, 0, {0}, 0},
#else
    {ELEMENT_TEXTBUTTON, 10, 5, "GUI_SINGLEPLAYER", 3, {0, 220, 640, 60}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 11, 5, NULL, 3, {0, 0, 0, 0}, 1, 0, 0, 0, 0, 0, {0}, 0},
#endif
#ifdef TARGET_XBOX
    {ELEMENT_TEXTBUTTON, 13, 5, "GUI_SETUP", 3, {0, 280, 640, 60}, 1, 0, 0, 0, 0, 0, {0}, 0},
#else
    {ELEMENT_TEXTBUTTON, 12, 5, "GUI_QUIT", 3, {0, 280, 640, 60}, 1, 0, 0, 0, 0, 0, {0}, 0},
#endif
    {ELEMENT_TEXTBUTTON, 14, 2, "GUI_CHOOSEPLAYER", 3, {20, 380, 150, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 15, 2, "GUI_VIEWCUTSCENES", 3, {250, 380, 150, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
#ifdef TARGET_XBOX
    {ELEMENT_TEXTBUTTON, 16, 2, "GUI_CREDITS", 3, {470, 380, 150, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
#else
    {ELEMENT_TEXTBUTTON, 13, 2, "GUI_SETUP", 3, {470, 380, 150, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
#endif
#ifdef QOL_IMPROVEMENTS
#ifdef TARGET_XBOX
    {ELEMENT_TEXT, 0, 0, NULL, 3, {0, 0, 0, 0}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 17, 2, L"Expansions & Mods", 3, {170, 430, 300, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
#else
    {ELEMENT_TEXTBUTTON, 16, 2, "GUI_CREDITS", 3, {130, 430, 150, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 17, 2, L"Expansions & Mods", 3, {370, 430, 150, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
#endif
    {ELEMENT_TEXT,  0,  0,  NULL,  3, {560, 440, 70, 15},  1,  0,  0,  0,  0,  0, {0},  0},
    {ELEMENT_TEXT,  0,  0,  NULL,  3, {560, 455, 70, 15},  1,  0,  0,  0,  0,  0, {0},  0},
#else
    {ELEMENT_TEXTBUTTON, 16, 2, "GUI_CREDITS", 3, {250, 420, 150, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
#endif
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0}
};

static jkGuiMenu jkGuiMain_menu = {jkGuiMain_elements, -1, 0xFFFF, 0xFFFF, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

#ifdef TARGET_XBOX
static jkGuiElement jkGuiMain_xboxMultiplayerElements[6] = {
    {ELEMENT_TEXT, 0, 5, L"Multiplayer", 3, {0, 50, 640, 60}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 20, 5, L"Split Screen", 3, {0, 150, 640, 50}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 21, 5, L"Character", 3, {0, 210, 640, 50}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 22, 5, L"System Link", 3, {0, 270, 640, 50}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, {230, 410, 180, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0}
};

static jkGuiMenu jkGuiMain_xboxMultiplayerMenu = {jkGuiMain_xboxMultiplayerElements, -1, 0xFFFF, 0xFFFF, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

enum
{
    JKGUI_XBOX_XSL_STATUS = 2,
    JKGUI_XBOX_XSL_PEER_COUNT = 3,
    JKGUI_XBOX_XSL_FIRST_PEER = 4,
    JKGUI_XBOX_XSL_HINT = 12,
    JKGUI_XBOX_XSL_BACK = 13
};

static wchar_t jkGuiMain_xboxSystemLinkStatus[96];
static wchar_t jkGuiMain_xboxSystemLinkPeerCount[64];
static wchar_t jkGuiMain_xboxSystemLinkPeers[XBOX_SYSTEMLINK_PROBE_MAX_PEERS][96];
static wchar_t jkGuiMain_xboxSystemLinkHint[96];
static int jkGuiMain_xboxSystemLinkLastStarted = -1;
static int jkGuiMain_xboxSystemLinkLastPort = -1;
static int jkGuiMain_xboxSystemLinkLastError = -1;
static unsigned long jkGuiMain_xboxSystemLinkLastId = 0;
static unsigned long jkGuiMain_xboxSystemLinkLastSent = 0;
static unsigned long jkGuiMain_xboxSystemLinkLastPeerPackets[XBOX_SYSTEMLINK_PROBE_MAX_PEERS];

static jkGuiElement jkGuiMain_xboxSystemLinkElements[15] = {
    {ELEMENT_TEXT, 0, 5, L"System Link Test", 3, {0, 36, 640, 52}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, L"Discovery Probe", 3, {0, 92, 640, 28}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxSystemLinkStatus, 3, {58, 132, 540, 24}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxSystemLinkPeerCount, 3, {58, 172, 540, 24}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 1, jkGuiMain_xboxSystemLinkPeers[0], 3, {62, 208, 540, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 1, jkGuiMain_xboxSystemLinkPeers[1], 3, {62, 234, 540, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 1, jkGuiMain_xboxSystemLinkPeers[2], 3, {62, 260, 540, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 1, jkGuiMain_xboxSystemLinkPeers[3], 3, {62, 286, 540, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 1, jkGuiMain_xboxSystemLinkPeers[4], 3, {62, 312, 540, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 1, jkGuiMain_xboxSystemLinkPeers[5], 3, {62, 338, 540, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 1, jkGuiMain_xboxSystemLinkPeers[6], 3, {62, 364, 540, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 1, jkGuiMain_xboxSystemLinkPeers[7], 3, {62, 390, 540, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxSystemLinkHint, 3, {58, 410, 540, 26}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, {230, 438, 180, 34}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0}
};

static jkGuiMenu jkGuiMain_xboxSystemLinkMenu = {jkGuiMain_xboxSystemLinkElements, -1, 0xFFFF, 0xFFFF, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

enum
{
    JKGUI_XBOX_READY_P1_LABEL = 1,
    JKGUI_XBOX_READY_P1_LIST = 2,
    JKGUI_XBOX_READY_P1_NAME = 3,
    JKGUI_XBOX_READY_P2_LABEL = 4,
    JKGUI_XBOX_READY_P2_LIST = 5,
    JKGUI_XBOX_READY_P2_NAME = 6,
    JKGUI_XBOX_READY_P3_LABEL = 7,
    JKGUI_XBOX_READY_P3_LIST = 8,
    JKGUI_XBOX_READY_P3_NAME = 9,
    JKGUI_XBOX_READY_P4_LABEL = 10,
    JKGUI_XBOX_READY_P4_LIST = 11,
    JKGUI_XBOX_READY_P4_NAME = 12,
    JKGUI_XBOX_READY_STATUS = 13,
    JKGUI_XBOX_READY_START = 14,
    JKGUI_XBOX_READY_CANCEL = 15
};

static int32_t jkGuiMain_xboxReadyListboxIdk[2] = {0xd, 0xe};
static Darray jkGuiMain_xboxReadyCharacters;
static int jkGuiMain_xboxReadyNumChars;
static int jkGuiMain_xboxReadyJoined[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static wchar_t jkGuiMain_xboxReadyNames[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS][32];
static wchar_t jkGuiMain_xboxReadyStatus[64];
static stdBitmap *jkGuiMain_xboxReadyPortraits[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static int jkGuiMain_xboxReadyPortraitSelection[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {-1, -1, -1, -1};
static int jkGuiMain_xboxReadyStartRequested;
static int jkGuiMain_xboxReadyPrevA[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static int jkGuiMain_xboxReadyPrevB[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static int jkGuiMain_xboxReadyPrevStart[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static int jkGuiMain_xboxReadyPrevUp[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static int jkGuiMain_xboxReadyPrevDown[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static int jkGuiMain_xboxReadyPrevLeft[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static int jkGuiMain_xboxReadyPrevRight[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];

static jkGuiElement jkGuiMain_xboxReadyElements[17] = {
    {ELEMENT_TEXT, 0, 5, L"Split Screen", 3, {0, 20, 640, 42}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_CUSTOM, 0, 2, 0, 0, {52, 90, 240, 110}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 0, 0, 0, {35, 100, 250, 128}, 0, 0, 0, 0, 0, jkGuiMain_xboxReadyListboxIdk, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxReadyNames[0], 3, {35, 135, 250, 42}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_CUSTOM, 1, 2, 0, 0, {348, 90, 240, 110}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 0, 0, 0, {355, 100, 250, 128}, 0, 0, 0, 0, 0, jkGuiMain_xboxReadyListboxIdk, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxReadyNames[1], 3, {355, 135, 250, 42}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_CUSTOM, 2, 2, 0, 0, {52, 240, 240, 110}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 0, 0, 0, {35, 280, 250, 128}, 0, 0, 0, 0, 0, jkGuiMain_xboxReadyListboxIdk, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxReadyNames[2], 3, {35, 315, 250, 42}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_CUSTOM, 3, 2, 0, 0, {348, 240, 240, 110}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 0, 0, 0, {355, 280, 250, 128}, 0, 0, 0, 0, 0, jkGuiMain_xboxReadyListboxIdk, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxReadyNames[3], 3, {355, 315, 250, 42}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, jkGuiMain_xboxReadyStatus, 3, {0, 64, 640, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, L"", 3, {360, 430, 180, 38}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, -1, 2, L"", 3, {100, 430, 180, 38}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0}
};

static jkGuiMenu jkGuiMain_xboxReadyMenu = {jkGuiMain_xboxReadyElements, -1, 0xFFFF, 0xFFFF, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

static const int jkGuiMain_xboxReadyLists[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {
    JKGUI_XBOX_READY_P1_LIST,
    JKGUI_XBOX_READY_P2_LIST,
    JKGUI_XBOX_READY_P3_LIST,
    JKGUI_XBOX_READY_P4_LIST
};

static const int jkGuiMain_xboxReadyNameElems[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {
    JKGUI_XBOX_READY_P1_NAME,
    JKGUI_XBOX_READY_P2_NAME,
    JKGUI_XBOX_READY_P3_NAME,
    JKGUI_XBOX_READY_P4_NAME
};

static const int jkGuiMain_xboxReadyPanelElems[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS] = {
    JKGUI_XBOX_READY_P1_LABEL,
    JKGUI_XBOX_READY_P2_LABEL,
    JKGUI_XBOX_READY_P3_LABEL,
    JKGUI_XBOX_READY_P4_LABEL
};

static int jkGuiMain_XboxReadySlotFromPanel(jkGuiElement *element)
{
    int i;
    for (i = 0; i < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; i++)
    {
        if (element == &jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyPanelElems[i]])
            return i;
    }
    return 0;
}

static void jkGuiMain_XboxReadyFreePortrait(int slot)
{
    if (slot < 0 || slot >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        return;
    if (jkGuiMain_xboxReadyPortraits[slot])
    {
        stdBitmap_Free(jkGuiMain_xboxReadyPortraits[slot]);
        jkGuiMain_xboxReadyPortraits[slot] = 0;
    }
    jkGuiMain_xboxReadyPortraitSelection[slot] = -1;
}

static uint8_t jkGuiMain_XboxNearestMenuColor(const rdColor24 *src)
{
    int best = 0;
    int bestDist = 0x7FFFFFFF;
    int i;

    if (!src)
        return 0;

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

static void jkGuiMain_XboxReadyBlitPortrait(stdVBuffer *dst, stdBitmap *bitmap, rdRect *dstRect)
{
    stdVBuffer *src;
    uint8_t remap[256];
    uint8_t *srcBase;
    uint8_t *dstBase;
    int x, y;

    if (!dst || !bitmap || !bitmap->mipSurfaces || !bitmap->mipSurfaces[0] || !bitmap->palette || !dstRect)
        return;

    src = bitmap->mipSurfaces[0];
    if (src->format.width <= 0 || src->format.height <= 0 || dstRect->width <= 0 || dstRect->height <= 0)
        return;

    for (x = 0; x < 256; x++)
        remap[x] = jkGuiMain_XboxNearestMenuColor(&((rdColor24*)bitmap->palette)[x]);

    stdDisplay_VBufferLock(src);
    stdDisplay_VBufferLock(dst);
    srcBase = (uint8_t*)src->surface_lock_alloc;
    dstBase = (uint8_t*)dst->surface_lock_alloc;
    if (!srcBase || !dstBase)
    {
        stdDisplay_VBufferUnlock(dst);
        stdDisplay_VBufferUnlock(src);
        return;
    }

    for (y = 0; y < dstRect->height; y++)
    {
        int sy = (y * src->format.height) / dstRect->height;
        int dy = dstRect->y + y;
        uint8_t *srcRow;
        uint8_t *dstRow;
        if (dy < 0 || dy >= dst->format.height || sy < 0 || sy >= src->format.height)
            continue;
        srcRow = srcBase + sy * src->format.width_in_bytes;
        dstRow = dstBase + dy * dst->format.width_in_bytes;
        for (x = 0; x < dstRect->width; x++)
        {
            int sx = (x * src->format.width) / dstRect->width;
            int dx = dstRect->x + x;
            if (dx < 0 || dx >= dst->format.width || sx < 0 || sx >= src->format.width)
                continue;
            dstRow[dx] = remap[srcRow[sx]];
        }
    }

    stdDisplay_VBufferUnlock(dst);
    stdDisplay_VBufferUnlock(src);
    XDBG("ProfilePortrait: ready blit remapped\n");
}

static stdBitmap *jkGuiMain_XboxReadyGetPortrait(int slot)
{
    jkGuiElement *list;
    wchar_t *name;

    if (slot < 0 || slot >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        return 0;
    if (!jkGuiMain_xboxReadyJoined[slot])
        return 0;

    list = &jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyLists[slot]];
    if (jkGuiMain_xboxReadyPortraitSelection[slot] == list->selectedTextEntry)
    {
        if (jkGuiMain_xboxReadyPortraits[slot]
            && jkGuiBuildMulti_XboxPortraitBitmapHasContent(jkGuiMain_xboxReadyPortraits[slot]))
            return jkGuiMain_xboxReadyPortraits[slot];
        if (jkGuiMain_xboxReadyPortraits[slot])
            jkGuiMain_XboxReadyFreePortrait(slot);
    }

    jkGuiMain_XboxReadyFreePortrait(slot);
    if (list->selectedTextEntry < 0 || list->selectedTextEntry >= jkGuiMain_xboxReadyNumChars)
        return 0;

    name = jkGuiRend_GetString(&jkGuiMain_xboxReadyCharacters, list->selectedTextEntry);
    if (!name)
        return 0;

    XDBG("ProfilePortrait: ready request\n");
    jkGuiMain_xboxReadyPortraits[slot] = jkGuiBuildMulti_XboxLoadPortraitCache(name);
    if (!jkGuiMain_xboxReadyPortraits[slot] && jkGuiBuildMulti_XboxEnsurePortraitCache(name))
        jkGuiMain_xboxReadyPortraits[slot] = jkGuiBuildMulti_XboxLoadPortraitCache(name);
    if (jkGuiMain_xboxReadyPortraits[slot])
        jkGuiMain_xboxReadyPortraitSelection[slot] = list->selectedTextEntry;
    else
        jkGuiMain_xboxReadyPortraitSelection[slot] = -1;
    return jkGuiMain_xboxReadyPortraits[slot];
}

static void jkGuiMain_XboxReadyBindList(int elemIdx, int selection, int numChars)
{
    jkGuiRend_SetClickableString(&jkGuiMain_xboxReadyElements[elemIdx], &jkGuiMain_xboxReadyCharacters);
    if (numChars <= 0)
        jkGuiMain_xboxReadyElements[elemIdx].selectedTextEntry = 0;
    else
        jkGuiMain_xboxReadyElements[elemIdx].selectedTextEntry = selection % numChars;
}

static void jkGuiMain_XboxReadyDrawPanel(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, BOOL redraw)
{
    int slot = jkGuiMain_XboxReadySlotFromPanel(element);
    int listIdx = jkGuiMain_xboxReadyLists[slot];
    jkGuiElement *list = &jkGuiMain_xboxReadyElements[listIdx];
    rdRect rect = element->rect;
    rdRect portrait;
    rdRect textRect;
    wchar_t line[64];
    wchar_t *name = 0;
    stdBitmap *portraitBm;

    (void)redraw;
    jkGuiRend_CopyVBuffer(menu, &rect);

    jk_snwprintf(line, 64, L"PLAYER %d", slot + 1);
    textRect.x = rect.x + 16;
    textRect.y = rect.y + 10;
    textRect.width = rect.width - 32;
    textRect.height = 22;
    stdFont_Draw3(vbuf, menu->fonts[2], textRect.y, &textRect, 0, line, 1);

    if (!jkGuiMain_xboxReadyJoined[slot])
    {
        textRect.y = rect.y + 50;
        stdFont_Draw3(vbuf, menu->fonts[2], textRect.y, &textRect, 0, L"PRESS A TO JOIN", 1);
        return;
    }

    if (list->selectedTextEntry >= 0 && list->selectedTextEntry < jkGuiMain_xboxReadyNumChars)
        name = jkGuiRend_GetString(&jkGuiMain_xboxReadyCharacters, list->selectedTextEntry);
    if (!name)
        name = L"Unknown";

    portrait.x = rect.x + 14;
    portrait.y = rect.y + 31;
    portrait.width = 74;
    portrait.height = 74;
    portraitBm = jkGuiMain_XboxReadyGetPortrait(slot);
    if (portraitBm && portraitBm->mipSurfaces && portraitBm->mipSurfaces[0]
        && portraitBm->mipSurfaces[0]->format.width == 74
        && portraitBm->mipSurfaces[0]->format.height == 74)
    {
        stdBitmap_EnsureData(portraitBm);
        if (jkGuiBuildMulti_XboxPortraitBitmapHasContent(portraitBm))
            jkGuiMain_XboxReadyBlitPortrait(vbuf, portraitBm, &portrait);
        else
            jkGuiMain_XboxReadyFreePortrait(slot);
    }

    textRect.x = rect.x + 98;
    textRect.y = rect.y + 40;
    textRect.width = rect.width - 108;
    textRect.height = 24;
    stdFont_Draw3(vbuf, menu->fonts[2], textRect.y, &textRect, 0, name, 1);
}

static int jkGuiMain_XboxReadyCount(void)
{
    int i;
    int count = 0;
    for (i = 0; i < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; i++)
        count += jkGuiMain_xboxReadyJoined[i] ? 1 : 0;
    return count;
}

static int jkGuiMain_XboxReadyReadEdge(int slot, int key, int *prev)
{
    int down = stdControl_XboxGetControllerKeyDown(slot, key) ? 1 : 0;
    int edge = down && !*prev;
    *prev = down;
    return edge;
}

static void jkGuiMain_XboxReadyPrimeEdges(void)
{
    int slot;

    stdControl_ReadControls();
    for (slot = 0; slot < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; slot++)
    {
        jkGuiMain_xboxReadyPrevA[slot] = stdControl_XboxGetControllerKeyDown(slot, KEY_JOY1_B1) ? 1 : 0;
        jkGuiMain_xboxReadyPrevB[slot] = stdControl_XboxGetControllerKeyDown(slot, KEY_JOY1_B2) ? 1 : 0;
        jkGuiMain_xboxReadyPrevStart[slot] = stdControl_XboxGetControllerKeyDown(slot, KEY_JOY1_B7) ? 1 : 0;
        jkGuiMain_xboxReadyPrevUp[slot] = stdControl_XboxGetControllerKeyDown(slot, KEY_JOY1_HUP) ? 1 : 0;
        jkGuiMain_xboxReadyPrevDown[slot] = stdControl_XboxGetControllerKeyDown(slot, KEY_JOY1_HDOWN) ? 1 : 0;
        jkGuiMain_xboxReadyPrevLeft[slot] = stdControl_XboxGetControllerKeyDown(slot, KEY_JOY1_HLEFT) ? 1 : 0;
        jkGuiMain_xboxReadyPrevRight[slot] = stdControl_XboxGetControllerKeyDown(slot, KEY_JOY1_HRIGHT) ? 1 : 0;
    }
}

static void jkGuiMain_XboxReadySetStatus(jkGuiMenu *menu)
{
    int count = jkGuiMain_XboxReadyCount();
    if (count < 1)
        jkGuiMain_xboxReadyStatus[0] = 0;
    else
        jk_snwprintf(jkGuiMain_xboxReadyStatus, 64, L"%d joined", count);
    if (menu)
        jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxReadyElements[JKGUI_XBOX_READY_STATUS], menu, 1);
}

static void jkGuiMain_XboxReadySetJoined(jkGuiMenu *menu, int slot, int joined)
{
    int listIdx;
    int nameIdx;
    int selected;
    wchar_t *name;

    if (slot < 0 || slot >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        return;
    listIdx = jkGuiMain_xboxReadyLists[slot];
    nameIdx = jkGuiMain_xboxReadyNameElems[slot];
    selected = jkGuiMain_xboxReadyElements[listIdx].selectedTextEntry;
    name = (selected >= 0 && selected < jkGuiMain_xboxReadyNumChars)
        ? jkGuiRend_GetString(&jkGuiMain_xboxReadyCharacters, selected)
        : 0;

    if (joined && name)
    {
        _wcsncpy(jkGuiMain_xboxReadyNames[slot], name, 31);
        jkGuiMain_xboxReadyNames[slot][31] = 0;
        jkGuiMain_xboxReadyJoined[slot] = 1;
        jkGuiMain_xboxReadyElements[listIdx].bIsVisible = 0;
        jkGuiMain_xboxReadyElements[nameIdx].bIsVisible = 0;
        if (menu && menu->focusedElement == &jkGuiMain_xboxReadyElements[listIdx])
            menu->focusedElement = 0;
    }
    else
    {
        jkGuiMain_xboxReadyJoined[slot] = 0;
        jkGuiMain_xboxReadyElements[listIdx].bIsVisible = 0;
        jkGuiMain_xboxReadyElements[nameIdx].bIsVisible = 0;
    }

    if (menu)
    {
        jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyPanelElems[slot]], menu, 1);
        jkGuiMain_XboxReadySetStatus(menu);
    }
}

static void jkGuiMain_XboxReadyMoveSelection(jkGuiMenu *menu, int slot, int delta)
{
    jkGuiElement *list;
    int selected;

    if (slot < 0 || slot >= XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS || jkGuiMain_xboxReadyNumChars <= 0)
        return;
    list = &jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyLists[slot]];
    selected = list->selectedTextEntry + delta;
    while (selected < 0)
        selected += jkGuiMain_xboxReadyNumChars;
    selected %= jkGuiMain_xboxReadyNumChars;
    list->selectedTextEntry = selected;
    if (list->texInfo.maxTextEntries > 0)
    {
        if (list->selectedTextEntry < list->texInfo.textScrollY)
            list->texInfo.textScrollY = list->selectedTextEntry;
        if (list->selectedTextEntry >= list->texInfo.textScrollY + list->texInfo.maxTextEntries)
            list->texInfo.textScrollY = list->selectedTextEntry - list->texInfo.maxTextEntries + 1;
    }
    if (jkGuiMain_xboxReadyJoined[slot])
    {
        wchar_t *name = jkGuiRend_GetString(&jkGuiMain_xboxReadyCharacters, selected);
        if (name)
        {
            _wcsncpy(jkGuiMain_xboxReadyNames[slot], name, 31);
            jkGuiMain_xboxReadyNames[slot][31] = 0;
        }
    }
    jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyPanelElems[slot]], menu, 1);
}

static void jkGuiMain_XboxReadyTick(jkGuiMenu *menu)
{
    int slot;
    int mask;

    mask = stdControl_XboxGetConnectedMask();
    for (slot = 0; slot < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; slot++)
    {
        if (!(mask & (1 << slot)))
            continue;

        if (jkGuiMain_XboxReadyReadEdge(slot, KEY_JOY1_B7, &jkGuiMain_xboxReadyPrevStart[slot]) && jkGuiMain_XboxReadyCount() > 0)
        {
            jkGuiMain_xboxReadyStartRequested = 1;
            menu->lastClicked = 20;
            continue;
        }

        if (!jkGuiMain_xboxReadyJoined[slot])
        {
            if (jkGuiMain_XboxReadyReadEdge(slot, KEY_JOY1_B1, &jkGuiMain_xboxReadyPrevA[slot]))
                jkGuiMain_XboxReadySetJoined(menu, slot, 1);
            if (slot == 0 && jkGuiMain_XboxReadyReadEdge(slot, KEY_JOY1_B2, &jkGuiMain_xboxReadyPrevB[slot]))
                menu->lastClicked = -1;
            continue;
        }

        if (jkGuiMain_XboxReadyReadEdge(slot, KEY_JOY1_B2, &jkGuiMain_xboxReadyPrevB[slot]))
        {
            if (slot == 0)
                menu->lastClicked = -1;
            else
                jkGuiMain_XboxReadySetJoined(menu, slot, 0);
            continue;
        }
        if (jkGuiMain_XboxReadyReadEdge(slot, KEY_JOY1_HUP, &jkGuiMain_xboxReadyPrevUp[slot]))
            jkGuiMain_XboxReadyMoveSelection(menu, slot, -1);
        if (jkGuiMain_XboxReadyReadEdge(slot, KEY_JOY1_HDOWN, &jkGuiMain_xboxReadyPrevDown[slot]))
            jkGuiMain_XboxReadyMoveSelection(menu, slot, 1);
        if (jkGuiMain_XboxReadyReadEdge(slot, KEY_JOY1_HLEFT, &jkGuiMain_xboxReadyPrevLeft[slot]))
            jkGuiMain_XboxReadyMoveSelection(menu, slot, -1);
        if (jkGuiMain_XboxReadyReadEdge(slot, KEY_JOY1_HRIGHT, &jkGuiMain_xboxReadyPrevRight[slot]))
            jkGuiMain_XboxReadyMoveSelection(menu, slot, 1);
    }
}

static int jkGuiMain_XboxShowSplitReady(void)
{
    int i;
    int result;
    int numChars;
    int menuModePushed = 0;

    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_ESC]->palette);
    menuModePushed = 1;
    _memset(&jkGuiMain_xboxReadyCharacters, 0, sizeof(jkGuiMain_xboxReadyCharacters));
    if (!jkGuiRend_DarrayNewStr(&jkGuiMain_xboxReadyCharacters, 32, 1))
    {
        if (menuModePushed)
            jkGui_SetModeGame();
        return -1;
    }

    numChars = jkGuiBuildMulti_Show2(&jkGuiMain_xboxReadyCharacters, &jkGuiMain_xboxReadyElements[JKGUI_XBOX_READY_P1_LIST], 0, 8, 0);
    if (numChars <= 0)
    {
        jkGuiRend_DarrayFree(&jkGuiMain_xboxReadyCharacters);
        jkGuiDialog_ErrorDialog(L"Split Screen", L"No Xbox multiplayer character profiles were found.");
        if (menuModePushed)
            jkGui_SetModeGame();
        return -1;
    }

    jkGuiMain_xboxReadyNumChars = numChars;
    for (i = 0; i < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; i++)
    {
        jkGuiMain_XboxReadyBindList(jkGuiMain_xboxReadyLists[i], i, numChars);
        jkGuiMain_xboxReadyJoined[i] = 0;
        jkGuiMain_xboxReadyNames[i][0] = 0;
        jkGuiMain_XboxReadyFreePortrait(i);
        jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyPanelElems[i]].drawFuncOverride = jkGuiMain_XboxReadyDrawPanel;
        jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyLists[i]].bIsVisible = 0;
        jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyNameElems[i]].bIsVisible = 0;
    }
    jkGuiMain_XboxReadySetStatus(0);
    jkGuiMain_xboxReadyStartRequested = 0;
    jkGuiMain_XboxReadyPrimeEdges();

    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiMain_xboxReadyMenu, 0);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMain_xboxReadyMenu, &jkGuiMain_xboxReadyElements[JKGUI_XBOX_READY_CANCEL]);
    jkGuiMain_xboxReadyMenu.idkFunc = jkGuiMain_XboxReadyTick;
    jkGuiRend_xboxSuppressControllerConfirm = 1;
    do
    {
        result = jkGuiRend_DisplayAndReturnClicked(&jkGuiMain_xboxReadyMenu);
        if (result == 20 && !jkGuiMain_xboxReadyStartRequested)
        {
            XDBG("SplitReady: ignoring stray GUI Start result without Start button edge\n");
            result = 0;
        }
        else if (result == 20 && jkGuiMain_XboxReadyCount() < 1)
        {
            XDBG("SplitReady: ignoring Start edge with no joined players\n");
            result = 0;
        }
    } while (result == 0);
    jkGuiRend_xboxSuppressControllerConfirm = 0;

    if (result == 20)
    {
        int outSlot = 0;
        int readyCount = jkGuiMain_XboxReadyCount();
        xboxSplitScreen_SetRequestedLocalPlayerCount(readyCount);
        for (i = 0; i < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; i++)
        {
            int selected = jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyLists[i]].selectedTextEntry;
            wchar_t *name = 0;
            char nameA[32];

            if (!jkGuiMain_xboxReadyJoined[i])
                continue;
            if (selected >= 0 && selected < numChars)
                name = jkGuiRend_GetString(&jkGuiMain_xboxReadyCharacters, selected);
            xboxSplitScreen_SetPendingController(outSlot, i);
            xboxSplitScreen_SetPendingMpc(outSlot, name);
            if (name)
            {
                stdString_WcharToChar(nameA, name, 31);
                nameA[31] = 0;
            }
            else
            {
                _strcpy(nameA, "(null)");
            }
            XDBGF("SplitReady: controller=%d slot=%d selected=%d name='%s'\n", i, outSlot, selected, nameA);
            outSlot++;
        }
        while (outSlot < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS)
        {
            xboxSplitScreen_SetPendingMpc(outSlot, 0);
            outSlot++;
        }
    }

    jkGuiRend_DarrayFree(&jkGuiMain_xboxReadyCharacters);
    for (i = 0; i < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; i++)
        jkGuiMain_XboxReadyFreePortrait(i);
    if (menuModePushed)
        jkGui_SetModeGame();
    return result == 20 ? 1 : -1;
}

static int jkGuiMain_XboxStartLocalMultiplayerTest(void)
{
    int result;

    XDBG("MPLoadTrace: ready screen enter\n");
    if (jkGuiMain_XboxShowSplitReady() != 1)
    {
        XDBG("MPLoadTrace: ready screen canceled\n");
        return -1;
    }

    XDBGF("MPLoadTrace: ready accepted players=%d\n", xboxSplitScreen_GetRequestedLocalPlayerCount());
    xboxSplitScreen_Enable();
    XDBG("MPLoadTrace: split screen enabled; calling jkMain_loadFile2 JK1MP/m10.jkl\n");
    result = jkMain_loadFile2("JK1MP", "m10.jkl") ? 1 : -1;
    XDBGF("MPLoadTrace: jkMain_loadFile2 returned %d\n", result);
    if (result != 1)
    {
        XDBG("MPLoadTrace: disabling split screen after load setup failure\n");
        xboxSplitScreen_Disable();
    }
    return result;
}

static void jkGuiMain_XboxSystemLinkResetUi(void)
{
    int i;

    jkGuiMain_xboxSystemLinkStatus[0] = 0;
    jkGuiMain_xboxSystemLinkPeerCount[0] = 0;
    jkGuiMain_xboxSystemLinkHint[0] = 0;
    for (i = 0; i < XBOX_SYSTEMLINK_PROBE_MAX_PEERS; i++)
    {
        jkGuiMain_xboxSystemLinkPeers[i][0] = 0;
        jkGuiMain_xboxSystemLinkLastPeerPackets[i] = 0xFFFFFFFF;
        jkGuiMain_xboxSystemLinkElements[JKGUI_XBOX_XSL_FIRST_PEER + i].bIsVisible = 0;
    }

    jkGuiMain_xboxSystemLinkLastStarted = -1;
    jkGuiMain_xboxSystemLinkLastPort = -1;
    jkGuiMain_xboxSystemLinkLastError = -1;
    jkGuiMain_xboxSystemLinkLastId = 0;
    jkGuiMain_xboxSystemLinkLastSent = 0xFFFFFFFF;
}

static int jkGuiMain_XboxSystemLinkFormatUi(void)
{
    XboxSystemLinkProbeStatus status;
    int dirty;
    int i;

    xboxSystemLinkProbe_GetStatus(&status);
    dirty = 0;

    if (status.started)
        jk_snwprintf(jkGuiMain_xboxSystemLinkStatus, 96, L"LOCAL ID  %08X    PORT %d", status.localId, status.localPort);
    else
        jk_snwprintf(jkGuiMain_xboxSystemLinkStatus, 96, L"NOT STARTED    ERROR %d", status.lastError);

    jk_snwprintf(jkGuiMain_xboxSystemLinkPeerCount, 64, L"PEERS FOUND  %d", status.peerCount);

    if (status.peerCount == 0)
        jk_snwprintf(jkGuiMain_xboxSystemLinkHint, 96, L"OPEN THIS SCREEN ON BOTH INSTANCES");
    else
        jkGuiMain_xboxSystemLinkHint[0] = 0;

    if (status.started != jkGuiMain_xboxSystemLinkLastStarted
        || status.localPort != jkGuiMain_xboxSystemLinkLastPort
        || status.localId != jkGuiMain_xboxSystemLinkLastId
        || status.lastError != jkGuiMain_xboxSystemLinkLastError
        || status.sent != jkGuiMain_xboxSystemLinkLastSent)
    {
        dirty = 1;
    }

    jkGuiMain_xboxSystemLinkLastStarted = status.started;
    jkGuiMain_xboxSystemLinkLastPort = status.localPort;
    jkGuiMain_xboxSystemLinkLastId = status.localId;
    jkGuiMain_xboxSystemLinkLastError = status.lastError;
    jkGuiMain_xboxSystemLinkLastSent = status.sent;

    for (i = 0; i < XBOX_SYSTEMLINK_PROBE_MAX_PEERS; i++)
    {
        int elemIdx = JKGUI_XBOX_XSL_FIRST_PEER + i;

        if (i < status.peerCount)
        {
            char addr[32];
            xboxSystemLinkProbe_FormatAddress(status.peers[i].address, addr, sizeof(addr));
            jk_snwprintf(jkGuiMain_xboxSystemLinkPeers[i], 96, L"%08X    %S:%d    PACKETS %lu",
                         status.peers[i].id,
                         addr,
                         status.peers[i].port,
                         status.peers[i].packets);
            if (!jkGuiMain_xboxSystemLinkElements[elemIdx].bIsVisible
                || jkGuiMain_xboxSystemLinkLastPeerPackets[i] != status.peers[i].packets)
            {
                dirty = 1;
            }
            jkGuiMain_xboxSystemLinkElements[elemIdx].bIsVisible = 1;
            jkGuiMain_xboxSystemLinkLastPeerPackets[i] = status.peers[i].packets;
        }
        else
        {
            if (jkGuiMain_xboxSystemLinkElements[elemIdx].bIsVisible)
                dirty = 1;
            jkGuiMain_xboxSystemLinkElements[elemIdx].bIsVisible = 0;
            jkGuiMain_xboxSystemLinkPeers[i][0] = 0;
            jkGuiMain_xboxSystemLinkLastPeerPackets[i] = 0xFFFFFFFF;
        }
    }

    return dirty;
}

static void jkGuiMain_XboxSystemLinkTick(jkGuiMenu *menu)
{
    int i;

    xboxSystemLinkProbe_Tick();
    if (!jkGuiMain_XboxSystemLinkFormatUi())
        return;

    jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxSystemLinkElements[JKGUI_XBOX_XSL_STATUS], menu, 1);
    jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxSystemLinkElements[JKGUI_XBOX_XSL_PEER_COUNT], menu, 1);
    for (i = 0; i < XBOX_SYSTEMLINK_PROBE_MAX_PEERS; i++)
        jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxSystemLinkElements[JKGUI_XBOX_XSL_FIRST_PEER + i], menu, 1);
    jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxSystemLinkElements[JKGUI_XBOX_XSL_HINT], menu, 1);
}

static void jkGuiMain_XboxShowSystemLinkProbe(void)
{
    XDBG("XSL probe screen enter\n");
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]->palette);
    jkGuiMain_XboxSystemLinkResetUi();
    jkGuiMain_XboxSystemLinkFormatUi();
    jkGuiMain_xboxSystemLinkMenu.idkFunc = jkGuiMain_XboxSystemLinkTick;
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiMain_xboxSystemLinkMenu, &jkGuiMain_xboxSystemLinkElements[JKGUI_XBOX_XSL_BACK]);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMain_xboxSystemLinkMenu, &jkGuiMain_xboxSystemLinkElements[JKGUI_XBOX_XSL_BACK]);
    xboxSystemLinkProbe_Start();
    jkGuiRend_DisplayAndReturnClicked(&jkGuiMain_xboxSystemLinkMenu);
    xboxSystemLinkProbe_Stop();
    jkGuiMain_xboxSystemLinkMenu.idkFunc = 0;
    XDBG("XSL probe screen leave\n");
}

static int jkGuiMain_XboxShowMultiplayer(void)
{
    int result;

    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]->palette);

    do
    {
        jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiMain_xboxMultiplayerMenu, &jkGuiMain_xboxMultiplayerElements[1]);
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMain_xboxMultiplayerMenu, &jkGuiMain_xboxMultiplayerElements[4]);
        result = jkGuiRend_DisplayAndReturnClicked(&jkGuiMain_xboxMultiplayerMenu);

        if (result == 20)
            return jkGuiMain_XboxStartLocalMultiplayerTest();

        if (result == 21)
        {
            jkGuiBuildMulti_Show();
            stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
            jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]->palette);
            result = -2;
        }
        else if (result == 22)
        {
            jkGuiMain_XboxShowSystemLinkProbe();
            stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
            jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]->palette);
            result = -2;
        }
    } while (result == -2);

    return -1;
}
#endif

// MOTS altered
void jkGuiMain_Show()
{
    int v1; // esi
    wchar_t *v2; // eax
    wchar_t *v4; // [esp-4h] [ebp-Ch]

#ifdef JKGUI_SMOL_SCREEN
    for (int i = 0; i < 11; i++) {
        jkGuiMain_elements[i].rect = jkGuiMain_elements[i].rectOrig;
        jkGuiMain_elements[i].bIsSmolDirty = 1; 
    }
#endif

    if (!Main_bMotsCompat) {
        jkGuiMain_elements[0].rect.y = 160;
        jkGuiMain_elements[1].rect.y = 220;
        jkGuiMain_elements[2].rect.y = 280;
        jkGuiMain_elements[3].rect.y = 380;
        jkGuiMain_elements[4].rect.y = 380;
        jkGuiMain_elements[5].rect.y = 380;
#ifdef QOL_IMPROVEMENTS
        jkGuiMain_elements[6].rect.y = 430;
        jkGuiMain_elements[7].rect.y = 430;
#else
        jkGuiMain_elements[6].rect.y = 420;
#endif
    }
    else {
        jkGuiMain_elements[0].rect.y = 160+25;
        jkGuiMain_elements[1].rect.y = 220+25;
        jkGuiMain_elements[2].rect.y = 280+25;
        jkGuiMain_elements[3].rect.y = 380+5;
        jkGuiMain_elements[4].rect.y = 380+5;
        jkGuiMain_elements[5].rect.y = 380+5;
#ifdef QOL_IMPROVEMENTS
        jkGuiMain_elements[6].rect.y = 430;
        jkGuiMain_elements[7].rect.y = 430;
#else
        jkGuiMain_elements[6].rect.y = 420+5;
#endif
    }

#ifdef JKGUI_SMOL_SCREEN
    for (int i = 0; i < 11; i++) {
        jkGuiMain_elements[i].rect.y -= 60;
    }
    
    jkGuiMain_elements[0].rect.y += 70; // Singleplayer
    jkGuiMain_elements[7].rect.height += 15; // Expansions & Mods
    jkGuiMain_elements[8].rect.height += 10; // Version
    jkGuiMain_elements[9].rect.height += 10; // git hash
    jkGuiMain_elements[8].rect.y += 20;
    jkGuiMain_elements[9].rect.y += 40; // git hash
    jkGuiMain_elements[9].rect.x -= 15;
    jkGui_SmolScreenFixup(&jkGuiMain_menu, 0);
#endif

    // Added: OpenJKDF2 version
    jkGuiMain_elements[8].wstr = openjkdf2_waReleaseVersion;
    jkGuiMain_elements[9].wstr = openjkdf2_waReleaseCommitShort;

    // Added
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]);

    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]->palette);

#ifdef TARGET_XBOX
    {
        char smokeLevel[128];
        if (jkGuiMain_XboxReadSmokeAutostartLevel(smokeLevel, sizeof(smokeLevel)))
        {
            XDBGF("jkGuiMain_Show: smoke autostart level '%s'\n", smokeLevel);
            if (jkMain_LoadLevelSingleplayer("JK1", smokeLevel))
            {
                XDBG("jkGuiMain_Show: smoke autostart scheduled\n");
                jkGui_SetModeGame();
                return;
            }
            XDBG("jkGuiMain_Show: smoke autostart failed, falling through to menu\n");
        }
    }
#endif

    if ( !jkGuiMain_bIdk || (jkGuiMain_bIdk = 0, jkGuiPlayer_ShowNewPlayer(1), !stdComm_dword_8321F8)
#if !defined(TARGET_NO_MULTIPLAYER_MENUS) && !defined(TARGET_XBOX)
        || jkGuiMultiplayer_Show2() != 1 
#endif
        )
    {
        if (Main_bMotsCompat) {
            jkGuiMain_elements[4].bIsVisible = Main_bDevMode; // MOTS added
        }
        else {
            jkGuiMain_elements[4].bIsVisible = 1;
        }

        do
        {
            if (g_should_exit) return; // Added

#ifndef TARGET_XBOX
            jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMain_menu, &jkGuiMain_elements[2]);
#endif
            v1 = jkGuiRend_DisplayAndReturnClicked(&jkGuiMain_menu);
            switch ( v1 )
            {
                case 10:
                    v1 = jkGuiSingleplayer_Show();
                    break;
#if !defined(TARGET_NO_MULTIPLAYER_MENUS) || defined(TARGET_XBOX)
                case 11:
#ifdef TARGET_XBOX
                    v1 = jkGuiMain_XboxShowMultiplayer();
#else
                    v1 = jkGuiMultiplayer_Show();
#endif
                    break;
#endif
                case 12:
                    v4 = jkStrings_GetUniStringWithFallback("GUI_QUITCONFIRM_Q");
                    v2 = jkStrings_GetUniStringWithFallback("GUI_QUITCONFIRM");
                    if ( !jkGuiDialog_YesNoDialog(v2, v4) )
                        goto LABEL_12;

                    // TODO proper shutdown?
#ifdef WIN32_BLOBS
                    jk_PostMessageA(stdGdi_GetHwnd(), 16, 0, 0);
#else
                    sithCvar_SaveGlobals();
                    jkPlayer_WriteConf(jkPlayer_playerShortName); // Added
                    g_should_exit = 1;
                    //exit(0);
                    return;
#endif
                    break;
                case 13:
                    jkGuiSetup_Show();
                    v1 = -1;
                    break;
                case 14:
                    jkGuiPlayer_ShowNewPlayer(0);
LABEL_12:
                    v1 = -1;
                    break;
                case 15:
                    jkMain_SwitchTo12();
                    break;
                case 16:
                    jkCredits_cdOverride = 1; // Added: Simulate disk 1 in menu for jkCredits
                    jkMain_SwitchTo13();
                    break;
#ifdef QOL_IMPROVEMENTS
                case 17:
                    jkGuiMods_Show();
                    v1 = -1;
                    break;
#endif
                default:
                    break;
            }
        }
        while ( v1 == -1 );
    }
    jkGui_SetModeGame();
}

void jkGuiMain_ShowCutscenes()
{
    char *v0; // ebx
    char *v1; // ebp
    char *v2; // edx
    wchar_t *v3; // eax
    int v4; // eax
    const char *v5; // eax
    const char *v6; // eax
    int v7; // esi
    void *i; // eax
    int v9; // [esp+10h] [ebp-15Ch]
    Darray darray; // [esp+14h] [ebp-158h] BYREF
    char v11[64]; // [esp+2Ch] [ebp-140h] BYREF
    char v12[256]; // [esp+6Ch] [ebp-100h] BYREF

    if ( !jkGuiCutscenes_initted )
        jkGui_InitMenu(&jkGuiMain_cutscenesMenu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
    jkGuiCutscenes_initted = 1;

    // Added
    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]);
    
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]->palette);
    jkGuiRend_DarrayNewStr(&darray, 32, 1);
    if ( !jkPlayer_ReadConf(jkPlayer_playerShortName) )
    {
        stdString_WcharToChar(v11, jkPlayer_playerShortName, 31);
        v11[31] = 0;
        Windows_ErrorMsgboxWide("ERR_CANNOT_SET_PLAYER %s", v11);
    }
    
    jkGuiMain_PopulateCutscenes(&darray, &jkGuiMain_cutscenesElements[1]);
    do
    {
        while ( 1 )
        {
            jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiMain_cutscenesMenu, &jkGuiMain_cutscenesElements[2]);
            jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMain_cutscenesMenu, &jkGuiMain_cutscenesElements[3]);
            v4 = jkGuiRend_DisplayAndReturnClicked(&jkGuiMain_cutscenesMenu);
            if ( v4 != 1 )
                break;

            // Added: Moved these up
            v5 = (const char *)jkGuiRend_GetId(&darray, jkGuiMain_cutscenesElements[1].selectedTextEntry);
            snprintf(v12, 256, "video%c%s", '\\', v5); // Added: sprintf -> snprintf
            if ( util_FileExists(v12) || jkRes_LoadCD(jkPlayer_aCutsceneVal[jkGuiMain_cutscenesElements[1].selectedTextEntry]) ) // Added: Don't need a CD switch if it exists.
            {
                // Added: move up
                //v5 = (const char *)jkGuiRend_GetId(&darray, jkGuiMain_cutscenesElements[1].selectedTextEntry);
                //snprintf(v12, 256, "video%c%s", '\\', v5); // Added: sprintf -> snprintf
                if ( util_FileExists(v12) )
                {
                    jkMain_SwitchTo4(v12);
                    goto LABEL_17;
                }
                v6 = (const char *)jkGuiRend_GetId(&darray, jkGuiMain_cutscenesElements[1].selectedTextEntry);
                stdPrintf(pHS->errorPrint, ".\\Gui\\jkGUIMain.c", 297, "Cannot find cutscene '%s'.\n", v6);
            }
        }
    }
    while ( v4 != -1 );
    jkMain_MenuReturn();
LABEL_17:
    v7 = 0;
    for ( i = (void *)jkGuiRend_GetId(&darray, 0); i; i = (void *)jkGuiRend_GetId(&darray, v7) )
    {
        pHS->free(i);
        ++v7;
    }
    jkGui_SetModeGame();
}

void jkGuiMain_Startup()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added
    
    jkGui_InitMenu(&jkGuiMain_menu, jkGui_stdBitmaps[JKGUI_BM_BK_MAIN]);
#ifdef TARGET_XBOX
    jkGui_InitMenu(&jkGuiMain_xboxMultiplayerMenu, jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
    jkGui_InitMenu(&jkGuiMain_xboxReadyMenu, jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
    jkGui_InitMenu(&jkGuiMain_xboxSystemLinkMenu, jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
#endif

    // Added: clean reset
    jkGuiMain_bIdk = 1;
}

void jkGuiMain_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added

    // Added: clean reset
    jkGuiCutscenes_initted = 0;
}

void jkGuiMain_PopulateCutscenes(Darray *list, jkGuiElement *element)
{
    char* v2;
    char *v3; // ebx
    wchar_t *v5; // eax
    int v6; // [esp+4h] [ebp-44h]
    char key[64]; // [esp+8h] [ebp-40h] BYREF

    v2 = jkPlayer_cutscenePath;
    for (v6 = 0; v6 < jkPlayer_setNumCutscenes; v6++)
    {
        v3 = _strcpy((char *)pHS->alloc(_strlen(v2) + 1), v2);
        stdFnames_CopyShortName(key, 64, v3); // TODO aaaaaaa ??? disassembly was wrong?
        jkGuiTitle_sub_4189A0(key);
        v5 = jkStrings_GetUniString(key);
        jkGuiRend_DarrayReallocStr(list, v5, (intptr_t)v3);
        v2 += 32;
    }
    jkGuiRend_AddStringEntry(list, 0, 0);
    jkGuiRend_SetClickableString(element, list);
    element->selectedTextEntry = 0;
}

void jkGuiMain_FreeCutscenes(Darray *a1)
{
    int v1; // esi
    void *i; // eax

    v1 = 0;
    for ( i = (void *)jkGuiRend_GetId(a1, 0); i; i = (void *)jkGuiRend_GetId(a1, v1) )
    {
        pHS->free(i);
        ++v1;
    }
}
