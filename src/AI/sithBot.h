#ifndef _SITHBOT_H
#define _SITHBOT_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum SithBotNavPhase
{
    SITHBOT_NAV_CHECKING_CACHE = 0,
    SITHBOT_NAV_ANALYZING_MAP,
    SITHBOT_NAV_CONNECTING_ROUTES,
    SITHBOT_NAV_SAVING_CACHE,
    SITHBOT_NAV_READY_FROM_CACHE,
    SITHBOT_NAV_READY_GENERATED
} SithBotNavPhase;

typedef void (*SithBotNavStatusCallback)(SithBotNavPhase phase);

void sithBot_TickAll(flex_t deltaSeconds, int deltaMs);
void sithBot_PrepareNavigation(SithBotNavStatusCallback statusCallback);
void sithBot_LogScoreboard(const char *reason);
void sithBot_ClearActiveBotInvulnerability(void);
int sithBot_ShouldSuppressDamage(sithThing *victim, sithThing *damager, flex_t amount, int damageClass);
void sithBot_LogDamageEvent(sithThing *victim, sithThing *damager, flex_t amount, int damageClass);
void sithBot_LogDeathEvent(sithPlayerInfo *playerInfo, sithThing *killedThing, sithThing *killedByThing);

#ifdef __cplusplus
}
#endif

#endif
