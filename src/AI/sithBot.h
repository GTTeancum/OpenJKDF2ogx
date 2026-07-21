#ifndef _SITHBOT_H
#define _SITHBOT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void sithBot_TickAll(flex_t deltaSeconds, int deltaMs);
void sithBot_LogScoreboard(const char *reason);

#ifdef __cplusplus
}
#endif

#endif
