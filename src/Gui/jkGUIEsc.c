#include "jkGUIEsc.h"

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
#include "Gui/jkGUIObjectives.h"
#include "Gui/jkGUIMap.h"
#include "Gui/jkGUISaveLoad.h"
#include "Gui/jkGUISetup.h"
#include "Gui/jkGUIForce.h"
#include "World/jkPlayer.h"
#include "Main/jk.h"
#include "Main/jkDev.h"
#include "Main/jkStrings.h"
#include "Main/jkMain.h"
#include "Platform/stdControl.h"
#include "Dss/sithMulti.h"
#include "Devices/sithSoundMixer.h"

enum jkGuiEscButton_t
{
    JKGUIESC_RETURNTOGAME = 1,
    JKGUIESC_OBJECTIVES   = 10,
    JKGUIESC_MAP          = 11,
    JKGUIESC_JEDIPOWERS   = 12,
    JKGUIESC_LOAD         = 13,
    JKGUIESC_RESTART      = 14,
    JKGUIESC_SAVE         = 15,
    JKGUIESC_SETUP        = 16,
    JKGUIESC_ABORT        = 17
};

enum jkGuiEscElement_t
{
    JKGUIESC_ELMT_OBJECTIVES   = 0,
    JKGUIESC_ELMT_MAP          = 1,
    JKGUIESC_ELMT_JEDIPOWERS   = 2,
    JKGUIESC_ELMT_RETURNTOGAME = 3,
    JKGUIESC_ELMT_LOAD         = 4,
    JKGUIESC_ELMT_SAVE         = 5,
    JKGUIESC_ELMT_RESTART      = 6,
    JKGUIESC_ELMT_SETUP        = 7,
    JKGUIESC_ELMT_ABORT        = 8,
};

static jkGuiElement jkGuiEsc_aElements[10] = {
    { ELEMENT_TEXTBUTTON, JKGUIESC_OBJECTIVES,   5, "GUI_OBJECTIVES",     3, {  0, 50,  400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_MAP,          5, "GUI_MAP",            3, {  0, 100, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_JEDIPOWERS,   5, "GUI_JEDIPOWERS",     3, {  0, 150, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_RETURNTOGAME, 5, "GUI_RETURN_TO_GAME", 3, {  0, 240, 400, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_LOAD,         5, "GUI_LOAD",           3, {400, 270, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_SAVE,         5, "GUI_SAVE",           3, {400, 320, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_RESTART,      5, "GUI_RESTART",        3, {400, 220, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_SETUP,        5, "GUI_SETUP",          3, {400, 370, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_TEXTBUTTON, JKGUIESC_ABORT,        5, "GUI_ABORT",          3, {400, 420, 240, 40},  1,  0,  0,  0,  0,  0, {0}, 0},
    { ELEMENT_END,        0,                     0,  NULL,                0, {0},                  0,  0,  0,  0,  0,  0, {0}, 0}
};

static jkGuiMenu jkGuiEsc_menu = { jkGuiEsc_aElements, -1, 0x0FFFF, 0x0FFFF, 0x0F, 0, 0, jkGui_stdBitmaps, jkGui_stdFonts, 0, 0, "thermloop01.wav", "thrmlpu2.wav", 0, 0, 0, 0, 0, 0 };

#ifdef TARGET_XBOX
typedef enum jkGuiEscKonamiInput
{
    JKGUIESC_KONAMI_UP = 0,
    JKGUIESC_KONAMI_DOWN,
    JKGUIESC_KONAMI_LEFT,
    JKGUIESC_KONAMI_RIGHT,
    JKGUIESC_KONAMI_B,
    JKGUIESC_KONAMI_A
} jkGuiEscKonamiInput;

static int jkGuiEsc_xboxKonamiIdx;
static int jkGuiEsc_xboxKonamiFired;

static int jkGuiEsc_XboxKonamiInput(int input)
{
    static const int sequence[] =
    {
        JKGUIESC_KONAMI_UP,
        JKGUIESC_KONAMI_UP,
        JKGUIESC_KONAMI_DOWN,
        JKGUIESC_KONAMI_DOWN,
        JKGUIESC_KONAMI_LEFT,
        JKGUIESC_KONAMI_RIGHT,
        JKGUIESC_KONAMI_LEFT,
        JKGUIESC_KONAMI_RIGHT,
        JKGUIESC_KONAMI_B,
        JKGUIESC_KONAMI_A
    };
    const int sequenceCount = (int)(sizeof(sequence) / sizeof(sequence[0]));

    if (input == sequence[jkGuiEsc_xboxKonamiIdx])
    {
        jkGuiEsc_xboxKonamiIdx++;
        if (jkGuiEsc_xboxKonamiIdx >= sequenceCount)
        {
            jkGuiEsc_xboxKonamiIdx = 0;
            if (jkDev_GiveAllCurrentMode())
            {
                jkGuiEsc_xboxKonamiFired = 1;
                jkGuiRend_PlayWav(jkGuiEsc_menu.soundClick);
            }
        }
        return 1;
    }

    jkGuiEsc_xboxKonamiIdx = (input == sequence[0]) ? 1 : 0;
    return jkGuiEsc_xboxKonamiIdx ? 1 : 0;
}

static int jkGuiEsc_XboxPollKonami(void)
{
    int consumed = 0;
    if (stdControl_XboxGetControllerKeyPress(0, KEY_JOY1_HUP))
        consumed |= jkGuiEsc_XboxKonamiInput(JKGUIESC_KONAMI_UP);
    if (stdControl_XboxGetControllerKeyPress(0, KEY_JOY1_HDOWN))
        consumed |= jkGuiEsc_XboxKonamiInput(JKGUIESC_KONAMI_DOWN);
    if (stdControl_XboxGetControllerKeyPress(0, KEY_JOY1_HLEFT))
        consumed |= jkGuiEsc_XboxKonamiInput(JKGUIESC_KONAMI_LEFT);
    if (stdControl_XboxGetControllerKeyPress(0, KEY_JOY1_HRIGHT))
        consumed |= jkGuiEsc_XboxKonamiInput(JKGUIESC_KONAMI_RIGHT);
    if (stdControl_XboxGetControllerKeyPress(0, KEY_JOY1_B2))
        consumed |= jkGuiEsc_XboxKonamiInput(JKGUIESC_KONAMI_B);
    if (stdControl_XboxGetControllerKeyPress(0, KEY_JOY1_B1))
        consumed |= jkGuiEsc_XboxKonamiInput(JKGUIESC_KONAMI_A);
    return consumed;
}

static void jkGuiEsc_XboxTick(jkGuiMenu *menu)
{
    (void)menu;
    jkGuiEsc_XboxPollKonami();
}

static void jkGuiEsc_SetRect(jkGuiElement *element, int x, int y, int w, int h)
{
    element->rect.x = x;
    element->rect.y = y;
    element->rect.width = w;
    element->rect.height = h;
}

static void jkGuiEsc_ApplyXboxLayout(void)
{
    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_OBJECTIVES],   0,  40, 400, 36);
    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_MAP],          0,  82, 400, 36);
    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_JEDIPOWERS],   0, 124, 400, 36);
    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_RETURNTOGAME], 0, 200, 400, 36);

    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART],    390, 125, 250, 36);
    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD],       390, 168, 250, 36);
    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE],       390, 211, 250, 36);
    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_SETUP],      390, 254, 250, 36);
    jkGuiEsc_SetRect(&jkGuiEsc_aElements[JKGUIESC_ELMT_ABORT],      390, 297, 250, 36);
}
#endif

int jkGuiEsc_HandleControllerFocus(jkGuiMenu *menu, int32_t dir)
{
    static const int order[] = {
        JKGUIESC_ELMT_OBJECTIVES,
        JKGUIESC_ELMT_MAP,
        JKGUIESC_ELMT_JEDIPOWERS,
        JKGUIESC_ELMT_RETURNTOGAME,
        JKGUIESC_ELMT_RESTART,
        JKGUIESC_ELMT_LOAD,
        JKGUIESC_ELMT_SAVE,
        JKGUIESC_ELMT_SETUP,
        JKGUIESC_ELMT_ABORT
    };
    int curOrderIdx = -1;
    int step = 0;

    if (menu != &jkGuiEsc_menu) {
        return 0;
    }
    stdPlatform_Printf("GuiEscFocus: dir=%d cur=%p curId=%d\n",
        dir,
        menu->lastMouseOverClickable,
        menu->lastMouseOverClickable ? menu->lastMouseOverClickable->hoverId : -999);
    if (dir == FOCUS_DOWN) {
        step = 1;
    }
    else if (dir == FOCUS_UP) {
        step = -1;
    }
    else {
        return 0;
    }

    jkGuiElement *cur = menu->lastMouseOverClickable;
    if (cur && !cur->bIsVisible) {
        cur = NULL;
    }

    for (int i = 0; i < (int)(sizeof(order) / sizeof(order[0])); i++) {
        if (cur == &jkGuiEsc_aElements[order[i]]) {
            curOrderIdx = i;
            break;
        }
    }

    if (curOrderIdx < 0) {
        curOrderIdx = step > 0 ? -1 : (int)(sizeof(order) / sizeof(order[0]));
    }

    for (int i = curOrderIdx + step; i >= 0 && i < (int)(sizeof(order) / sizeof(order[0])); i += step) {
        jkGuiElement *next = &jkGuiEsc_aElements[order[i]];
        if (next->bIsVisible) {
            stdPlatform_Printf("GuiEscFocus: next orderIdx=%d elemIdx=%d elem=%p id=%d visible=%d\n",
                i,
                order[i],
                next,
                next->hoverId,
                next->bIsVisible);
            menu->focusedElement = NULL;
            jkGuiRend_ClickableMouseover(menu, next);
            return 1;
        }
    }

    stdPlatform_Printf("GuiEscFocus: no visible next step=%d curIdx=%d\n", step, curOrderIdx);
    return 1;
}

void jkGuiEsc_Startup()
{
    jkGui_InitMenu(&jkGuiEsc_menu, jkGui_stdBitmaps[JKGUI_BM_BK_ESC]);
    jkGuiEsc_bInitialized = 1;
}

void jkGuiEsc_Shutdown()
{
    stdPlatform_Printf("OpenJKDF2: %s\n", __func__); // Added
    
    jkGuiEsc_bInitialized = 0;
}

void jkGuiEsc_Show()
{
    int32_t v3; // eax
    int32_t clicked;
#ifdef TARGET_XBOX
    int xboxCheatConsumed;
#endif

    sithSoundMixer_PauseSong(1);

    if ( sithNet_isMulti )
    {
        jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 0;
        jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
        jkGuiEsc_aElements[JKGUIESC_ELMT_OBJECTIVES].bIsVisible = !!(sithMulti_multiModeFlags & MULTIMODEFLAG_COOP); // Added: co-op
        jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 0;
    }
    else
    {
        jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_OBJECTIVES].bIsVisible = 1;
        jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 1;

        // MOTS added
        if (Main_bMotsCompat) {
            if (sithPlayer_pLocalPlayerThing->thingflags & SITH_TF_DEAD || sithPlayer_pLocalPlayerThing->actorParams.typeflags & SITH_AF_DISABLED)
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
            if (sithPlayer_pLocalPlayerThing->actorParams.typeflags & SITH_AF_DISABLED) {
                jkGuiEsc_aElements[JKGUIESC_ELMT_LOAD].bIsVisible = 0;
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
                jkGuiEsc_aElements[JKGUIESC_ELMT_RESTART].bIsVisible = 0;
            }
        }
        else {
            if (sithPlayer_pLocalPlayerThing->thingflags & SITH_TF_DEAD)
                jkGuiEsc_aElements[JKGUIESC_ELMT_SAVE].bIsVisible = 0;
        }
    }

#ifdef TARGET_XBOX
    jkGuiEsc_ApplyXboxLayout();
    jkGuiEsc_xboxKonamiIdx = 0;
    jkGuiEsc_xboxKonamiFired = 0;
    jkGuiEsc_menu.idkFunc = jkGuiEsc_XboxTick;
#endif

    while ( 1 )
    {
        jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiEsc_menu, &jkGuiEsc_aElements[JKGUIESC_ELMT_RETURNTOGAME]);
#ifdef TARGET_XBOX
        jkGuiRend_XboxFooterBegin(&jkGuiEsc_menu);
        jkGuiRend_XboxFooterAddAction(&jkGuiEsc_menu, JKGUI_XBOX_BTN_A, 0, L"Select");
        jkGuiRend_XboxFooterAddElementAction(&jkGuiEsc_menu, JKGUI_XBOX_BTN_B, &jkGuiEsc_aElements[JKGUIESC_ELMT_RETURNTOGAME], L"Back");
#endif
        clicked = jkGuiRend_DisplayAndReturnClicked(&jkGuiEsc_menu);
#ifdef TARGET_XBOX
        xboxCheatConsumed = jkGuiEsc_XboxPollKonami();
        if (jkGuiEsc_xboxKonamiFired)
        {
            jkGuiEsc_xboxKonamiFired = 0;
            continue;
        }
        if (xboxCheatConsumed && clicked == -1)
            continue;
#endif
        switch (clicked)
        {
            case -1:
                return;

            case JKGUIESC_OBJECTIVES:
                jkGuiObjectives_Show();
                continue;

            case JKGUIESC_MAP:
                jkGuiMap_Show();
                continue;

            case JKGUIESC_JEDIPOWERS:
                jkGuiForce_Show(0, 0.0, 0.0, 0, 0, 0);
                continue;

            case JKGUIESC_LOAD:
                v3 = jkGuiSaveLoad_Show(0);
                if ( v3 == 1 )
                {
                    jkMain_MissionReload();
                    jkGuiRend_UpdateSurface();
                    return;
                }
                if ( v3 != 34 )
                    continue;
                jkGuiRend_UpdateSurface();
                return;

            case JKGUIESC_RESTART:
                if ( !jkGuiDialog_YesNoDialog(jkStrings_GetUniStringWithFallback("GUI_RESTART_MISSION"), jkStrings_GetUniStringWithFallback("GUI_CONFIRM_RESTART")) )
                    continue;
                jkPlayer_LoadAutosave();
                jkMain_MissionReload();
                jkGuiRend_UpdateSurface();
                return;

            case JKGUIESC_SAVE:
                if ( jkGuiSaveLoad_Show(1) != 1 )
                    continue;

            case JKGUIESC_RETURNTOGAME:
                jkMain_MissionReload();
                jkGuiRend_UpdateSurface();
                return;

            case JKGUIESC_SETUP:
                jkGuiSetup_Show();
                continue;

            case JKGUIESC_ABORT:
                if ( !jkGuiDialog_YesNoDialog(jkStrings_GetUniStringWithFallback("GUI_ABORT_GAME"), jkStrings_GetUniStringWithFallback("GUI_CONFIRM_ABORT")) )
                    continue;
                jkMain_MenuReturn();
                jkGuiRend_UpdateSurface();
                return;

            default:
                continue;
        }
    }
}
