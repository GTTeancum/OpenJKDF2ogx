#include "jkGUISetup.h"
#include "jkGUIXboxControls.h"

#include "General/Darray.h"
#include "General/stdBitmap.h"
#include "General/stdFont.h"
#include "General/stdStrTable.h"
#include "General/stdFileUtil.h"
#include "Engine/rdMaterial.h" // TODO move stdVBuffer
#include "stdPlatform.h"
#include "jk.h"
#include "Gui/jkGUIRend.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIGameplay.h"
#include "Gui/jkGUIDisplay.h"
#include "Gui/jkGUISound.h"
#include "Gui/jkGUIKeyboard.h"
#include "Gui/jkGUIMouse.h"
#include "Gui/jkGUIJoystick.h"
#include "Gui/jkGUIGeneral.h"
#include "Gui/jkGUIControlOptions.h"
#include "Platform/stdControl.h"
#include "Platform/wuRegistry.h"
#include "Win95/stdDisplay.h"

static jkGuiElement jkGuiSetup_buttons[9] = {
    {ELEMENT_TEXT, 0, 0, 0, 3, {0, 410, 640, 20}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 6, "GUI_SETUP", 3, {20, 20, 600, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 100, 2, "GUI_GENERAL", 3, {20, 80, 120, 40},  1, 0, "GUI_GENERAL_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 101, 2, "GUI_GAMEPLAY", 3, {140, 80, 120, 40}, 1, 0, "GUI_GAMEPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 102, 2, "GUI_DISPLAY", 3, {260, 80, 120, 40},  1, 0, "GUI_DISPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 103, 2, "GUI_SOUND", 3, {380, 80, 120, 40}, 1, 0, "GUI_SOUND_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 104, 2, "GUI_CONTROLS", 3, {500, 80, 120, 40}, 1, 0, "GUI_CONTROLS_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK", 3, {440, 430, 200, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0},
};

static jkGuiMenu jkGuiSetup_menu = {jkGuiSetup_buttons, 0, 0xFF, 0xE1, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

static jkGuiElement jkGuiSetupControls_buttons[13] = {
    {ELEMENT_TEXT, 0, 0, 0, 3, {0, 410, 640, 20}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 6, "GUI_SETUP", 3, {20, 20, 600, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 100, 2, "GUI_GENERAL", 3, {20, 80, 120, 40},  1, 0, "GUI_GENERAL_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 101, 2, "GUI_GAMEPLAY", 3, {140, 80, 120, 40}, 1, 0, "GUI_GAMEPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 102, 2, "GUI_DISPLAY", 3, {260, 80, 120, 40},  1, 0, "GUI_DISPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 103, 2, "GUI_SOUND", 3, {380, 80, 120, 40}, 1, 0, "GUI_SOUND_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 104, 2, "GUI_CONTROLS", 3, {500, 80, 120, 40}, 1, 0, "GUI_CONTROLS_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 105, 2, "GUI_KEYBOARD", 3, {40, 120, 140, 40}, 1, 0, "GUI_KEYBOARD_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 106, 2, "GUI_MOUSE", 3, {180, 120, 140, 40},  1, 0, "GUI_MOUSE_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 107, 2, "GUI_JOYSTICK", 3, {320, 120, 140, 40}, 1, 0, "GUI_JOYSTICK_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 108, 2, "GUI_CONTROLOPTIONS", 3, {460, 120, 140,  40}, 1, 0, "GUI_CONTROLOPTIONS_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK", 3, {440, 430, 200, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0},
};

static jkGuiMenu jkGuiSetupControls_menu = {jkGuiSetupControls_buttons, 0, 0xFF, 0xE1, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

#ifdef TARGET_XBOX
static jkGuiMenu *jkGuiSetupXbox_submenus[8];
static int jkGuiSetupXbox_tab = 100;
static int jkGuiSetupXbox_probe;
static unsigned int jkGuiSetupXbox_probeTime;
static int jkGuiSetupXbox_probeStep;

int jkGuiSetup_XboxIsSubmenu(jkGuiMenu *menu)
{
    int i;
    for (i = 0; i < 8; i++)
        if (menu && jkGuiSetupXbox_submenus[i] == menu) return 1;
    return 0;
}

static void jkGuiSetupXbox_DrawTab(jkGuiElement *element, jkGuiMenu *menu, stdVBuffer *vbuf, BOOL redraw)
{
    if (menu->lastMouseOverClickable == element)
        jkGuiRend_XboxDrawActiveTab(element, menu, vbuf, redraw);
    else
        jkGuiRend_TextButtonDraw(element, menu, vbuf, redraw);
}

int jkGuiSetup_XboxMenuTab(jkGuiMenu *menu)
{
    int i;
    if (menu == &jkGuiSetup_menu) return jkGuiSetupXbox_tab;
    if (jkGuiXboxControls_IsMenu(menu)) return 104;
    if (!jkGuiSetup_XboxIsSubmenu(menu)) return 0;
    for (i = 2; i <= 6; i++)
        if (menu->paElements[i].type == ELEMENT_TEXT) return menu->paElements[i].hoverId;
    return 0;
}

void jkGuiSetup_XboxChangeMenu(jkGuiMenu *menu, int direction)
{
    int tab = jkGuiSetup_XboxMenuTab(menu);
    int next = tab + direction;
    if (!tab || next < 100 || next > 104) return;
    if (menu == &jkGuiSetup_menu) {
        jkGuiSetupXbox_tab = next;
        menu->focusedElement = NULL;
        jkGuiRend_ClickableMouseover(menu, &jkGuiSetup_buttons[next - 98]);
    } else menu->lastClicked = next;
}

static int jkGuiSetupXbox_OrderBefore(jkGuiMenu *menu, jkGuiElement *a, jkGuiElement *b)
{
    /* Gameplay's two narrow checkbox columns belong to the same option:
       visit Solo, then MP, before proceeding to the following row. */
    int pairedA = jkGuiSetup_XboxMenuTab(menu) == 101 && a->type == ELEMENT_CHECKBOX && a->rect.x >= 320;
    int pairedB = jkGuiSetup_XboxMenuTab(menu) == 101 && b->type == ELEMENT_CHECKBOX && b->rect.x >= 320;
    if (pairedA && pairedB)
        return a->rect.y < b->rect.y || (a->rect.y == b->rect.y && a->rect.x < b->rect.x);
    if (pairedA != pairedB) return !pairedA;
    return a->rect.x < b->rect.x || (a->rect.x == b->rect.x && a->rect.y < b->rect.y);
}

int jkGuiSetup_XboxFocus(jkGuiMenu *menu, int dir)
{
    jkGuiElement *element;
    int idx;
    if (jkGuiSetup_XboxIsSubmenu(menu)) {
        jkGuiElement *items[64];
        int count = 0, i, j;
        element = menu->lastMouseOverClickable;
        if (dir == FOCUS_LEFT || dir == FOCUS_RIGHT) {
            /* Sliders retain their standard left/right value adjustment. */
            if (element && element->type == ELEMENT_SLIDER) return 0;
            return 1;
        }
        if (dir != FOCUS_UP && dir != FOCUS_DOWN && dir != FOCUS_NONE) return 0;
        for (i = 7; menu->paElements[i].type != ELEMENT_END && count < 64; i++) {
            jkGuiElement *e = &menu->paElements[i];
            if (!e->bIsVisible || e->enableHover || e->rect.y < 130 || e->rect.y >= 410) continue;
            if (e->type != ELEMENT_CHECKBOX && e->type != ELEMENT_SLIDER && e->type != ELEMENT_TEXTBUTTON) continue;
            j = count;
            while (j > 0 && jkGuiSetupXbox_OrderBefore(menu, e, items[j-1])) {
                items[j] = items[j-1];
                j--;
            }
            items[j] = e;
            count++;
        }
        if (!count) return 1;
        for (idx = 0; idx < count && items[idx] != element; idx++);
        if (idx == count) idx = 0;
        else if (dir == FOCUS_DOWN && idx + 1 < count) idx++;
        else if (dir == FOCUS_UP && idx > 0) idx--;
        menu->focusedElement = NULL;
        jkGuiRend_ClickableMouseover(menu, items[idx]);
        return 1;
    }
    if (menu != &jkGuiSetup_menu) return 0;
    element = menu->lastMouseOverClickable;
    idx = element ? element->hoverId - 100 : jkGuiSetupXbox_tab - 100;
    if (idx < 0 || idx > 4) idx = 0;
    if (dir == FOCUS_LEFT && idx > 0) idx--;
    if (dir == FOCUS_RIGHT && idx < 4) idx++;
    jkGuiSetupXbox_tab = idx + 100;
    menu->focusedElement = NULL;
    jkGuiRend_ClickableMouseover(menu, &jkGuiSetup_buttons[idx + 2]);
    return 1;
}

static int jkGuiSetupXbox_InfoClick(jkGuiElement *element, jkGuiMenu *menu, int x, int y, BOOL redraw)
{
    return 0; /* System-controlled video information, not a setting. */
}

static wchar_t jkGuiSetupXbox_resolutionText[64] = L"Current resolution: 640 x 480";

static jkGuiElement jkGuiSetupXboxDisplay_buttons[14] = {
    {ELEMENT_TEXT, 0, 0, 0, 3, {0, 410, 640, 20}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 6, "GUI_SETUP", 3, {20, 20, 600, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 100, 2, "GUI_GENERAL", 3, {20, 80, 120, 40},  1, 0, "GUI_GENERAL_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 101, 2, "GUI_GAMEPLAY", 3, {140, 80, 120, 40}, 1, 0, "GUI_GAMEPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 102, 2, "GUI_DISPLAY", 3, {260, 80, 120, 40},  1, 0, "GUI_DISPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 103, 2, "GUI_SOUND", 3, {380, 80, 120, 40}, 1, 0, "GUI_SOUND_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 104, 2, "GUI_CONTROLS", 3, {500, 80, 120, 40}, 1, 0, "GUI_CONTROLS_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Display", 3, {30, 145, 580, 24}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 0, 2, jkGuiSetupXbox_resolutionText, 3, {40, 190, 560, 40}, 1, 0, 0, 0, jkGuiSetupXbox_InfoClick, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Aspect ratio follows Xbox system settings.", 3, {40, 245, 560, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Video information is read-only.", 3, {40, 280, 560, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK", 3, {440, 430, 200, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, {0, 430, 200, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0},
};

static jkGuiMenu jkGuiSetupXboxDisplay_menu = {jkGuiSetupXboxDisplay_buttons, 0, 0xFF, 0xE1, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

static void jkGuiSetupXbox_DebugMenu(const char *label, jkGuiMenu *menu)
{
    int i;
    stdPlatform_Printf("SetupDbg: %s menu=%p last=%p focus=%p return=%p escape=%p clickableIdx=%d\n",
        label,
        menu,
        menu->lastMouseOverClickable,
        menu->focusedElement,
        menu->pReturnKeyShortcutElement,
        menu->pEscapeKeyShortcutElement,
        menu->clickableIdxIdk);
    for (i = 0; menu->paElements[i].type != ELEMENT_END; i++)
    {
        jkGuiElement *elem = &menu->paElements[i];
        stdPlatform_Printf("SetupDbg:   elem[%02d]=%p type=%d id=%d visible=%d hover=%d textType=%d rect=(%d,%d,%d,%d)\n",
            i,
            elem,
            elem->type,
            elem->hoverId,
            elem->bIsVisible,
            elem->enableHover,
            elem->textType,
            elem->rect.x,
            elem->rect.y,
            elem->rect.width,
            elem->rect.height);
    }
}

static int jkGuiSetupXbox_ShowDisplay(void)
{
    int w = 640;
    int h = 480;
    int result;
    jkGuiSetupXboxDisplay_buttons[11].bIsVisible = 0;
    if (stdDisplay_pCurVideoMode)
    {
        w = stdDisplay_pCurVideoMode->format.width;
        h = stdDisplay_pCurVideoMode->format.height;
    }
    jk_snwprintf(jkGuiSetupXbox_resolutionText, 64, L"Current resolution: %d x %d", w, h);
    jkGui_sub_412E20(&jkGuiSetupXboxDisplay_menu, 100, 104, 102);
    jkGuiSetup_sub_412EF0(&jkGuiSetupXboxDisplay_menu, 0);
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetupXboxDisplay_menu, NULL);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiSetupXboxDisplay_menu, &jkGuiSetupXboxDisplay_buttons[12]);
    jkGuiSetupXbox_DebugMenu("display-before", &jkGuiSetupXboxDisplay_menu);
    jkGuiRend_XboxFooterBegin(&jkGuiSetupXboxDisplay_menu);
    jkGuiRend_XboxFooterAddElementAction(&jkGuiSetupXboxDisplay_menu, JKGUI_XBOX_BTN_B, &jkGuiSetupXboxDisplay_buttons[12], L"Back");
    result = jkGuiRend_DisplayAndReturnClicked(&jkGuiSetupXboxDisplay_menu);
    return result >= 100 ? result : -1;
}

#endif

void jkGuiSetup_sub_412EF0(jkGuiMenu *menu, int a2)
{
    jkGuiElement *paElements; // eax

    paElements = menu->paElements;
    paElements[2].enableHover = 1;
    paElements[3].enableHover = 1;
    paElements[4].enableHover = 1;
    paElements[5].enableHover = 1;
    paElements[6].enableHover = 1;
#ifdef TARGET_XBOX
    paElements[2].enableHover = 0;
    paElements[3].enableHover = 0;
    paElements[4].enableHover = 0;
    paElements[5].enableHover = 0;
    paElements[6].enableHover = 0;
    paElements[1].wstr = L"Setup";
    paElements[2].wstr = L"General";
    paElements[3].wstr = L"Gameplay";
    paElements[4].wstr = L"Display";
    paElements[5].wstr = L"Sound";
    paElements[6].wstr = L"Controls";
    /* Controls keeps its accepted navigation. Other pages are entered from
       the tab strip and return there with Back, rather than focusing tabs. */
    if (menu != &jkGuiSetup_menu && paElements[6].type != ELEMENT_TEXT)
    {
        int i;
        jkGuiElement *first = NULL;
        for (i = 0; i < 8; i++) {
            if (jkGuiSetupXbox_submenus[i] == menu) break;
            if (!jkGuiSetupXbox_submenus[i]) {
                jkGuiSetupXbox_submenus[i] = menu;
                break;
            }
        }
        for (i = 2; i <= 6; i++) paElements[i].enableHover = 1;
        for (i = 7; paElements[i].type != ELEMENT_END; i++) {
            jkGuiElement *e = &paElements[i];
            if (e->bIsVisible && !e->enableHover && e->rect.y >= 130 && e->rect.y < 410
                && (e->type == ELEMENT_CHECKBOX || e->type == ELEMENT_SLIDER || e->type == ELEMENT_TEXTBUTTON)) {
                first = e;
                break;
            }
        }
        jkGuiRend_XboxSetInitialFocus(menu, first);
    }
#endif
    if ( a2 )
    {
        paElements[7].enableHover = 1;
        paElements[8].enableHover = 1;
        paElements[9].enableHover = 1;
        paElements[10].enableHover = 1;
    }
}

#ifdef TARGET_XBOX
static void jkGuiSetupXbox_SmokeTick(jkGuiMenu *menu)
{
    int mode=1;
    int tab=100;
    FILE *probe=fopen("D:\\xbox_smoke_setup.txt","rb");
    if(probe) {fscanf(probe,"%d %d",&mode,&tab);fclose(probe);}
    menu->idkFunc = NULL;
    menu->lastClicked = mode==2 ? 104 : mode==5 ? 101 : (tab >= 100 && tab <= 104 ? tab : 100);
}

void jkGuiSetup_XboxProbeMenu(jkGuiMenu *menu)
{
    unsigned int now;
    jkGuiElement *target;
    int step;
    if (!jkGuiSetupXbox_probe) return;
    if (menu != &jkGuiSetup_menu && !jkGuiSetup_XboxIsSubmenu(menu)) return;
    now = stdPlatform_GetTimeMsec();
    if (!jkGuiSetupXbox_probeTime) jkGuiSetupXbox_probeTime = now;
    if (now - jkGuiSetupXbox_probeTime < 10000) return;
    jkGuiSetupXbox_probeTime = now;
    step = jkGuiSetupXbox_probeStep++;
    stdPlatform_Printf("SetupNavProbe: step=%d root=%d tab=%d focus=%d\n", step,
        menu == &jkGuiSetup_menu, jkGuiSetupXbox_tab,
        menu->lastMouseOverClickable ? (int)(menu->lastMouseOverClickable - menu->paElements) : -1);
    if (jkGuiSetupXbox_probe == 5) {
        int n;
        if (step == 0) for (n = 0; n < 6; n++) jkGuiRend_FocusElementDir(menu, FOCUS_DOWN);
        else if (step == 1) jkGuiRend_FocusElementDir(menu, FOCUS_RIGHT);
        else if (step >= 2 && step <= 5) jkGuiRend_FocusElementDir(menu, FOCUS_DOWN);
        else if (step == 6) jkGuiSetup_XboxChangeMenu(menu, 1);
        else if (step == 7) jkGuiSetup_XboxChangeMenu(menu, -1);
        else if (step == 8) jkGuiRend_XboxFooterInvokeButton(menu, JKGUI_XBOX_BTN_B);
        else jkGuiSetupXbox_probe = 0;
        return;
    }
    /* The fixture uses production directional focus and click handlers. */
    if (step == 1 || step == 11) {
        jkGuiRend_FocusElementDir(menu, FOCUS_DOWN);
    } else if (step == 3 || step == 6 || step == 9) {
        jkGuiRend_FocusElementDir(menu, FOCUS_RIGHT);
    } else if (step == 0 || step == 4 || step == 7 || step == 10) {
        target = menu->lastMouseOverClickable;
        jkGuiRend_InvokeClicked(target, menu, target->rect.x + 1, target->rect.y + 1, 1);
    } else if (step == 2 || step == 5 || step == 8 || step == 12) {
        jkGuiRend_XboxFooterInvokeButton(menu, JKGUI_XBOX_BTN_B);
    } else {
        jkGuiSetupXbox_probe = 0;
    }
}
#endif

void jkGuiSetup_Show()
{
#ifdef TARGET_XBOX
    int result, mode = 0, n;
    FILE *probe = fopen("D:\\xbox_smoke_setup.txt", "rb");
    if (probe) { fscanf(probe, "%d", &mode); fclose(probe); }
    jkGuiSetupXbox_probe = mode == 3 || mode == 5 ? mode : 0;
    jkGuiSetupXbox_probeTime = 0;
    jkGuiSetupXbox_probeStep = 0;
    jkGuiSetupXbox_tab = 100;
    jkGuiSetup_sub_412EF0(&jkGuiSetup_menu, 0);
    for (n = 2; n <= 6; n++) {
        jkGuiSetup_buttons[n].type = ELEMENT_TEXTBUTTON;
        jkGuiSetup_buttons[n].drawFuncOverride = jkGuiSetupXbox_DrawTab;
    }
    jkGuiSetup_menu.idkFunc = mode == 1 || mode == 2 || mode == 5 ? jkGuiSetupXbox_SmokeTick : NULL;
    for (;;) {
        jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetup_menu, NULL);
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiSetup_menu, &jkGuiSetup_buttons[7]);
        jkGuiRend_XboxSetInitialFocus(&jkGuiSetup_menu, &jkGuiSetup_buttons[jkGuiSetupXbox_tab - 98]);
        jkGuiRend_XboxFooterBegin(&jkGuiSetup_menu);
        jkGuiRend_XboxFooterAddAction(&jkGuiSetup_menu, JKGUI_XBOX_BTN_A, 0, L"Select");
        jkGuiRend_XboxFooterAddElementAction(&jkGuiSetup_menu, JKGUI_XBOX_BTN_B, &jkGuiSetup_buttons[7], L"Back");
        result = jkGuiRend_DisplayAndReturnClicked(&jkGuiSetup_menu);
        if (result < 100 || result > 104) break;
        do {
            jkGuiSetupXbox_tab = result;
            switch (result) {
                case 100: result = jkGuiGeneral_Show(); break;
                case 101: result = jkGuiGameplay_Show(); break;
                case 102: result = jkGuiSetupXbox_ShowDisplay(); break;
                case 103: result = jkGuiSound_Show(); break;
                case 104: result = jkGuiXboxControls_Show(); break;
            }
        } while (result >= 100 && result <= 104);
    }
    jkGuiSetupXbox_probe = 0;
#else
    int i; // esi
    int v1; // edi
    int v2; // eax
;
    jkGuiSetup_sub_412EF0(&jkGuiSetup_menu, 0);
#ifdef TARGET_XBOX
    {
        FILE *probe = fopen("D:\\xbox_smoke_setup.txt", "rb");
        if (probe) {
            fclose(probe);
            jkGuiSetup_menu.idkFunc = jkGuiSetupXbox_SmokeTick;
        }
    }
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetup_menu, NULL);
#else
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetup_menu, &jkGuiSetup_buttons[7]);
#endif
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiSetup_menu, &jkGuiSetup_buttons[7]);
    #ifdef TARGET_XBOX
    jkGuiSetupXbox_DebugMenu("root-before", &jkGuiSetup_menu);
    #endif
    for (
#ifdef TARGET_XBOX
        jkGuiRend_XboxFooterBegin(&jkGuiSetup_menu),
        jkGuiRend_XboxFooterAddAction(&jkGuiSetup_menu, JKGUI_XBOX_BTN_A, 0, L"Select"),
        jkGuiRend_XboxFooterAddElementAction(&jkGuiSetup_menu, JKGUI_XBOX_BTN_B, &jkGuiSetup_buttons[7], L"Back"),
#endif
        i = jkGuiRend_DisplayAndReturnClicked(&jkGuiSetup_menu);
        i != -1;
#ifdef TARGET_XBOX
        jkGuiRend_XboxFooterBegin(&jkGuiSetup_menu),
        jkGuiRend_XboxFooterAddAction(&jkGuiSetup_menu, JKGUI_XBOX_BTN_A, 0, L"Select"),
        jkGuiRend_XboxFooterAddElementAction(&jkGuiSetup_menu, JKGUI_XBOX_BTN_B, &jkGuiSetup_buttons[7], L"Back"),
#endif
        i = jkGuiRend_DisplayAndReturnClicked(&jkGuiSetup_menu) )
    {
        #ifdef TARGET_XBOX
        stdPlatform_Printf("SetupDbg: root returned id=%d\n", i);
        #endif
        if ( i == 1 )
            break;
        if ( i >= 100 )
        {
            while ( 2 )
            {
                if ( i <= 104 )
                {
                    switch ( i )
                    {
                        case 100:
                            i = jkGuiGeneral_Show();
                            goto LABEL_23;
                        case 101:
                            i = jkGuiGameplay_Show();
                            goto LABEL_23;
                        case 102:
#ifdef TARGET_XBOX
                            i = jkGuiSetupXbox_ShowDisplay();
                            goto LABEL_23;
#else
                            i = jkGuiDisplay_Show();
                            goto LABEL_23;
#endif
                        case 103:
                            i = jkGuiSound_Show();
                            goto LABEL_23;
                        case 104:
#ifdef TARGET_XBOX
                            i = jkGuiXboxControls_Show();
                            goto LABEL_23;
#else
                            do
                            {
                                jkGui_sub_412E20(&jkGuiSetupControls_menu, 105, 108, 0);
                                jkGui_sub_412E20(&jkGuiSetupControls_menu, 102, 107, 104);
                                jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetupControls_menu, &jkGuiSetupControls_buttons[11]);
                                jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiSetupControls_menu, &jkGuiSetupControls_buttons[11]);
                                i = jkGuiRend_DisplayAndReturnClicked(&jkGuiSetupControls_menu);
                                v1 = 0;
                                while ( i >= 105 )
                                {
                                    if ( i > 108 )
                                        break;
                                    switch ( i )
                                    {
                                        case 105:
                                            v2 = jkGuiKeyboard_Show();
                                            goto LABEL_17;
                                        case 106:
                                            v2 = jkGuiMouse_Show();
                                            goto LABEL_17;
                                        case 107:
                                            v2 = jkGuiJoystick_Show();
                                            goto LABEL_17;
                                        case 108:
                                            v2 = jkGuiControlOptions_Show();
LABEL_17:
                                            i = v2;
                                            v1 = 1;
                                            break;
                                        default:
                                            break;
                                    }
                                    if ( !v1 )
                                    {
                                        jkGui_sub_412E20(&jkGuiSetup_menu, 105, 108, i);
                                        jkGuiSetup_menu.paElements[jkGuiSetup_menu.clickableIdxIdk].wstr = 0; // MOTS added
                                        jkGuiRend_Paint(&jkGuiSetup_menu);
                                    }
                                }
                            }
                            while ( v1 );
                            if ( i != -1 )
                                goto LABEL_23;
                            return;
#endif
                        default:
LABEL_23:
                            jkGui_sub_412E20(&jkGuiSetup_menu, 100, 104, i);
                            jkGuiSetup_menu.paElements[jkGuiSetup_menu.clickableIdxIdk].wstr = 0; // MOTS added
                            jkGuiRend_Paint(&jkGuiSetup_menu);
                            if ( i < 100 )
                                break;
                            continue;
                    }
                }
                break;
            }
        }
#ifdef TARGET_XBOX
        jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetup_menu, NULL);
#else
        jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetup_menu, &jkGuiSetup_buttons[7]);
#endif
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiSetup_menu, &jkGuiSetup_buttons[7]);
        #ifdef TARGET_XBOX
        jkGuiSetupXbox_DebugMenu("root-loop", &jkGuiSetup_menu);
        #endif
    }
    #ifdef TARGET_XBOX
    stdPlatform_Printf("SetupDbg: leaving setup i=%d\n", i);
    #endif
#endif
}

void jkGuiSetup_Startup()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added

    jkGui_InitMenu(&jkGuiSetup_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
    jkGui_InitMenu(&jkGuiSetupControls_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
#ifdef TARGET_XBOX
    jkGui_InitMenu(&jkGuiSetupXboxDisplay_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
    jkGuiXboxControls_Startup();
#endif
}

void jkGuiSetup_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added
#ifdef TARGET_XBOX
    jkGuiXboxControls_Shutdown();
#endif
}
