#include "jkGUISetup.h"

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
static int32_t jkGuiSetupXbox_sliderImages[2] = {JKGUI_BM_SLIDER_BACK, JKGUI_BM_SLIDER_THUMB};
static wchar_t jkGuiSetupXbox_resolutionText[64] = L"Current resolution: 640 x 480";
static wchar_t jkGuiSetupXbox_sensitivityText[32] = L"Look sensitivity: 50";

static jkGuiElement jkGuiSetupXboxDisplay_buttons[14] = {
    {ELEMENT_TEXT, 0, 0, 0, 3, {0, 410, 640, 20}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 6, "GUI_SETUP", 3, {20, 20, 600, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 100, 2, "GUI_GENERAL", 3, {20, 80, 120, 40},  1, 0, "GUI_GENERAL_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 101, 2, "GUI_GAMEPLAY", 3, {140, 80, 120, 40}, 1, 0, "GUI_GAMEPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 102, 2, "GUI_DISPLAY", 3, {260, 80, 120, 40},  1, 0, "GUI_DISPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 103, 2, "GUI_SOUND", 3, {380, 80, 120, 40}, 1, 0, "GUI_SOUND_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 104, 2, "GUI_CONTROLS", 3, {500, 80, 120, 40}, 1, 0, "GUI_CONTROLS_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Display", 3, {30, 145, 580, 24}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, jkGuiSetupXbox_resolutionText, 2, {40, 195, 560, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Video mode is fixed on Xbox for now.", 2, {40, 230, 560, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Enhanced display options can be added later.", 2, {40, 255, 560, 22}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK", 3, {440, 430, 200, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, {0, 430, 200, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0},
};

static jkGuiMenu jkGuiSetupXboxDisplay_menu = {jkGuiSetupXboxDisplay_buttons, 0, 0xFF, 0xE1, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

static jkGuiElement jkGuiSetupXboxControls_buttons[25] = {
    {ELEMENT_TEXT, 0, 0, 0, 3, {0, 410, 640, 20}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 6, "GUI_SETUP", 3, {20, 20, 600, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 100, 2, "GUI_GENERAL", 3, {20, 80, 120, 40},  1, 0, "GUI_GENERAL_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 101, 2, "GUI_GAMEPLAY", 3, {140, 80, 120, 40}, 1, 0, "GUI_GAMEPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 102, 2, "GUI_DISPLAY", 3, {260, 80, 120, 40},  1, 0, "GUI_DISPLAY_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 103, 2, "GUI_SOUND", 3, {380, 80, 120, 40}, 1, 0, "GUI_SOUND_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 104, 2, "GUI_CONTROLS", 3, {500, 80, 120, 40}, 1, 0, "GUI_CONTROLS_HINT", 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Controls", 3, {30, 128, 580, 24}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Options", 2, {40, 160, 560, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, jkGuiSetupXbox_sensitivityText, 2, {50, 188, 220, 20}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_SLIDER, 0, 0, (const char*)100, 0, {285, 182, 320, 30}, 1, 0, 0, 0, 0, jkGuiSetupXbox_sliderImages, {0}, 0},
    {ELEMENT_CHECKBOX, 0, 0, L"Invert look", 0, {50, 225, 220, 20}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_CHECKBOX, 0, 0, L"Vibration", 0, {335, 225, 220, 20}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Default gamepad layout", 2, {40, 270, 560, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Move: Left stick", 2, {50, 300, 250, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Look: Right stick", 2, {335, 300, 250, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Fire: RT", 2, {50, 325, 250, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Alt fire: LT", 2, {335, 325, 250, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Jump / confirm: A", 2, {50, 350, 250, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Crouch / back: B", 2, {335, 350, 250, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Use / activate: X", 2, {50, 375, 250, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXT, 0, 0, L"Weapons: Black / White", 2, {335, 375, 250, 18}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, 1, 2, "GUI_OK", 3, {440, 430, 200, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_TEXTBUTTON, -1, 2, "GUI_CANCEL", 3, {0, 430, 200, 40}, 1, 0, 0, 0, 0, 0, {0}, 0},
    {ELEMENT_END, 0, 0, 0, 0, {0}, 0, 0, 0, 0, 0, 0, {0}, 0},
};

static jkGuiMenu jkGuiSetupXboxControls_menu = {jkGuiSetupXboxControls_buttons, 0, 0xFF, 0xE1, 0xF, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0};

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

static void jkGuiSetupXbox_UpdateSensitivityText(void)
{
    jk_snwprintf(jkGuiSetupXbox_sensitivityText, 32, L"Look sensitivity: %d", jkGuiSetupXboxControls_buttons[10].selectedTextEntry);
}

static int jkGuiSetupXbox_ShowDisplay(void)
{
    int w = 640;
    int h = 480;
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
    if (jkGuiRend_DisplayAndReturnClicked(&jkGuiSetupXboxDisplay_menu) == -1)
    {
        stdPlatform_Printf("SetupDbg: display returned -1\n");
        return -1;
    }
    stdPlatform_Printf("SetupDbg: display returned keep tab 102\n");
    return 102;
}

static int jkGuiSetupXbox_ShowControls(void)
{
    int result;
    jkGui_sub_412E20(&jkGuiSetupXboxControls_menu, 100, 104, 104);
    jkGuiSetup_sub_412EF0(&jkGuiSetupXboxControls_menu, 0);
    jkGuiSetupXboxControls_buttons[10].selectedTextEntry = stdControl_XboxGetLookSensitivity();
    jkGuiSetupXboxControls_buttons[11].selectedTextEntry = stdControl_XboxGetInvertLook();
    jkGuiSetupXboxControls_buttons[12].selectedTextEntry = stdControl_XboxGetVibration();
    jkGuiSetupXbox_UpdateSensitivityText();
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetupXboxControls_menu, NULL);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiSetupXboxControls_menu, &jkGuiSetupXboxControls_buttons[23]);
    jkGuiSetupXbox_DebugMenu("controls-before", &jkGuiSetupXboxControls_menu);
    result = jkGuiRend_DisplayAndReturnClicked(&jkGuiSetupXboxControls_menu);
    stdPlatform_Printf("SetupDbg: controls returned %d sens=%d invert=%d vibration=%d\n",
        result,
        jkGuiSetupXboxControls_buttons[10].selectedTextEntry,
        jkGuiSetupXboxControls_buttons[11].selectedTextEntry,
        jkGuiSetupXboxControls_buttons[12].selectedTextEntry);
    if (result != -1)
    {
        jkGuiSetupXbox_UpdateSensitivityText();
        stdControl_XboxSetLookOptions(
            jkGuiSetupXboxControls_buttons[10].selectedTextEntry,
            jkGuiSetupXboxControls_buttons[11].selectedTextEntry,
            jkGuiSetupXboxControls_buttons[12].selectedTextEntry);
        wuRegistry_SaveInt("xboxLookSensitivity", jkGuiSetupXboxControls_buttons[10].selectedTextEntry);
        wuRegistry_SaveBool("xboxInvertLook", jkGuiSetupXboxControls_buttons[11].selectedTextEntry);
        wuRegistry_SaveBool("xboxVibration", jkGuiSetupXboxControls_buttons[12].selectedTextEntry);
    }
    return (result == -1) ? -1 : 104;
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
#endif
    if ( a2 )
    {
        paElements[7].enableHover = 1;
        paElements[8].enableHover = 1;
        paElements[9].enableHover = 1;
        paElements[10].enableHover = 1;
    }
}

void jkGuiSetup_Show()
{
    int i; // esi
    int v1; // edi
    int v2; // eax
;
    jkGuiSetup_sub_412EF0(&jkGuiSetup_menu, 0);
#ifdef TARGET_XBOX
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetup_menu, NULL);
#else
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiSetup_menu, &jkGuiSetup_buttons[7]);
#endif
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiSetup_menu, &jkGuiSetup_buttons[7]);
    #ifdef TARGET_XBOX
    jkGuiSetupXbox_DebugMenu("root-before", &jkGuiSetup_menu);
    #endif
    for ( i = jkGuiRend_DisplayAndReturnClicked(&jkGuiSetup_menu); i != -1; i = jkGuiRend_DisplayAndReturnClicked(&jkGuiSetup_menu) )
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
                            i = jkGuiSetupXbox_ShowControls();
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
}

void jkGuiSetup_Startup()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added

    jkGui_InitMenu(&jkGuiSetup_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
    jkGui_InitMenu(&jkGuiSetupControls_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
#ifdef TARGET_XBOX
    jkGui_InitMenu(&jkGuiSetupXboxDisplay_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
    jkGui_InitMenu(&jkGuiSetupXboxControls_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
#endif
}

void jkGuiSetup_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added
}
