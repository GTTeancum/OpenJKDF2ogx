#ifndef _JKGUI_XBOX_KEYBOARD_H
#define _JKGUI_XBOX_KEYBOARD_H

#include "types.h"

int jkGuiXboxKeyboard_Show(wchar_t *text, int maxChars, const wchar_t *title);
int jkGuiXboxKeyboard_TextBoxClicked(jkGuiElement *element, jkGuiMenu *menu, int32_t mouseX, int32_t mouseY, BOOL redraw);
void jkGuiXboxKeyboard_Startup(void);

#endif // _JKGUI_XBOX_KEYBOARD_H
