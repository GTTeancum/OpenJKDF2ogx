#ifndef JKGUIXBOXCONTROLS_H
#define JKGUIXBOXCONTROLS_H
#include "types.h"
#ifdef TARGET_XBOX
void jkGuiXboxControls_Startup(void);
void jkGuiXboxControls_Shutdown(void);
int jkGuiXboxControls_Show(void);
int jkGuiXboxControls_IsMenu(jkGuiMenu *menu);
int jkGuiXboxControls_Focus(jkGuiMenu *menu, int direction);
void jkGuiXboxControls_ProbeMenu(jkGuiMenu *menu);
#endif
#endif
