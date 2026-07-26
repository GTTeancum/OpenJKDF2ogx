#ifndef _SITHBOT_H
#define _SITHBOT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

void sithBot_TickAll(flex_t deltaSeconds, int deltaMs);
void sithBot_PrepareNavigation(void);
void sithBot_LogScoreboard(const char *reason);
void sithBot_ClearActiveBotInvulnerability(void);
int sithBot_ShouldSuppressDamage(sithThing *victim, sithThing *damager, flex_t amount, int damageClass);
void sithBot_LogDamageEvent(sithThing *victim, sithThing *damager, flex_t amount, int damageClass);
void sithBot_LogDeathEvent(sithPlayerInfo *playerInfo, sithThing *killedThing, sithThing *killedByThing);

#ifdef __cplusplus
}
#endif

#endif
