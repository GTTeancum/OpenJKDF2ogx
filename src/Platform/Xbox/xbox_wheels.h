#ifndef _XBOX_WHEELS_H
#define _XBOX_WHEELS_H

#include "../../types.h"
#include "../../General/stdFont.h"

#ifdef __cplusplus
extern "C" {
#endif

void xbox_wheels_UpdateInput(int port, unsigned int tick, int gameplay,
                             int blackDown, int whiteDown, int yDown, int bDown,
                             float rightX, float rightY);
int xbox_wheels_IsOpenForPort(int port);
int xbox_wheels_IsAnyOpen(void);
int xbox_wheels_ShouldSuppressLook(int port);
int xbox_wheels_ActivateInventoryBin(int binIdx);
void xbox_wheels_Draw(stdFont *font);

#ifdef __cplusplus
}
#endif

#endif
