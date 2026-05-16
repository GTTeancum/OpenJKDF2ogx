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
#endif

// Added
extern int jkCredits_cdOverride;
static wchar_t jkGuiMain_versionBuffer[64];

static int jkGuiMain_bIdk = 1;
static int jkGuiCutscenes_initted;

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
static jkGuiElement jkGuiMain_xboxMultiplayerElements[7] = {
    {ELEMENT_TEXT, 0, 5, L"Multiplayer", 3, {0, 50, 640, 60}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 20, 5, L"Split Screen", 3, {0, 150, 640, 50}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 21, 5, L"Character", 3, {0, 210, 640, 50}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 22, 5, L"System Link", 3, {0, 270, 640, 50}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 23, 5, L"Combo", 3, {0, 330, 640, 50}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, {230, 410, 180, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0}
};

static jkGuiMenu jkGuiMain_xboxMultiplayerMenu = {jkGuiMain_xboxMultiplayerElements, -1, 0xFFFF, 0xFFFF, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

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
static int jkGuiMain_xboxReadyLocked[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS];
static wchar_t jkGuiMain_xboxReadyNames[XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS][32];
static wchar_t jkGuiMain_xboxReadyStatus[64];

static jkGuiElement jkGuiMain_xboxReadyElements[17] = {
    {ELEMENT_TEXT, 0, 5, L"Split Screen Ready", 3, {0, 18, 640, 42}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, L"Player 1", 3, {35, 66, 250, 25}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 0, 0, 0, {35, 100, 250, 128}, 1, 0, 0, 0, 0, jkGuiMain_xboxReadyListboxIdk, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxReadyNames[0], 3, {35, 135, 250, 42}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, L"Player 2", 3, {355, 66, 250, 25}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 0, 0, 0, {355, 100, 250, 128}, 1, 0, 0, 0, 0, jkGuiMain_xboxReadyListboxIdk, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxReadyNames[1], 3, {355, 135, 250, 42}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, L"Player 3", 3, {35, 246, 250, 25}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 0, 0, 0, {35, 280, 250, 128}, 1, 0, 0, 0, 0, jkGuiMain_xboxReadyListboxIdk, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxReadyNames[2], 3, {35, 315, 250, 42}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 2, L"Player 4", 3, {355, 246, 250, 25}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_LISTBOX, 1, 0, 0, 0, {355, 280, 250, 128}, 1, 0, 0, 0, 0, jkGuiMain_xboxReadyListboxIdk, {0}, 0},
    {ELEMENT_TEXT, 0, 2, jkGuiMain_xboxReadyNames[3], 3, {355, 315, 250, 42}, 0, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, jkGuiMain_xboxReadyStatus, 3, {150, 414, 340, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 20, 2, L"Start", 3, {360, 430, 180, 38}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, {100, 430, 180, 38}, 1, 0, 0, 0, 0, 0, {0}, 0},
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

static void jkGuiMain_XboxReadyBindList(int elemIdx, int selection, int numChars)
{
    jkGuiRend_SetClickableString(&jkGuiMain_xboxReadyElements[elemIdx], &jkGuiMain_xboxReadyCharacters);
    if (numChars <= 0)
        jkGuiMain_xboxReadyElements[elemIdx].selectedTextEntry = 0;
    else
        jkGuiMain_xboxReadyElements[elemIdx].selectedTextEntry = selection % numChars;
}

static int jkGuiMain_XboxReadyCount(void)
{
    int i;
    int count = 0;
    for (i = 0; i < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; i++)
        count += jkGuiMain_xboxReadyLocked[i] ? 1 : 0;
    return count;
}

static void jkGuiMain_XboxReadySetStatus(jkGuiMenu *menu)
{
    int count = jkGuiMain_XboxReadyCount();
    if (count < 2)
        jk_snwprintf(jkGuiMain_xboxReadyStatus, 64, L"%d ready - need 2", count);
    else
        jk_snwprintf(jkGuiMain_xboxReadyStatus, 64, L"%d ready", count);
    if (menu)
        jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxReadyElements[JKGUI_XBOX_READY_STATUS], menu, 1);
}

static void jkGuiMain_XboxReadySetLocked(jkGuiMenu *menu, int slot, int locked)
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

    if (locked && name)
    {
        _wcsncpy(jkGuiMain_xboxReadyNames[slot], name, 31);
        jkGuiMain_xboxReadyNames[slot][31] = 0;
        jkGuiMain_xboxReadyLocked[slot] = 1;
        jkGuiMain_xboxReadyElements[listIdx].bIsVisible = 0;
        jkGuiMain_xboxReadyElements[nameIdx].bIsVisible = 1;
        if (menu && menu->focusedElement == &jkGuiMain_xboxReadyElements[listIdx])
            menu->focusedElement = 0;
    }
    else
    {
        jkGuiMain_xboxReadyLocked[slot] = 0;
        jkGuiMain_xboxReadyElements[listIdx].bIsVisible = 1;
        jkGuiMain_xboxReadyElements[nameIdx].bIsVisible = 0;
    }

    if (menu)
    {
        jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxReadyElements[listIdx], menu, 1);
        jkGuiRend_UpdateAndDrawClickable(&jkGuiMain_xboxReadyElements[nameIdx], menu, 1);
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
    jkGuiRend_UpdateAndDrawClickable(list, menu, 1);
}

static void jkGuiMain_XboxReadyTick(jkGuiMenu *menu)
{
    int slot;
    int mask;

    stdControl_ReadControls();
    mask = stdControl_XboxGetConnectedMask();
    for (slot = 0; slot < XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS; slot++)
    {
        if (!(mask & (1 << slot)))
            continue;
        if (jkGuiMain_xboxReadyLocked[slot])
        {
            if (stdControl_XboxGetControllerKeyPress(slot, KEY_JOY1_B2))
                jkGuiMain_XboxReadySetLocked(menu, slot, 0);
            continue;
        }

        if (stdControl_XboxGetControllerKeyPress(slot, KEY_JOY1_HUP))
            jkGuiMain_XboxReadyMoveSelection(menu, slot, -1);
        if (stdControl_XboxGetControllerKeyPress(slot, KEY_JOY1_HDOWN))
            jkGuiMain_XboxReadyMoveSelection(menu, slot, 1);
        if (stdControl_XboxGetControllerKeyPress(slot, KEY_JOY1_B1))
            jkGuiMain_XboxReadySetLocked(menu, slot, 1);
    }
}

static int jkGuiMain_XboxShowSplitReady(void)
{
    int i;
    int result;
    int numChars;
    int menuModePushed = 0;

    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]->palette);
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
        jkGuiMain_xboxReadyLocked[i] = 0;
        jkGuiMain_xboxReadyNames[i][0] = 0;
        jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyLists[i]].bIsVisible = 1;
        jkGuiMain_xboxReadyElements[jkGuiMain_xboxReadyNameElems[i]].bIsVisible = 0;
    }
    jkGuiMain_XboxReadySetStatus(0);

    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiMain_xboxReadyMenu, &jkGuiMain_xboxReadyElements[JKGUI_XBOX_READY_START]);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMain_xboxReadyMenu, &jkGuiMain_xboxReadyElements[JKGUI_XBOX_READY_CANCEL]);
    jkGuiMain_xboxReadyMenu.idkFunc = jkGuiMain_XboxReadyTick;
    do
    {
        result = jkGuiRend_DisplayAndReturnClicked(&jkGuiMain_xboxReadyMenu);
        if (result == 20 && jkGuiMain_XboxReadyCount() < 2)
        {
            jkGuiDialog_ErrorDialog(L"Split Screen", L"At least two players must ready up.");
            result = 0;
        }
    } while (result == 0);

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

            if (!jkGuiMain_xboxReadyLocked[i])
                continue;
            if (selected >= 0 && selected < numChars)
                name = jkGuiRend_GetString(&jkGuiMain_xboxReadyCharacters, selected);
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
    if (menuModePushed)
        jkGui_SetModeGame();
    return result == 20 ? 1 : -1;
}

static int jkGuiMain_XboxStartLocalMultiplayerTest(void)
{
    int result;

    if (jkGuiMain_XboxShowSplitReady() != 1)
        return -1;

    xboxSplitScreen_Enable();
    result = jkMain_loadFile2("JK1MP", "m10.jkl") ? 1 : -1;
    if (result != 1)
        xboxSplitScreen_Disable();
    return result;
}

static int jkGuiMain_XboxShowMultiplayer(void)
{
    int result;

    stdBitmap_EnsureData(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
    jkGui_SetModeMenu(jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]->palette);

    do
    {
        jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiMain_xboxMultiplayerMenu, &jkGuiMain_xboxMultiplayerElements[1]);
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiMain_xboxMultiplayerMenu, &jkGuiMain_xboxMultiplayerElements[5]);
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
        else if (result == 22 || result == 23)
        {
            jkGuiDialog_ErrorDialog(L"Multiplayer", L"Coming soon");
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
    jkGui_InitMenu(&jkGuiMain_xboxReadyMenu, jkGui_stdBitmaps[JKGUI_BM_BK_MULTI]);
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
