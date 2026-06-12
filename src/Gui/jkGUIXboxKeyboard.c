#include "Gui/jkGUIXboxKeyboard.h"

#ifdef TARGET_XBOX

#include "Devices/sithControl.h"
#include "General/stdString.h"
#include "Gui/jkGUI.h"
#include "Gui/jkGUIRend.h"
#include "Main/jkStrings.h"
#include "Platform/stdControl.h"
#include "stdPlatform.h"
#include "jk.h"

#define XKB_KEY_FIRST      1000
#define XKB_CMD_SHIFT      1100
#define XKB_CMD_SPACE      1101
#define XKB_CMD_BACKSPACE  1102
#define XKB_CMD_DONE       1103
#define XKB_CMD_CANCEL     1104

#define XKB_NUM_KEYS       40
#define XKB_NUM_ELEMENTS   (3 + XKB_NUM_KEYS + 5 + 1)

static wchar_t jkGuiXboxKeyboard_title[64];
static wchar_t jkGuiXboxKeyboard_input[64];
static wchar_t jkGuiXboxKeyboard_labels[XKB_NUM_KEYS][8];
static wchar_t jkGuiXboxKeyboard_shiftLabel[16] = L"SHIFT";
static wchar_t jkGuiXboxKeyboard_spaceLabel[16] = L"SPACE";
static wchar_t jkGuiXboxKeyboard_backLabel[16] = L"BACK";
static wchar_t jkGuiXboxKeyboard_doneLabel[16] = L"DONE";
static wchar_t jkGuiXboxKeyboard_cancelLabel[16] = L"CANCEL";

static wchar_t *jkGuiXboxKeyboard_target = NULL;
static int jkGuiXboxKeyboard_targetMax = 0;
static int jkGuiXboxKeyboard_caps = 1;
static int jkGuiXboxKeyboard_initted = 0;
static int jkGuiXboxKeyboard_prevX = 0;
static int jkGuiXboxKeyboard_prevY = 0;
static int jkGuiXboxKeyboard_prevStart = 0;

static jkGuiElement jkGuiXboxKeyboard_elements[XKB_NUM_ELEMENTS];
static jkGuiMenu jkGuiXboxKeyboard_menu =
{
    jkGuiXboxKeyboard_elements, -1, 65535, 65535, 15, NULL, NULL,
    jkGui_stdBitmaps, jkGui_stdFonts, 0, NULL,
    "thermloop01.wav", "thrmlpu2.wav",
    NULL, NULL, NULL, 0, NULL, NULL
};

static const char jkGuiXboxKeyboard_chars[XKB_NUM_KEYS] =
{
    '1','2','3','4','5','6','7','8','9','0',
    'Q','W','E','R','T','Y','U','I','O','P',
    'A','S','D','F','G','H','J','K','L','_',
    'Z','X','C','V','B','N','M','-',' ','\b'
};

static int jkGuiXboxKeyboard_ReadEdge(int key, int *prev)
{
    int val = 0;
    int down = stdControl_ReadKey(key, &val) && val;
    int edge = down && !*prev;
    *prev = down;
    return edge;
}

static void jkGuiXboxKeyboard_SetRect(rdRect *rect, int x, int y, int w, int h)
{
    rect->x = x;
    rect->y = y;
    rect->width = w;
    rect->height = h;
}

static void jkGuiXboxKeyboard_ResetEdges(void)
{
    int val = 0;
    jkGuiXboxKeyboard_prevX = stdControl_ReadKey(KEY_JOY1_B3, &val) && val;
    jkGuiXboxKeyboard_prevY = stdControl_ReadKey(KEY_JOY1_B4, &val) && val;
    jkGuiXboxKeyboard_prevStart = stdControl_ReadKey(KEY_JOY1_B7, &val) && val;
}

static void jkGuiXboxKeyboard_UpdateText(jkGuiMenu *menu)
{
    if (jkGuiXboxKeyboard_target)
    {
        _wcsncpy(jkGuiXboxKeyboard_input, jkGuiXboxKeyboard_target, 63);
        jkGuiXboxKeyboard_input[63] = 0;
    }
    if (menu)
        jkGuiRend_UpdateAndDrawClickable(&jkGuiXboxKeyboard_elements[2], menu, 1);
}

static void jkGuiXboxKeyboard_RebuildKeyLabels(void)
{
    int i;
    for (i = 0; i < XKB_NUM_KEYS; i++)
    {
        char ch = jkGuiXboxKeyboard_chars[i];
        if (ch == ' ')
        {
            _wcscpy(jkGuiXboxKeyboard_labels[i], L"SP");
        }
        else if (ch == '\b')
        {
            _wcscpy(jkGuiXboxKeyboard_labels[i], L"<");
        }
        else
        {
            if (ch >= 'A' && ch <= 'Z' && !jkGuiXboxKeyboard_caps)
                ch = (char)(ch + ('a' - 'A'));
            jkGuiXboxKeyboard_labels[i][0] = (wchar_t)ch;
            jkGuiXboxKeyboard_labels[i][1] = 0;
        }
    }
}

static void jkGuiXboxKeyboard_RestoreWideLabels(void)
{
    int i;
    jkGuiXboxKeyboard_elements[1].wstr = jkGuiXboxKeyboard_title;
    jkGuiXboxKeyboard_elements[2].wstr = jkGuiXboxKeyboard_input;
    for (i = 0; i < XKB_NUM_KEYS; i++)
        jkGuiXboxKeyboard_elements[3 + i].wstr = jkGuiXboxKeyboard_labels[i];
    jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 0].wstr = jkGuiXboxKeyboard_shiftLabel;
    jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 1].wstr = jkGuiXboxKeyboard_spaceLabel;
    jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 2].wstr = jkGuiXboxKeyboard_backLabel;
    jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 3].wstr = jkGuiXboxKeyboard_cancelLabel;
    jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 4].wstr = jkGuiXboxKeyboard_doneLabel;
}

static void jkGuiXboxKeyboard_AppendChar(wchar_t ch, jkGuiMenu *menu)
{
    size_t len;
    if (!jkGuiXboxKeyboard_target || jkGuiXboxKeyboard_targetMax <= 1)
        return;

    len = _wcslen(jkGuiXboxKeyboard_target);
    if ((int)len >= jkGuiXboxKeyboard_targetMax - 1)
    {
        jkGui_MessageBeep();
        return;
    }

    jkGuiXboxKeyboard_target[len] = ch;
    jkGuiXboxKeyboard_target[len + 1] = 0;
    jkGuiXboxKeyboard_UpdateText(menu);
}

static void jkGuiXboxKeyboard_Backspace(jkGuiMenu *menu)
{
    size_t len;
    if (!jkGuiXboxKeyboard_target)
        return;
    len = _wcslen(jkGuiXboxKeyboard_target);
    if (!len)
    {
        jkGui_MessageBeep();
        return;
    }
    jkGuiXboxKeyboard_target[len - 1] = 0;
    jkGuiXboxKeyboard_UpdateText(menu);
}

static int jkGuiXboxKeyboard_ButtonClicked(jkGuiElement *element, jkGuiMenu *menu, int32_t mouseX, int32_t mouseY, BOOL redraw)
{
    int id = element->hoverId;
    if (id >= XKB_KEY_FIRST && id < XKB_KEY_FIRST + XKB_NUM_KEYS)
    {
        char ch = jkGuiXboxKeyboard_chars[id - XKB_KEY_FIRST];
        if (ch == '\b')
            jkGuiXboxKeyboard_Backspace(menu);
        else
        {
            if (ch >= 'A' && ch <= 'Z' && !jkGuiXboxKeyboard_caps)
                ch = (char)(ch + ('a' - 'A'));
            jkGuiXboxKeyboard_AppendChar((wchar_t)ch, menu);
        }
        return 0;
    }

    switch (id)
    {
        case XKB_CMD_SHIFT:
            jkGuiXboxKeyboard_caps = !jkGuiXboxKeyboard_caps;
            jkGuiXboxKeyboard_RebuildKeyLabels();
            jkGuiRend_Paint(menu);
            return 0;
        case XKB_CMD_SPACE:
            jkGuiXboxKeyboard_AppendChar(L' ', menu);
            return 0;
        case XKB_CMD_BACKSPACE:
            jkGuiXboxKeyboard_Backspace(menu);
            return 0;
        case XKB_CMD_DONE:
            return 1;
        case XKB_CMD_CANCEL:
            return -1;
        default:
            return 0;
    }
}

static void jkGuiXboxKeyboard_Tick(jkGuiMenu *menu)
{
    (void)menu;
}

static void jkGuiXboxKeyboard_InitElements(void)
{
    int row, col, idx;
    int left = 70;
    int top = 140;
    int keyW = 50;
    int keyH = 34;
    int gap = 6;

    _memset(jkGuiXboxKeyboard_elements, 0, sizeof(jkGuiXboxKeyboard_elements));

    jkGuiXboxKeyboard_elements[0].type = ELEMENT_TEXT;
    jkGuiXboxKeyboard_elements[0].textType = 0;
    jkGuiXboxKeyboard_elements[0].selectedTextEntry = 3;
    jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[0].rect, 0, 410, 640, 20);
    jkGuiXboxKeyboard_elements[0].bIsVisible = 1;

    jkGuiXboxKeyboard_elements[1].type = ELEMENT_TEXT;
    jkGuiXboxKeyboard_elements[1].textType = 5;
    jkGuiXboxKeyboard_elements[1].wstr = jkGuiXboxKeyboard_title;
    jkGuiXboxKeyboard_elements[1].selectedTextEntry = 3;
    jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[1].rect, 40, 35, 560, 45);
    jkGuiXboxKeyboard_elements[1].bIsVisible = 1;

    jkGuiXboxKeyboard_elements[2].type = ELEMENT_TEXTBUTTON;
    jkGuiXboxKeyboard_elements[2].textType = 2;
    jkGuiXboxKeyboard_elements[2].wstr = jkGuiXboxKeyboard_input;
    jkGuiXboxKeyboard_elements[2].selectedTextEntry = 3;
    jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[2].rect, 90, 85, 460, 36);
    jkGuiXboxKeyboard_elements[2].bIsVisible = 1;

    for (row = 0; row < 4; row++)
    {
        for (col = 0; col < 10; col++)
        {
            idx = row * 10 + col;
            jkGuiXboxKeyboard_elements[3 + idx].type = ELEMENT_TEXTBUTTON;
            jkGuiXboxKeyboard_elements[3 + idx].hoverId = XKB_KEY_FIRST + idx;
            jkGuiXboxKeyboard_elements[3 + idx].textType = 2;
            jkGuiXboxKeyboard_elements[3 + idx].wstr = jkGuiXboxKeyboard_labels[idx];
            jkGuiXboxKeyboard_elements[3 + idx].selectedTextEntry = 3;
            jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[3 + idx].rect, left + col * (keyW + gap), top + row * (keyH + gap), keyW, keyH);
            jkGuiXboxKeyboard_elements[3 + idx].bIsVisible = 1;
            jkGuiXboxKeyboard_elements[3 + idx].clickHandlerFunc = jkGuiXboxKeyboard_ButtonClicked;
        }
    }

    idx = 3 + XKB_NUM_KEYS;
    jkGuiXboxKeyboard_elements[idx + 0].type = ELEMENT_TEXTBUTTON;
    jkGuiXboxKeyboard_elements[idx + 0].hoverId = XKB_CMD_SHIFT;
    jkGuiXboxKeyboard_elements[idx + 0].textType = 2;
    jkGuiXboxKeyboard_elements[idx + 0].wstr = jkGuiXboxKeyboard_shiftLabel;
    jkGuiXboxKeyboard_elements[idx + 0].selectedTextEntry = 3;
    jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[idx + 0].rect, 50, 330, 115, 40);
    jkGuiXboxKeyboard_elements[idx + 0].bIsVisible = 1;
    jkGuiXboxKeyboard_elements[idx + 0].clickHandlerFunc = jkGuiXboxKeyboard_ButtonClicked;

    jkGuiXboxKeyboard_elements[idx + 1].type = ELEMENT_TEXTBUTTON;
    jkGuiXboxKeyboard_elements[idx + 1].hoverId = XKB_CMD_SPACE;
    jkGuiXboxKeyboard_elements[idx + 1].textType = 2;
    jkGuiXboxKeyboard_elements[idx + 1].wstr = jkGuiXboxKeyboard_spaceLabel;
    jkGuiXboxKeyboard_elements[idx + 1].selectedTextEntry = 3;
    jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[idx + 1].rect, 170, 330, 120, 40);
    jkGuiXboxKeyboard_elements[idx + 1].bIsVisible = 1;
    jkGuiXboxKeyboard_elements[idx + 1].clickHandlerFunc = jkGuiXboxKeyboard_ButtonClicked;

    jkGuiXboxKeyboard_elements[idx + 2].type = ELEMENT_TEXTBUTTON;
    jkGuiXboxKeyboard_elements[idx + 2].hoverId = XKB_CMD_BACKSPACE;
    jkGuiXboxKeyboard_elements[idx + 2].textType = 2;
    jkGuiXboxKeyboard_elements[idx + 2].wstr = jkGuiXboxKeyboard_backLabel;
    jkGuiXboxKeyboard_elements[idx + 2].selectedTextEntry = 3;
    jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[idx + 2].rect, 295, 330, 110, 40);
    jkGuiXboxKeyboard_elements[idx + 2].bIsVisible = 1;
    jkGuiXboxKeyboard_elements[idx + 2].clickHandlerFunc = jkGuiXboxKeyboard_ButtonClicked;

    jkGuiXboxKeyboard_elements[idx + 3].type = ELEMENT_TEXTBUTTON;
    jkGuiXboxKeyboard_elements[idx + 3].hoverId = XKB_CMD_CANCEL;
    jkGuiXboxKeyboard_elements[idx + 3].textType = 2;
    jkGuiXboxKeyboard_elements[idx + 3].wstr = jkGuiXboxKeyboard_cancelLabel;
    jkGuiXboxKeyboard_elements[idx + 3].selectedTextEntry = 3;
    jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[idx + 3].rect, 0, 430, 200, 40);
    jkGuiXboxKeyboard_elements[idx + 3].bIsVisible = 1;
    jkGuiXboxKeyboard_elements[idx + 3].clickHandlerFunc = jkGuiXboxKeyboard_ButtonClicked;

    jkGuiXboxKeyboard_elements[idx + 4].type = ELEMENT_TEXTBUTTON;
    jkGuiXboxKeyboard_elements[idx + 4].hoverId = XKB_CMD_DONE;
    jkGuiXboxKeyboard_elements[idx + 4].textType = 2;
    jkGuiXboxKeyboard_elements[idx + 4].wstr = jkGuiXboxKeyboard_doneLabel;
    jkGuiXboxKeyboard_elements[idx + 4].selectedTextEntry = 3;
    jkGuiXboxKeyboard_SetRect(&jkGuiXboxKeyboard_elements[idx + 4].rect, 440, 430, 200, 40);
    jkGuiXboxKeyboard_elements[idx + 4].bIsVisible = 1;
    jkGuiXboxKeyboard_elements[idx + 4].clickHandlerFunc = jkGuiXboxKeyboard_ButtonClicked;

    jkGuiXboxKeyboard_elements[idx + 5].type = ELEMENT_END;

    jkGuiXboxKeyboard_menu.idkFunc = jkGuiXboxKeyboard_Tick;
    jkGui_InitMenu(&jkGuiXboxKeyboard_menu, jkGui_stdBitmaps[JKGUI_BM_BK_SETUP]);
    jkGuiXboxKeyboard_RestoreWideLabels();
    jkGuiXboxKeyboard_initted = 1;
}

void jkGuiXboxKeyboard_Startup(void)
{
    if (!jkGuiXboxKeyboard_initted)
        jkGuiXboxKeyboard_InitElements();
}

int jkGuiXboxKeyboard_Show(wchar_t *text, int maxChars, const wchar_t *title)
{
    int ret;
    if (!text || maxChars <= 1)
        return -1;

    jkGuiXboxKeyboard_Startup();
    jkGuiXboxKeyboard_target = text;
    jkGuiXboxKeyboard_targetMax = maxChars;
    jkGuiXboxKeyboard_caps = 1;
    jkGuiXboxKeyboard_RebuildKeyLabels();
    jkGuiXboxKeyboard_RestoreWideLabels();

    _wcsncpy(jkGuiXboxKeyboard_title, title ? title : L"Enter Text", 63);
    jkGuiXboxKeyboard_title[63] = 0;
    jkGuiXboxKeyboard_UpdateText(NULL);

    jkGuiXboxKeyboard_menu.lastClicked = 0;
    jkGuiXboxKeyboard_menu.focusedElement = NULL;
    jkGuiXboxKeyboard_menu.lastMouseDownClickable = NULL;
    jkGuiXboxKeyboard_menu.lastMouseOverClickable = &jkGuiXboxKeyboard_elements[3];
    jkGuiRend_MenuSetReturnKeyShortcutElement(&jkGuiXboxKeyboard_menu, NULL);
    jkGuiRend_MenuSetEscapeKeyShortcutElement(&jkGuiXboxKeyboard_menu, &jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 3]);
    jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 3].bIsVisible = 0;
    jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 4].bIsVisible = 0;
    jkGuiRend_XboxFooterBegin(&jkGuiXboxKeyboard_menu);
    jkGuiRend_XboxFooterAddAction(&jkGuiXboxKeyboard_menu, JKGUI_XBOX_BTN_A, 0, L"Select");
    jkGuiRend_XboxFooterAddAction(&jkGuiXboxKeyboard_menu, JKGUI_XBOX_BTN_B, -1, L"Cancel");
    jkGuiRend_XboxFooterAddElementAction(&jkGuiXboxKeyboard_menu, JKGUI_XBOX_BTN_X, &jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 2], L"Back");
    jkGuiRend_XboxFooterAddElementAction(&jkGuiXboxKeyboard_menu, JKGUI_XBOX_BTN_Y, &jkGuiXboxKeyboard_elements[3 + XKB_NUM_KEYS + 1], L"Space");
    jkGuiRend_XboxFooterAddAction(&jkGuiXboxKeyboard_menu, JKGUI_XBOX_BTN_START, 1, L"Done");
    jkGuiRend_XboxSetInitialFocus(&jkGuiXboxKeyboard_menu, &jkGuiXboxKeyboard_elements[3]);
    jkGuiXboxKeyboard_ResetEdges();

    ret = jkGuiRend_DisplayAndReturnClicked(&jkGuiXboxKeyboard_menu);
    jkGuiXboxKeyboard_target = NULL;
    jkGuiXboxKeyboard_targetMax = 0;
    return ret == 1 ? 1 : -1;
}

int jkGuiXboxKeyboard_TextBoxClicked(jkGuiElement *element, jkGuiMenu *menu, int32_t mouseX, int32_t mouseY, BOOL redraw)
{
    wchar_t *text;
    int maxChars;
    if (!element)
        return 0;

    text = (wchar_t *)element->wstr;
    maxChars = element->selectedTextEntry;
    if (text && maxChars > 1)
    {
        jkGuiXboxKeyboard_Show(text, maxChars, jkStrings_GetUniStringWithFallback("GUI_NAME"));
        if (menu)
        {
            element->texInfo.textHeight = _wcslen(text);
            jkGuiRend_Paint(menu);
        }
    }
    return 0;
}

#else

int jkGuiXboxKeyboard_Show(wchar_t *text, int maxChars, const wchar_t *title) { return -1; }
int jkGuiXboxKeyboard_TextBoxClicked(jkGuiElement *element, jkGuiMenu *menu, int32_t mouseX, int32_t mouseY, BOOL redraw) { return 0; }
void jkGuiXboxKeyboard_Startup(void) { }

#endif
