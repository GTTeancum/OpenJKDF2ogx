#ifndef _XBOX_SPLITSCREEN_H
#define _XBOX_SPLITSCREEN_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XBOX_SPLITSCREEN_MAX_LOCAL_PLAYERS 4

int  xboxSplitScreen_IsEnabled(void);
int  xboxSplitScreen_GetLocalPlayerCount(void);
int  xboxSplitScreen_GetRequestedLocalPlayerCount(void);
void xboxSplitScreen_Enable(void);
void xboxSplitScreen_SetRequestedLocalPlayerCount(int count);
void xboxSplitScreen_SetPendingMpc(int slot, const wchar_t *name);
void xboxSplitScreen_OnMultiplayerServerStarted(void);
void xboxSplitScreen_Disable(void);
void xboxSplitScreen_BeginControlFrame(void);
void xboxSplitScreen_TickControls(float deltaSecs, int deltaMs);
void xboxSplitScreen_EndControlFrame(void);
void xboxSplitScreen_SetContextForLocalSlot(int slot);
void xboxSplitScreen_RestoreContext(void);
void xboxSplitScreen_GetViewport(int slot, int *x, int *y, int *w, int *h);
void xboxSplitScreen_ApplyViewport(int slot);
void xboxSplitScreen_ResetViewport(void);
int  xboxSplitScreen_RenderGameplayFrame(void);

#ifdef __cplusplus
}
#endif

#endif
