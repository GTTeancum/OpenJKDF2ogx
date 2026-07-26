#include "AI/sithBot.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "Cog/sithCog.h"
#include "Cog/sithCogParse.h"
#include "Dss/jkDSS.h"
#include "Dss/sithDSSThing.h"
#include "Dss/sithMulti.h"
#include "Engine/sithCamera.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"
#include "Engine/sithPuppet.h"
#include "Engine/rdThing.h"
#include "General/stdConffile.h"
#include "General/stdMath.h"
#include "Gameplay/jkSaber.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithPlayerActions.h"
#include "Gameplay/sithTime.h"
#include "Main/Main.h"
#include "Primitives/rdMatrix.h"
#include "Primitives/rdVector.h"
#include "World/jkPlayer.h"
#include "World/sithActor.h"
#include "World/sithItem.h"
#include "World/sithModel.h"
#include "World/sithSoundClass.h"
#include "World/sithSurface.h"
#include "World/sithTemplate.h"
#include "World/sithThing.h"
#include "World/sithWeapon.h"
#include "World/sithWorld.h"
#include "jk.h"
#include "stdPlatform.h"

#ifdef TARGET_XBOX
#include "Platform/Xbox/xbox_debug.h"
#include "Platform/Xbox/xbox_splitscreen.h"
#endif

void sithCogFunctionSector_SetSectorThrust(sithCog *ctx);
void sithCogFunctionThing_ApplyForce(sithCog *ctx);

#define SITHBOT_MAX_BOTS 31
#define SITHBOT_MAX_NODES 512
#define SITHBOT_MAX_EDGES 24
#define SITHBOT_NETID_BASE 0x42000000
#define SITHBOT_RESPAWN_MS 3000
#define SITHBOT_STUCK_MS 600
#define SITHBOT_PICKUP_CHECK_MS 180
#define SITHBOT_PICKUP_RADIUS 1.15
#define SITHBOT_LINK_RADIUS_SQ 144.0
#define SITHBOT_LINK_CANDIDATES 40
#define SITHBOT_MAX_DYNAMIC_HAZARDS 32
#define SITHBOT_MAX_BLOCKED_EDGES 128
#define SITHBOT_MAX_INFERRED_LIFTS 64
#define SITHBOT_MAX_PORTAL_NODES 160
#define SITHBOT_MAX_PATH_LIFTS 64
#define SITHBOT_MAX_LIFT_FRAMES 8
#define SITHBOT_DYNAMIC_HAZARD_MS 45000
#define SITHBOT_MIN_MAXVEL 3.0
#define SITHBOT_MIN_MAXTHRUST 4.0
#define SITHBOT_FORCE_MANA_MAX 400.0
#define SITHBOT_FORCE_HEAL_COST 200.0
#define SITHBOT_FORCE_LIGHTNING_COST 35.0
#define SITHBOT_FORCE_PUSH_COST 20.0
#define SITHBOT_ARM_REJECT_SLOTS 4
#define SITHBOT_ROUTE_COMMIT_MS 2600
#define SITHBOT_ROUTE_WATCH_MS 1100
#define SITHBOT_INTERACTION_WAIT_MS 600
#define SITHBOT_INTERACTION_REPEAT_MS 1800
#define SITHBOT_TACTICAL_MOVE_MIN_MS 1600
#define SITHBOT_TACTICAL_MOVE_MAX_MS 2600
#define SITHBOT_TARGET_COMMIT_MS 1600
#define SITHBOT_HUNT_REPLAN_MS 2800
#define SITHBOT_LAST_SEEN_MS 8000
#define SITHBOT_COMBAT_SEPARATION_MIN 1.15
#define SITHBOT_COMBAT_HOLD_STRAFE 0.82
#define SITHBOT_BNAV_MAGIC 0x56414E42u
#define SITHBOT_BNAV_VERSION 14u

typedef enum SithBotNodeKind
{
    SITHBOT_NODE_SPAWN,
    SITHBOT_NODE_ITEM,
    SITHBOT_NODE_FLOOR,
    SITHBOT_NODE_LIFT,
    SITHBOT_NODE_PORTAL,
    SITHBOT_NODE_JUMPPAD
} SithBotNodeKind;

typedef enum SithBotGoalMode
{
    SITHBOT_GOAL_ROAM,
    SITHBOT_GOAL_HUNT,
    SITHBOT_GOAL_ESCAPE,
    SITHBOT_GOAL_ARM,
    SITHBOT_GOAL_TACTICAL_ITEM
} SithBotGoalMode;

typedef enum SithBotCombatMode
{
    SITHBOT_COMBAT_NONE,
    SITHBOT_COMBAT_CHARGE,
    SITHBOT_COMBAT_TACTICAL,
    SITHBOT_COMBAT_RETREAT,
    SITHBOT_COMBAT_HOLD,
    SITHBOT_COMBAT_HUNT
} SithBotCombatMode;

typedef struct SithBotNode
{
    rdVector3 pos;
    sithSector *sector;
    int thingIdx;
    int pathFrame;
    int kind;
    int edgeCount;
    int edges[SITHBOT_MAX_EDGES];
} SithBotNode;

typedef struct SithBotNavFileHeader
{
    uint32_t magic;
    uint32_t version;
    uint32_t worldHash;
    uint32_t nodeCount;
    int32_t numSectors;
    int32_t numThings;
    int32_t numVertices;
    int32_t numSurfaces;
} SithBotNavFileHeader;

typedef struct SithBotNavFileNode
{
    float pos[3];
    int32_t sectorIdx;
    int32_t thingIdx;
    int32_t pathFrame;
    int32_t kind;
    int32_t edgeCount;
    int32_t edges[SITHBOT_MAX_EDGES];
} SithBotNavFileNode;

typedef struct SithBotLinkCandidate
{
    int nodeIdx;
    flex_t distSq;
} SithBotLinkCandidate;

typedef struct SithBotWeaponSpec
{
    int jkBin;
    int motsBin;
    int ammoBin;
    flex_t ammoCost;
    const char *projectileName;
    flex_t fireWaitMs;
    flex_t minDist;
    flex_t idealDist;
    flex_t maxDist;
    rdVector3 fireOffset;
    flex_t spreadDeg;
    int mode;
    int scaleFlags;
    flex_t autoAimFov;
    flex_t autoAimMaxDist;
    flex_t selfSafeDist;
    flex_t score;
} SithBotWeaponSpec;

typedef struct SithBotDynamicHazard
{
    sithSector *sector;
    rdVector3 pos;
    uint32_t avoidUntilMs;
    flex_t severity;
} SithBotDynamicHazard;

typedef struct SithBotBlockedEdge
{
    int fromNode;
    int toNode;
    uint32_t untilMs;
} SithBotBlockedEdge;

typedef struct SithBotState
{
    int active;
    int playerIdx;
    int goalNode;
    int nextNode;
    int enemyIdx;
    uint32_t nextGoalMs;
    uint32_t nextFireMs;
    uint32_t nextUseMs;
    uint32_t nextSyncMs;
    uint32_t nextPickupMs;
    uint32_t nextMoveLogMs;
    uint32_t nextProgressLogMs;
    uint32_t nextCombatLogMs;
    uint32_t nextForceMs;
    uint32_t nextForceRegenMs;
    uint32_t nextTacticalPickupMs;
    uint32_t explosiveBackoffUntilMs;
    uint32_t hazardFleeUntilMs;
    uint32_t respawnAtMs;
    uint32_t lastMoveCheckMs;
    uint32_t blockedSinceMs;
    uint32_t routeCommitUntilMs;
    uint32_t interactionWaitUntilMs;
    uint32_t interactionRepeatUntilMs;
    uint32_t nextLiftLogMs;
    uint32_t lastEnemySeenMs;
    uint32_t combatMoveUntilMs;
    uint32_t jumpPadAirUntilMs;
    uint32_t jumpPadDodgeUntilMs;
    uint32_t routeWatchStartMs;
    rdVector3 lastMovePos;
    rdVector3 lastEnemySeenPos;
    rdVector3 combatMoveTarget;
    rdVector3 steeringDir;
    rdVector3 hazardPos;
    rdVector3 safeAnchorPos;
    rdVector3 jumpPadLandingPos;
    rdVector3 routeWatchPos;
    sithSector *lastEnemySeenSector;
    sithSector *hazardSector;
    sithSector *safeAnchorSector;
    sithSector *envDamageSector;
    uint32_t nextSafeAnchorMs;
    uint32_t nextFallRecoveryMs;
    uint32_t envDamageWindowMs;
    flex_t envDamageAccum;
    flex_t armBestDist;
    flex_t routeBestDist;
    flex_t frameDeltaSeconds;
    int stuckTicks;
    int blockedMoveTicks;
    int goalMode;
    int routeGoalNode;
    int lastInteractionSurfaceIdx;
    int lastInteractionThingIdx;
    int combatStrafeSign;
    int lastSeenEnemyIdx;
    int combatTargetIdx;
    int combatMode;
    int combatHasMoveTarget;
    uint32_t steeringUntilMs;
    int steeringTargetNode;
    int steeringCombat;
    int ridingLiftThingIdx;
    int ridingLiftTargetNode;
    int jumpPadLaunchNode;
    int jumpPadTargetNode;
    int jumpPadDodgeSign;
    int routeWatchGoal;
    int routeFailureGoal;
    int routeFailureCount;
    int routeHistoryGoal;
    int routeLastNode;
    int routePriorNode;
    int routeFlipCount;
    flex_t jumpPadLaunchZ;
    int armRejectThingIdx[SITHBOT_ARM_REJECT_SLOTS];
    uint32_t armRejectUntilMs[SITHBOT_ARM_REJECT_SLOTS];
} SithBotState;

static SithBotState sithBot_bots[SITHBOT_MAX_BOTS];
static SithBotNode sithBot_nodes[SITHBOT_MAX_NODES];
static int sithBot_numNodes;
static sithWorld *sithBot_navWorld;
static int sithBot_navBuilt;
static int sithBot_spawnedForWorld;
static uint32_t sithBot_matchStartMs;
static int sithBot_scoreLogged;
static int sithBot_debugShotsLogged;
static int sithBot_debugHitsLogged;
static int sithBot_debugHuntsLogged;
static int sithBot_debugFireFailuresLogged;
static int sithBot_debugPickupsLogged;
static int sithBot_debugMovesLogged;
static int sithBot_debugProgressLogged;
static int sithBot_debugJumpsLogged;
static int sithBot_debugRouteNudgesLogged;
static int sithBot_debugCombatMovesLogged;
static int sithBot_debugDamageLogged;
static int sithBot_debugDeathsLogged;
static int sithBot_debugHazardsLogged;
static int sithBot_debugHazardMovesLogged;
static int sithBot_debugLedgeAvoidLogged;
static int sithBot_debugDynamicHazardsLogged;
static int sithBot_debugArmGoalsLogged;
static int sithBot_debugArmRejectsLogged;
static int sithBot_debugTacticalPickupsLogged;
static int sithBot_debugNoLosFireLogged;
static int sithBot_debugForceLogged;
static int sithBot_debugFallRecoveriesLogged;
static int sithBot_debugUsesLogged;
static int sithBot_debugLiftsLogged;
static int sithBot_debugJumpPadsLogged;
static int sithBot_debugTickSkipsLogged;
static int sithBot_qualityJumpDetected;
static int sithBot_qualityJumpLanded;
static int sithBot_qualityJumpRetry;
static int sithBot_qualityJumpFailed;
static int sithBot_qualityJumpTimeout;
static int sithBot_qualityRouteNudges;
static int sithBot_qualityRouteStalls;
static int sithBot_qualityNoLosFireAttempts;
static int sithBot_qualityWeaponShots[SITHBIN_NUMBINS];
static int sithBot_cameraPlayer;
static SithBotDynamicHazard sithBot_dynamicHazards[SITHBOT_MAX_DYNAMIC_HAZARDS];
static SithBotBlockedEdge sithBot_blockedEdges[SITHBOT_MAX_BLOCKED_EDGES];
static sithSector *sithBot_inferredLiftSectors[SITHBOT_MAX_INFERRED_LIFTS];
static int sithBot_numInferredLiftSectors;

static void sithBot_Logf(const char *fmt, ...);
static int sithBot_IsBlastWeaponSpec(const SithBotWeaponSpec *spec);
static int sithBot_BotStateForPlayer(int playerIdx);
static int sithBot_IsAutostartServerPlaceholder(int playerIdx);
static int sithBot_IsThingAlivePlayer(sithThing *thing);
static int sithBot_HasCombatLos(sithThing *from, sithThing *to);
static int sithBot_IsRiskyNavSectorForBot(sithSector *sector);
static int sithBot_IsCollisionSpikeSectorForBot(sithSector *sector);
static int sithBot_IsDynamicHazardSector(sithSector *sector);
static int sithBot_IsItemAvailable(sithThing *item);
static int sithBot_EmergencyMoveOutOfHazard(int victimSlot, sithThing *thing);
static int sithBot_PositionHasWalkableFootprint(sithThing *probeThing, sithSector *sector, const rdVector3 *pos, const rdVector3 *flatDir, flex_t stepHeight);
static void sithBot_ApplyWeaponPresentation(sithThing *thing, int weaponBin);
static void sithBot_SyncPositionIfNeeded(SithBotState *state, sithThing *thing);
static void sithBot_MoveToward(SithBotState *state, sithThing *thing, const rdVector3 *target, int combat);
static void sithBot_FaceToward(SithBotState *state, sithThing *thing, const rdVector3 *target, int combat);
static int sithBot_IsDirectDestinationSafe(sithThing *thing, const rdVector3 *destination);
static int sithBot_CogControlsThing(sithCog *cog, sithThing *thing);
static int sithBot_CogHandlesMessage(sithCog *cog, int message);
static int sithBot_GetSurfaceCenter(sithSurface *surface, rdVector3 *center);
static int sithBot_HandleJumpPadRoute(SithBotState *state, sithThing *thing, int startNode, int nextNode);
static int sithBot_DetectJumpPadLaunch(SithBotState *state, sithThing *thing);
static void sithBot_AdjustMoveDirForPlayers(SithBotState *state, sithThing *thing, rdVector3 *flatDir);

static const SithBotWeaponSpec sithBot_weaponSpecs[] =
{
    /* MotS-only entries mirror the primary-fire values in its stock weapon COGs. */
    { SITHBIN_CONCUSSION_RIFLE,      SITHBIN_MOTS_CONCUSSION_RIFLE,      SITHBIN_POWER,       8.0, "+concbullet",   1350.0, 3.0,  5.0, 18.0, { 0.0200, 0.1500, 0.0000 }, 0.6, SITH_ANIM_FIRE, 0x60, 5.0, 18.0, 3.0, 960.0 },
    { SITHBIN_RAIL_DETONATOR,        SITHBIN_MOTS_RAIL_DETONATOR,        SITHBIN_RAILCHARGES, 1.0, "+raildet2",     1050.0, 3.6,  5.0, 15.0, { 0.0214, 0.1500, 0.0000 }, 0.4, SITH_ANIM_FIRE, 0x60, 5.0, 15.0, 3.6, 930.0 },
    { SITHBIN_TUSKEN_PROD,           SITHBIN_MOTS_TUSKEN_PROD,           SITHBIN_POWER,       2.0, "+crossbowbolt3", 600.0, 2.3,  7.0, 24.0, { 0.0207, 0.0888, 0.0000 }, 0.8, SITH_ANIM_FIRE, 0x20, 5.0, 20.0, 2.3, 860.0 },
    { SITHBIN_REPEATER,              SITHBIN_MOTS_REPEATER,              SITHBIN_POWER,       1.0, "+repeaterball",  360.0, 1.2,  6.0, 22.0, { 0.0170, 0.1500, 0.0000 }, 1.2, SITH_ANIM_FIRE, 0x70, 5.0, 22.0, 1.2, 820.0 },
    { -1,                            SITHBIN_MOTS_STORMTROOPER_SCOPE,     SITHBIN_ENERGY,      4.0, "+sclaser",       500.0, 1.5, 11.0, 30.0, { 0.0000, 0.0000, 0.0350 }, 0.3, SITH_ANIM_FIRE, 0x00, 0.1, 30.0, 0.0, 780.0 },
    { SITHBIN_STORMTROOPER_RIFLE,    SITHBIN_MOTS_STORMTROOPER_RIFLE,    SITHBIN_ENERGY,      2.0, "+stlaser",       430.0, 0.8,  7.0, 25.0, { 0.0168, 0.1896, 0.0000 }, 1.0, SITH_ANIM_FIRE, 0x70, 5.0, 20.0, 0.0, 700.0 },
    { -1,                            SITHBIN_MOTS_BLASTECH,               SITHBIN_ENERGY,      1.0, "+bryarbolt",     500.0, 0.0,  5.5, 20.0, { 0.0135, 0.1624, 0.0000 }, 0.8, SITH_ANIM_FIRE, 0x20, 5.0, 20.0, 0.0, 660.0 },
    { SITHBIN_BRYARPISTOL,           SITHBIN_MOTS_BRYARPISTOL,           SITHBIN_ENERGY,      1.0, "+bryarbolt",     500.0, 0.0,  4.5, 18.0, { 0.0135, 0.1624, 0.0000 }, 0.8, SITH_ANIM_FIRE, 0x60, 5.0, 16.0, 0.0, 500.0 }
};

static int sithBot_IsBotNetId(int netId)
{
    return netId >= SITHBOT_NETID_BASE && netId < SITHBOT_NETID_BASE + 64;
}

static int sithBot_GetPlayerSlotForThing(sithThing *thing)
{
    int i;

    if (!thing)
        return -1;

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        if (jkPlayer_playerInfos[i].playerThing == thing)
            return i;
    }

    return -1;
}

static const char *sithBot_GetThingDebugName(sithThing *thing)
{
    if (!thing)
        return "";
    if (thing->templateBase && thing->templateBase->template_name[0])
        return thing->templateBase->template_name;
    if (thing->template_name[0])
        return thing->template_name;
    return "";
}

static const char *sithBot_GetDamageClassName(int damageClass)
{
    if (damageClass & SITH_DAMAGE_FALL)
        return "fall";
    if (damageClass & SITH_DAMAGE_DROWN)
        return "drown";
    if (damageClass & SITH_DAMAGE_SABER)
        return "saber";
    if (damageClass & SITH_DAMAGE_FORCE)
        return "force";
    if (damageClass & SITH_DAMAGE_FIRE)
        return "fire";
    if (damageClass & SITH_DAMAGE_ENERGY)
        return "energy";
    if (damageClass & SITH_DAMAGE_IMPACT)
        return "impact";
    return "unknown";
}

static int sithBot_GetSectorIndex(sithSector *sector)
{
    if (!sithWorld_pCurrentWorld || !sector || !sithWorld_pCurrentWorld->sectors)
        return -1;
    if (sector < sithWorld_pCurrentWorld->sectors ||
        sector >= sithWorld_pCurrentWorld->sectors + sithWorld_pCurrentWorld->numSectors)
        return -1;
    return (int)(sector - sithWorld_pCurrentWorld->sectors);
}

static void sithBot_AddDynamicHazardSector(sithSector *sector, const rdVector3 *pos, flex_t amount, const char *reason)
{
    int i;
    int best = -1;
    flex_t bestSeverity = 3.4e38f;

    if (!sector || amount <= 0.20)
        return;

    for (i = 0; i < SITHBOT_MAX_DYNAMIC_HAZARDS; i++)
    {
        if (sithBot_dynamicHazards[i].sector == sector)
        {
            best = i;
            break;
        }
        if (!sithBot_dynamicHazards[i].sector || sithBot_dynamicHazards[i].avoidUntilMs <= sithTime_curMs)
        {
            best = i;
            break;
        }
        if (sithBot_dynamicHazards[i].severity < bestSeverity)
        {
            bestSeverity = sithBot_dynamicHazards[i].severity;
            best = i;
        }
    }

    if (best < 0)
        return;

    sithBot_dynamicHazards[best].sector = sector;
    sithBot_dynamicHazards[best].avoidUntilMs = sithTime_curMs + SITHBOT_DYNAMIC_HAZARD_MS;
    sithBot_dynamicHazards[best].severity = amount;
    if (pos)
        rdVector_Copy3(&sithBot_dynamicHazards[best].pos, pos);
    else
        rdVector_Copy3(&sithBot_dynamicHazards[best].pos, &sector->center);

    if (sithBot_debugDynamicHazardsLogged < 24)
    {
        sithBot_Logf("BotMatch: learned-hazard sector=%d flags=%X amount=%.2f reason=%s pos=(%.2f,%.2f,%.2f)\n",
                     sithBot_GetSectorIndex(sector),
                     (unsigned int)sector->flags,
                     amount,
                     reason ? reason : "",
                     sithBot_dynamicHazards[best].pos.x,
                     sithBot_dynamicHazards[best].pos.y,
                     sithBot_dynamicHazards[best].pos.z);
        sithBot_debugDynamicHazardsLogged++;
    }
}

static int sithBot_IsDynamicHazardSector(sithSector *sector)
{
    int i;

    if (!sector)
        return 1;

    for (i = 0; i < SITHBOT_MAX_DYNAMIC_HAZARDS; i++)
    {
        if (sithBot_dynamicHazards[i].sector != sector)
            continue;
        if (sithBot_dynamicHazards[i].avoidUntilMs > sithTime_curMs)
            return 1;
        sithBot_dynamicHazards[i].sector = 0;
        sithBot_dynamicHazards[i].avoidUntilMs = 0;
        sithBot_dynamicHazards[i].severity = 0.0;
    }

    return 0;
}

static int sithBot_ShouldFleeSelfDamage(sithThing *victim, sithThing *damager, flex_t amount, int damageClass)
{
    if (!victim || !damager || amount <= 0.05)
        return 0;
    if (damager != victim)
        return 0;
    if (amount < 6.0)
        return 0;
    if (damageClass & (SITH_DAMAGE_FALL | SITH_DAMAGE_DROWN | SITH_DAMAGE_SABER))
        return 0;
    return (damageClass & (SITH_DAMAGE_ENERGY | SITH_DAMAGE_FIRE | SITH_DAMAGE_FORCE | SITH_DAMAGE_IMPACT)) != 0;
}

static int sithBot_IsEnvironmentalDamageSource(sithThing *victim, sithThing *damager, int ownerSlot, int damageClass)
{
    if (!victim)
        return 0;
    if (ownerSlot >= 0)
        return 0;
    if (damageClass & (SITH_DAMAGE_FALL | SITH_DAMAGE_SABER))
        return 0;
    if (damager && damager == victim->attachedThing &&
        (victim->attach_flags & (SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)) != 0 &&
        damager->moveType == SITH_MT_PATH && (damager->thingflags & SITH_TF_STANDABLE) != 0)
    {
        return 0;
    }
    if (damager && (damager->type == SITH_THING_PLAYER || damager->type == SITH_THING_WEAPON || damager->type == SITH_THING_EXPLOSION))
        return 0;

    return (damageClass & (SITH_DAMAGE_IMPACT | SITH_DAMAGE_ENERGY | SITH_DAMAGE_FIRE | SITH_DAMAGE_FORCE | SITH_DAMAGE_DROWN)) != 0;
}

static int sithBot_ShouldFleeEnvironmentalDamage(sithThing *victim, sithThing *damager, int ownerSlot, flex_t amount, int damageClass)
{
    if (!sithBot_IsEnvironmentalDamageSource(victim, damager, ownerSlot, damageClass))
        return 0;
    if (amount <= 0.20)
        return 0;
    if (amount < 6.0 && (damageClass & SITH_DAMAGE_DROWN) == 0)
        return 0;
    return 1;
}

static int sithBot_ShouldFleeAccumulatedEnvironmentalDamage(int victimSlot, sithThing *victim, sithThing *damager, int ownerSlot, flex_t amount, int damageClass)
{
    int stateIdx;
    SithBotState *state;

    if (victimSlot < 0 || !victim || !victim->sector || amount <= 0.20)
        return 0;
    if (!sithBot_IsEnvironmentalDamageSource(victim, damager, ownerSlot, damageClass))
        return 0;

    stateIdx = sithBot_BotStateForPlayer(victimSlot);
    if (stateIdx < 0)
        return 0;

    state = &sithBot_bots[stateIdx];
    if (state->envDamageSector != victim->sector || state->envDamageWindowMs <= sithTime_curMs)
    {
        state->envDamageSector = victim->sector;
        state->envDamageWindowMs = sithTime_curMs + 2200;
        state->envDamageAccum = 0.0;
    }

    state->envDamageAccum += amount;
    if (state->envDamageAccum >= 3.0 || (victim->actorParams.health < 18.0 && state->envDamageAccum >= 1.0))
        return 1;

    return 0;
}

static void sithBot_MarkHazardFlee(int victimSlot, sithThing *victim, flex_t amount, int damageClass)
{
    int stateIdx;
    SithBotState *state;

    if (victimSlot < 0 || !victim || !victim->sector)
        return;

    stateIdx = sithBot_BotStateForPlayer(victimSlot);
    if (stateIdx < 0)
        return;

    state = &sithBot_bots[stateIdx];
    state->hazardFleeUntilMs = sithTime_curMs + 3600;
    rdVector_Copy3(&state->hazardPos, &victim->position);
    state->hazardSector = victim->sector;
    state->goalNode = -1;
    state->nextNode = -1;
    state->nextGoalMs = 0;
    state->goalMode = SITHBOT_GOAL_ESCAPE;
    state->envDamageSector = 0;
    state->envDamageWindowMs = 0;
    state->envDamageAccum = 0.0;
    sithBot_AddDynamicHazardSector(victim->sector, &victim->position, amount, "damage");
    if (amount >= 40.0 || sithBot_IsCollisionSpikeSectorForBot(victim->sector))
        sithBot_EmergencyMoveOutOfHazard(victimSlot, victim);

    if (sithBot_debugHazardsLogged < 40)
    {
        sithBot_Logf("BotMatch: hazard-flee slot=%d class=%s amount=%.2f sectorFlags=%X pos=(%.2f,%.2f,%.2f)\n",
                     victimSlot,
                     sithBot_GetDamageClassName(damageClass),
                     amount,
                     victim->sector ? (unsigned int)victim->sector->flags : 0,
                     victim->position.x,
                     victim->position.y,
                     victim->position.z);
        sithBot_debugHazardsLogged++;
    }
}

void sithBot_LogDamageEvent(sithThing *victim, sithThing *damager, flex_t amount, int damageClass)
{
    sithThing *owner;
    int victimSlot;
    int ownerSlot;
    int victimIsBot;
    int ownerIsBot;
    flex_t healthBefore;
    flex_t healthAfter;
    int lethal;

    if (!sithNet_isMulti || !victim || victim->type != SITH_THING_PLAYER)
        return;

    owner = damager ? sithThing_GetParent(damager) : 0;
    if (!owner)
        owner = damager;

    victimSlot = sithBot_GetPlayerSlotForThing(victim);
    ownerSlot = sithBot_GetPlayerSlotForThing(owner);
    victimIsBot = victimSlot >= 0 && sithBot_IsBotNetId(jkPlayer_playerInfos[victimSlot].net_id);
    ownerIsBot = ownerSlot >= 0 && sithBot_IsBotNetId(jkPlayer_playerInfos[ownerSlot].net_id);
    if (!victimIsBot && !ownerIsBot)
        return;

    if (victimIsBot && ownerSlot >= 0 && ownerSlot != victimSlot &&
        sithBot_IsThingAlivePlayer(owner) &&
        !((sithNet_MultiModeFlags & MULTIMODEFLAG_TEAMS) &&
          sithPlayer_sub_4C9060(victim, owner)))
    {
        int stateIdx = sithBot_BotStateForPlayer(victimSlot);
        if (stateIdx >= 0)
        {
            SithBotState *state = &sithBot_bots[stateIdx];
            sithThing *current = state->enemyIdx >= 0 && state->enemyIdx < jkPlayer_maxPlayers
                ? jkPlayer_playerInfos[state->enemyIdx].playerThing
                : 0;
            int takeThreat = state->enemyIdx == ownerSlot ||
                !sithBot_IsThingAlivePlayer(current) ||
                !sithBot_HasCombatLos(victim, current) ||
                amount >= 20.0;

            if (takeThreat)
            {
                state->enemyIdx = ownerSlot;
                state->lastSeenEnemyIdx = ownerSlot;
                state->lastEnemySeenMs = sithTime_curMs;
                rdVector_Copy3(&state->lastEnemySeenPos, &owner->position);
                state->lastEnemySeenSector = owner->sector;
                if (state->combatTargetIdx != ownerSlot)
                {
                    state->combatTargetIdx = -1;
                    state->combatHasMoveTarget = 0;
                }
            }
        }
    }

    if (victimIsBot &&
        (sithBot_ShouldFleeSelfDamage(victim, damager, amount, damageClass) ||
         sithBot_ShouldFleeEnvironmentalDamage(victim, damager, ownerSlot, amount, damageClass) ||
         sithBot_ShouldFleeAccumulatedEnvironmentalDamage(victimSlot, victim, damager, ownerSlot, amount, damageClass)))
    {
        sithBot_MarkHazardFlee(victimSlot, victim, amount, damageClass);
    }

    healthBefore = victim->actorParams.health;
    healthAfter = healthBefore - amount;
    lethal = healthBefore >= 1.0 && healthAfter < 1.0;
    if (!lethal && sithBot_debugDamageLogged >= 96)
        return;

    sithBot_Logf("BotMatch: damage victimSlot=%d victimBot=%d damagerThing=%d damagerType=%d damager='%s' ownerSlot=%d ownerBot=%d class=%s amount=%.2f healthBefore=%.2f healthAfter=%.2f lethal=%d sectorFlags=%X pos=(%.2f,%.2f,%.2f)\n",
                 victimSlot,
                 victimIsBot,
                 damager ? damager->thingIdx : -1,
                 damager ? damager->type : -1,
                 sithBot_GetThingDebugName(damager),
                 ownerSlot,
                 ownerIsBot,
                 sithBot_GetDamageClassName(damageClass),
                 amount,
                 healthBefore,
                 healthAfter,
                 lethal,
                 victim->sector ? (unsigned int)victim->sector->flags : 0,
                 victim->position.x,
                 victim->position.y,
                 victim->position.z);
    sithBot_debugDamageLogged++;
}

int sithBot_ShouldSuppressDamage(sithThing *victim, sithThing *damager, flex_t amount, int damageClass)
{
    sithThing *owner;
    int victimSlot;
    int ownerSlot;

    (void)amount;
    (void)damageClass;
    if (!sithNet_isMulti || !victim || victim->type != SITH_THING_PLAYER)
        return 0;
    victimSlot = sithBot_GetPlayerSlotForThing(victim);
    if (victimSlot >= 0 && sithBot_IsAutostartServerPlaceholder(victimSlot))
        return 1;
    owner = damager ? sithThing_GetParent(damager) : 0;
    if (!owner)
        owner = damager;
    ownerSlot = sithBot_GetPlayerSlotForThing(owner);
    if (ownerSlot >= 0 && sithBot_IsAutostartServerPlaceholder(ownerSlot))
        return 1;
    return 0;
}

void sithBot_LogDeathEvent(sithPlayerInfo *playerInfo, sithThing *killedThing, sithThing *killedByThing)
{
    int victimSlot;
    int killerSlot;
    int victimIsBot;
    int killerIsBot;

    if (!sithNet_isMulti || !playerInfo || !killedThing)
        return;

    victimSlot = sithBot_GetPlayerSlotForThing(killedThing);
    killerSlot = sithBot_GetPlayerSlotForThing(killedByThing);
    victimIsBot = victimSlot >= 0 && sithBot_IsBotNetId(jkPlayer_playerInfos[victimSlot].net_id);
    killerIsBot = killerSlot >= 0 && sithBot_IsBotNetId(jkPlayer_playerInfos[killerSlot].net_id);
    if (!victimIsBot && !killerIsBot)
        return;
    if (sithBot_debugDeathsLogged >= 96)
        return;

    sithBot_Logf("BotMatch: death victimSlot=%d victimBot=%d killerSlot=%d killerBot=%d killedByThing=%d killedByType=%d killedBy='%s' countedAs=%s pos=(%.2f,%.2f,%.2f) sectorFlags=%X kills=%d deaths=%d suicides=%d\n",
                 victimSlot,
                 victimIsBot,
                 killerSlot,
                 killerIsBot,
                 killedByThing ? killedByThing->thingIdx : -1,
                 killedByThing ? killedByThing->type : -1,
                 sithBot_GetThingDebugName(killedByThing),
                 killedByThing && killedByThing != killedThing && killedByThing->type == SITH_THING_PLAYER ? "kill" : "suicide",
                 killedThing->position.x,
                 killedThing->position.y,
                 killedThing->position.z,
                 killedThing->sector ? (unsigned int)killedThing->sector->flags : 0,
                 playerInfo->numKills,
                 playerInfo->numKilled,
                 playerInfo->numSuicides);
    sithBot_debugDeathsLogged++;
}

static int sithBot_WStrEqualsAscii(const wchar_t *wstr, const char *ascii)
{
    if (!wstr || !ascii)
        return 0;

    while (*wstr && *ascii)
    {
        if ((unsigned int)*wstr != (unsigned char)*ascii)
            return 0;
        wstr++;
        ascii++;
    }

    return *wstr == 0 && *ascii == 0;
}

static int sithBot_IsAutostartServerPlaceholder(int playerIdx)
{
    sithPlayerInfo *info;

    if (playerIdx != 0 || !Main_bAutostart || Main_numBots <= 0)
        return 0;

    info = &jkPlayer_playerInfos[playerIdx];
    if (sithBot_IsBotNetId(info->net_id))
        return 0;

    return sithBot_WStrEqualsAscii(info->player_name, "ServerDed");
}

static void sithBot_Logf(const char *fmt, ...)
{
    va_list args;
    char buf[512];

    va_start(args, fmt);
#ifdef _MSC_VER
    _vsnprintf(buf, sizeof(buf) - 1, fmt, args);
#else
    vsnprintf(buf, sizeof(buf), fmt, args);
#endif
    va_end(args);
    buf[sizeof(buf) - 1] = 0;

#ifdef TARGET_XBOX
#if defined(XBOX_PERF_SMOKE) || defined(XBOX_VERBOSE_DEBUG)
    xbox_debug_PerfPrintf("%s", buf);
#else
    xbox_debug_Print(buf);
#endif
#else
    stdPlatform_Printf("%s", buf);
#endif
}

void sithBot_LogScoreboard(const char *reason)
{
    int i;

    if (!sithNet_isMulti)
        return;

    sithMulti_ProcessScore();
    sithBot_Logf("BotMatch: scoreboard reason=%s elapsedMs=%u players=%d map='%s' episode='%s'\n",
                 reason ? reason : "manual",
                 sithBot_matchStartMs ? (unsigned)(sithTime_curMs - sithBot_matchStartMs) : 0,
                 jkPlayer_maxPlayers,
                 sithWorld_pCurrentWorld ? sithWorld_pCurrentWorld->map_jkl_fname : "",
                 sithWorld_pCurrentWorld ? sithWorld_pCurrentWorld->episodeName : "");

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        sithPlayerInfo *info = &jkPlayer_playerInfos[i];
        if (!(info->flags & 1))
            continue;
        sithBot_Logf("BotMatch: score slot=%d bot=%d name='%S' kills=%d deaths=%d suicides=%d score=%d net=%u\n",
                     i,
                     sithBot_IsBotNetId(info->net_id),
                     info->player_name,
                     info->numKills,
                     info->numKilled,
                     info->numSuicides,
                     info->score,
                     info->net_id);
    }
    sithBot_Logf("BotMatch: quality jumpDetected=%d jumpLanded=%d jumpRetry=%d jumpFailed=%d jumpTimeout=%d routeNudges=%d routeStalls=%d noLosFireAttempts=%d\n",
                 sithBot_qualityJumpDetected,
                 sithBot_qualityJumpLanded,
                 sithBot_qualityJumpRetry,
                 sithBot_qualityJumpFailed,
                 sithBot_qualityJumpTimeout,
                 sithBot_qualityRouteNudges,
                 sithBot_qualityRouteStalls,
                 sithBot_qualityNoLosFireAttempts);
    sithBot_Logf("BotMatch: weapon-shots bryar=%d strifle=%d crossbow=%d repeater=%d rail=%d concussion=%d saber=%d scope=%d blastech=%d\n",
                 sithBot_qualityWeaponShots[Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL],
                 sithBot_qualityWeaponShots[Main_bMotsCompat ? SITHBIN_MOTS_STORMTROOPER_RIFLE : SITHBIN_STORMTROOPER_RIFLE],
                 sithBot_qualityWeaponShots[Main_bMotsCompat ? SITHBIN_MOTS_TUSKEN_PROD : SITHBIN_TUSKEN_PROD],
                 sithBot_qualityWeaponShots[Main_bMotsCompat ? SITHBIN_MOTS_REPEATER : SITHBIN_REPEATER],
                 sithBot_qualityWeaponShots[Main_bMotsCompat ? SITHBIN_MOTS_RAIL_DETONATOR : SITHBIN_RAIL_DETONATOR],
                 sithBot_qualityWeaponShots[Main_bMotsCompat ? SITHBIN_MOTS_CONCUSSION_RIFLE : SITHBIN_CONCUSSION_RIFLE],
                 sithBot_qualityWeaponShots[Main_bMotsCompat ? SITHBIN_MOTS_LIGHTSABER : SITHBIN_LIGHTSABER],
                 Main_bMotsCompat ? sithBot_qualityWeaponShots[SITHBIN_MOTS_STORMTROOPER_SCOPE] : 0,
                 Main_bMotsCompat ? sithBot_qualityWeaponShots[SITHBIN_MOTS_BLASTECH] : 0);
}

static flex_t sithBot_DistSq(const rdVector3 *a, const rdVector3 *b)
{
    flex_t dx = a->x - b->x;
    flex_t dy = a->y - b->y;
    flex_t dz = a->z - b->z;
    return dx * dx + dy * dy + dz * dz;
}

static flex_t sithBot_AbsFlex(flex_t value)
{
    return value < 0.0 ? -value : value;
}

static int sithBot_IsSectorSafeForBot(sithSector *sector)
{
    if (!sector)
        return 0;

    return (sector->flags & (SITH_SECTOR_FALLDEATH | SITH_SECTOR_NOACTORS)) == 0;
}

static int sithBot_IsRiskyNavSectorForBot(sithSector *sector)
{
    if (!sector)
        return 1;

    return (sector->flags & (SITH_SECTOR_COGLINKED | SITH_SECTOR_HAS_COLLIDE_BOX)) ==
        (SITH_SECTOR_COGLINKED | SITH_SECTOR_HAS_COLLIDE_BOX);
}

static int sithBot_IsCollisionSpikeSectorForBot(sithSector *sector)
{
    if (!sector)
        return 1;

    return (sector->flags & SITH_SECTOR_HAS_COLLIDE_BOX) != 0;
}

static int sithBot_IsNavSectorUsableForBot(sithSector *sector)
{
    return sithBot_IsSectorSafeForBot(sector) &&
        !sithBot_IsRiskyNavSectorForBot(sector) &&
        !sithBot_IsDynamicHazardSector(sector);
}

static int sithBot_IsSurfaceWalkableForBot(sithSurface *surface)
{
    if (!surface || !sithBot_IsSectorSafeForBot(surface->parent_sector))
        return 0;
    if (!(surface->surfaceFlags & SITH_SURFACE_FLOOR))
        return 0;

    /* The stock AI walk probe treats this historic flag as non-walkable. */
    return (surface->surfaceFlags & SITH_SURFACE_AI_CAN_WALK_ON_FLOOR) == 0;
}

static int sithBot_AddNode(const rdVector3 *pos, sithSector *sector, int kind, int thingIdx, flex_t minDist)
{
    int i;

    if (!pos || !sithBot_IsNavSectorUsableForBot(sector) || sithBot_numNodes >= SITHBOT_MAX_NODES)
        return -1;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        if (sithBot_nodes[i].sector != sector ||
            sithBot_DistSq(&sithBot_nodes[i].pos, pos) >= minDist * minDist)
        {
            continue;
        }

        /* A pickup is a goal layered over the floor, not a replacement for the
           floor sample. Keep the two roles separate so routes never depend on
           using item nodes as hallway joints. */
        if ((kind == SITHBOT_NODE_ITEM) != (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM))
            continue;

        if (sithBot_nodes[i].sector == sector)
            return i;
    }

    rdVector_Copy3(&sithBot_nodes[sithBot_numNodes].pos, pos);
    sithBot_nodes[sithBot_numNodes].sector = sector;
    sithBot_nodes[sithBot_numNodes].kind = kind;
    sithBot_nodes[sithBot_numNodes].thingIdx = thingIdx;
    sithBot_nodes[sithBot_numNodes].pathFrame = -1;
    sithBot_nodes[sithBot_numNodes].edgeCount = 0;
    return sithBot_numNodes++;
}

static int sithBot_CanSeePosition(sithSector *fromSector, const rdVector3 *fromPos, sithSector *toSector, const rdVector3 *toPos)
{
    rdVector3 end;
    sithSector *hitSector;

    if (!fromSector || !toSector || !fromPos || !toPos)
        return 0;

    rdVector_Copy3(&end, toPos);
    hitSector = sithCollision_GetSectorLookAt(fromSector, fromPos, &end, 0.03);
    return hitSector == toSector;
}

static int sithBot_HasCombatLos(sithThing *from, sithThing *to)
{
    if (!from || !to)
        return 0;
    return sithCollision_HasLos(from, to, 0);
}

static int sithBot_IsUpwardThrustSector(sithSector *sector)
{
    flex_t liftThreshold = 0.35;
    int i;

    if (!sector)
        return 0;

    for (i = 0; i < sithBot_numInferredLiftSectors; i++)
    {
        if (sithBot_inferredLiftSectors[i] == sector)
            return 1;
    }

    if (!(sector->flags & SITH_SECTOR_HASTHRUST))
        return 0;

    if (sithWorld_pCurrentWorld && sithWorld_pCurrentWorld->worldGravity > 0.0)
        liftThreshold = sithWorld_pCurrentWorld->worldGravity * 0.75;

    return sector->thrust.z > liftThreshold;
}

static int sithBot_AddInferredLiftSector(sithSector *sector)
{
    int i;

    if (!sithBot_IsSectorSafeForBot(sector))
        return 0;

    for (i = 0; i < sithBot_numInferredLiftSectors; i++)
    {
        if (sithBot_inferredLiftSectors[i] == sector)
            return 0;
    }

    if (sithBot_numInferredLiftSectors >= SITHBOT_MAX_INFERRED_LIFTS)
        return 0;

    sithBot_inferredLiftSectors[sithBot_numInferredLiftSectors++] = sector;
    return 1;
}

static int sithBot_CogScriptUsesVerb(sithCog *cog, cogSymbolFunc_t func)
{
    sithCogScript *script;
    int32_t *program;
    uint32_t i;

    if (!cog || !func || !cog->cogscript || !cog->pSymbolTable)
        return 0;

    script = cog->cogscript;
    program = script->script_program;
    if (!program || !script->codeSize)
        return 0;

    i = 0;
    while (i < script->codeSize)
    {
        int op = program[i++];
        switch (op)
        {
            case COG_OPCODE_PUSHINT:
            case COG_OPCODE_PUSHFLOAT:
                i++;
                break;

            case COG_OPCODE_PUSHSYMBOL:
            {
                int symbolIdx;
                sithCogSymbol *symbol;

                if (i >= script->codeSize)
                    return 0;
                symbolIdx = program[i++];
                symbol = sithCogParse_GetSymbol(cog->pSymbolTable, symbolIdx);
                if (symbol && symbol->val.type == COG_VARTYPE_VERB && symbol->val.dataAsFunc == func)
                    return 1;
                break;
            }

            case COG_OPCODE_PUSHVECTOR:
                i += 3;
                break;

            case COG_OPCODE_GOFALSE:
            case COG_OPCODE_GOTRUE:
            case COG_OPCODE_GO:
            case COG_OPCODE_CALL:
                i++;
                break;

            default:
                break;
        }
    }

    return 0;
}

static sithSector *sithBot_GetCogSectorSymbol(sithCog *cog, sithCogReference *ref)
{
    sithCogSymbol *symbol;
    int sectorIdx;

    if (!cog || !ref || ref->type != SENDERTYPE_SECTOR || !cog->pSymbolTable || !sithWorld_pCurrentWorld)
        return 0;

    symbol = sithCogParse_GetSymbol(cog->pSymbolTable, ref->hash);
    if (!symbol || symbol->val.type != COG_VARTYPE_INT)
        return 0;

    sectorIdx = symbol->val.data[0];
    if (sectorIdx < 0 || sectorIdx >= sithWorld_pCurrentWorld->numSectors)
        return 0;

    return &sithWorld_pCurrentWorld->sectors[sectorIdx];
}

static int sithBot_InferLiftSectorsFromCogs(void)
{
    int i;
    int j;
    int added = 0;

    if (!sithWorld_pCurrentWorld || !sithWorld_pCurrentWorld->cogs)
        return 0;

    for (i = 0; i < sithWorld_pCurrentWorld->numCogsLoaded; i++)
    {
        sithCog *cog = &sithWorld_pCurrentWorld->cogs[i];
        sithCogScript *script = cog->cogscript;

        if (!script || !script->aIdk)
            continue;
        if (!sithBot_CogScriptUsesVerb(cog, sithCogFunctionSector_SetSectorThrust))
            continue;

        for (j = 0; j < (int)script->numIdk; j++)
        {
            sithSector *sector = sithBot_GetCogSectorSymbol(cog, &script->aIdk[j]);
            if (sector)
                added += sithBot_AddInferredLiftSector(sector);
        }
    }

    return added;
}

static int sithBot_SegmentTouchesUpwardThrustSector(const SithBotNode *from, const SithBotNode *to)
{
    sithSector *sector;
    rdVector3 prev;
    rdVector3 sample;
    rdVector3 sectorEnd;
    int samples;
    int i;

    if (!from || !to)
        return 0;
    if (sithBot_IsUpwardThrustSector(from->sector) || sithBot_IsUpwardThrustSector(to->sector))
        return 1;

    samples = (int)(rdVector_Dist3(&from->pos, &to->pos) / 0.75) + 1;
    if (samples < 2)
        samples = 2;
    if (samples > 18)
        samples = 18;

    rdVector_Copy3(&prev, &from->pos);
    sector = from->sector;
    for (i = 1; i <= samples; i++)
    {
        flex_t t = (flex_t)i / (flex_t)samples;
        sample.x = from->pos.x + (to->pos.x - from->pos.x) * t;
        sample.y = from->pos.y + (to->pos.y - from->pos.y) * t;
        sample.z = from->pos.z + (to->pos.z - from->pos.z) * t;

        rdVector_Copy3(&sectorEnd, &sample);
        sector = sithCollision_GetSectorLookAt(sector, &prev, &sectorEnd, 0.03);
        if (!sithBot_IsSectorSafeForBot(sector))
            return 0;
        if (sithBot_IsUpwardThrustSector(sector))
            return 1;

        rdVector_Copy3(&prev, &sample);
    }

    return 0;
}

static int sithBot_IsAssistedVerticalSegment(const SithBotNode *from, const SithBotNode *to)
{
    flex_t dx;
    flex_t dy;
    flex_t dz;
    flex_t horizontalDistSq;

    if (!from || !to || !sithBot_IsSectorSafeForBot(from->sector) || !sithBot_IsSectorSafeForBot(to->sector))
        return 0;

    dz = sithBot_AbsFlex(to->pos.z - from->pos.z);
    if (dz <= 1.45 || dz > 12.0)
        return 0;

    dx = to->pos.x - from->pos.x;
    dy = to->pos.y - from->pos.y;
    horizontalDistSq = dx * dx + dy * dy;
    if (horizontalDistSq > 9.0)
        return 0;
    if (!sithBot_CanSeePosition(from->sector, &from->pos, to->sector, &to->pos))
        return 0;

    return sithBot_SegmentTouchesUpwardThrustSector(from, to);
}

static int sithBot_NodeNeedsFloor(const SithBotNode *node)
{
    if (!node)
        return 1;
    if (node->kind == SITHBOT_NODE_LIFT && sithBot_IsUpwardThrustSector(node->sector))
        return 0;
    return 1;
}

static sithThing *sithBot_FindNavProbeThing(void)
{
    int i;

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        sithThing *thing = jkPlayer_playerInfos[i].playerThing;
        if (thing && thing->type == SITH_THING_PLAYER)
            return thing;
    }

    if (sithWorld_pCurrentWorld && sithWorld_pCurrentWorld->playerThing &&
        sithWorld_pCurrentWorld->playerThing->type == SITH_THING_PLAYER)
        return sithWorld_pCurrentWorld->playerThing;

    return 0;
}

static int sithBot_IsSupportedByStandableThing(sithThing *thing)
{
    return thing &&
        (thing->attach_flags & (SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)) &&
        thing->attachedThing &&
        (thing->attachedThing->thingflags & SITH_TF_STANDABLE);
}

static int sithBot_PositionHasWalkableFloorWithRise(sithThing *probeThing, sithSector *sector, const rdVector3 *pos, flex_t stepHeight)
{
    rdVector3 down;
    rdVector3 probeStart;
    rdVector3 probeEnd;
    sithCollisionSearchEntry *entry;
    sithSector *searchSector;
    flex_t searchDist = 1.10;
    flex_t searchRadius = 0.03;
    int result = 0;

    if (!pos || !sithBot_IsSectorSafeForBot(sector))
        return 0;

    if (stepHeight < 0.35)
        stepHeight = 0.35;
    if (stepHeight > 1.80)
        stepHeight = 1.80;

    if (probeThing)
    {
        searchRadius = probeThing->moveSize * 0.25;
        if (searchRadius < 0.03)
            searchRadius = 0.03;

        searchDist = sithPhysics_ThingGetInsertOffsetZ(probeThing) + 0.65;
        if (searchDist < 0.85)
            searchDist = 0.85;
        if (searchDist > 1.80)
            searchDist = 1.80;
    }

    rdVector_Copy3(&probeStart, pos);
    probeStart.z += stepHeight;
    searchDist += stepHeight;
    rdVector_Copy3(&probeEnd, &probeStart);
    searchSector = sithCollision_GetSectorLookAt(sector, pos, &probeEnd, 0.01);
    if (!searchSector)
        searchSector = sector;
    if (!sithBot_IsSectorSafeForBot(searchSector))
        return 0;
    rdVector_Neg3(&down, &rdroid_zVector3);
    sithCollision_SearchRadiusForThings(searchSector, probeThing, &probeStart, &down, searchDist, searchRadius, RAYCAST_2000 | RAYCAST_2);
    while ((entry = sithCollision_NextSearchResult()) != 0)
    {
        if (entry->hitType & SITHCOLLISION_WORLD)
        {
            if (sithBot_IsSurfaceWalkableForBot(entry->surface))
            {
                result = 1;
                break;
            }
            if (entry->surface && (entry->surface->surfaceFlags & SITH_SURFACE_FLOOR))
                break;
            continue;
        }
        if (entry->hitType & SITHCOLLISION_THING)
        {
            if (entry->receiver && (entry->receiver->thingflags & SITH_TF_STANDABLE))
            {
                result = 1;
                break;
            }
        }
    }
    sithCollision_SearchClose();

    return result;
}

static int sithBot_PositionHasWalkableFloor(sithThing *probeThing, sithSector *sector, const rdVector3 *pos)
{
    return sithBot_PositionHasWalkableFloorWithRise(probeThing, sector, pos, 0.35);
}

static int sithBot_IsWalkableSegment(const SithBotNode *from, const SithBotNode *to)
{
    sithThing *probeThing;
    sithSector *sector;
    rdVector3 flatDir;
    rdVector3 prev;
    rdVector3 sample;
    rdVector3 sectorEnd;
    flex_t dist;
    flex_t dx;
    flex_t dy;
    flex_t rise;
    int samples;
    int i;
    int assistedVertical;

    if (!from || !to || !sithBot_IsSectorSafeForBot(from->sector) || !sithBot_IsSectorSafeForBot(to->sector))
        return 0;
    assistedVertical = sithBot_IsAssistedVerticalSegment(from, to);
    if (!assistedVertical && sithBot_AbsFlex(to->pos.z - from->pos.z) > 1.45)
        return 0;
    dx = to->pos.x - from->pos.x;
    dy = to->pos.y - from->pos.y;
    rise = to->pos.z - from->pos.z;
    flatDir.x = dx;
    flatDir.y = dy;
    flatDir.z = 0.0;
    rdVector_Normalize3Acc(&flatDir);
    if (!assistedVertical && rise > 0.45 &&
        dx * dx + dy * dy < rise * rise * 1.5625)
    {
        return 0;
    }
    if (!sithBot_CanSeePosition(from->sector, &from->pos, to->sector, &to->pos))
        return 0;

    probeThing = sithBot_FindNavProbeThing();
    if ((sithBot_NodeNeedsFloor(from) && !sithBot_PositionHasWalkableFloor(probeThing, from->sector, &from->pos)) ||
        (sithBot_NodeNeedsFloor(to) && !sithBot_PositionHasWalkableFloor(probeThing, to->sector, &to->pos)))
        return 0;

    dist = rdVector_Dist3(&from->pos, &to->pos);
    samples = (int)(dist / 0.75) + 1;
    if (samples < 1)
        samples = 1;
    if (samples > 18)
        samples = 18;

    rdVector_Copy3(&prev, &from->pos);
    sector = from->sector;
    for (i = 1; i <= samples; i++)
    {
        flex_t t = (flex_t)i / (flex_t)samples;
        sample.x = from->pos.x + (to->pos.x - from->pos.x) * t;
        sample.y = from->pos.y + (to->pos.y - from->pos.y) * t;
        sample.z = from->pos.z + (to->pos.z - from->pos.z) * t;

        if (!assistedVertical && sithBot_AbsFlex(sample.z - prev.z) > 0.80)
            return 0;

        rdVector_Copy3(&sectorEnd, &sample);
        sector = sithCollision_GetSectorLookAt(sector, &prev, &sectorEnd, 0.03);
        if (!sithBot_IsSectorSafeForBot(sector))
            return 0;
        if (sithBot_IsDynamicHazardSector(sector) && sector != from->sector)
            return 0;
        if (!(assistedVertical && sithBot_IsUpwardThrustSector(sector)) &&
            !sithBot_PositionHasWalkableFloor(probeThing, sector, &sample))
            return 0;
        if (!assistedVertical && probeThing &&
            !sithBot_PositionHasWalkableFootprint(probeThing, sector, &sample, &flatDir, 0.35))
        {
            return 0;
        }

        rdVector_Copy3(&prev, &sample);
    }

    return 1;
}

static int sithBot_IsMoveStepSafeWithRise(sithThing *thing, const rdVector3 *flatDir, flex_t probeDist, flex_t stepHeight)
{
    rdVector3 probe;
    rdVector3 end;
    sithSector *sector;

    if (!thing || !thing->sector || !flatDir)
        return 0;

    rdVector_Copy3(&probe, &thing->position);
    probe.x += flatDir->x * probeDist;
    probe.y += flatDir->y * probeDist;

    rdVector_Copy3(&end, &probe);
    sector = sithCollision_GetSectorLookAt(thing->sector, &thing->position, &end, 0.03);
    if (!sithBot_IsSectorSafeForBot(sector))
        return 0;
    if (sithBot_IsDynamicHazardSector(sector) && sector != thing->sector)
        return 0;

    if (sithBot_IsUpwardThrustSector(sector) ||
        sithBot_IsUpwardThrustSector(thing->sector) ||
        sithBot_IsSupportedByStandableThing(thing))
        return sithBot_PositionHasWalkableFloorWithRise(thing, sector, &probe, stepHeight);

    return sithBot_PositionHasWalkableFootprint(thing, sector, &probe, flatDir, stepHeight);
}

static int sithBot_IsMoveStepSafe(sithThing *thing, const rdVector3 *flatDir, flex_t probeDist)
{
    return sithBot_IsMoveStepSafeWithRise(thing, flatDir, probeDist, 0.35);
}

static int sithBot_IsMoveCenterSafeWithRise(sithThing *thing, const rdVector3 *flatDir, flex_t probeDist, flex_t stepHeight)
{
    rdVector3 probe;
    rdVector3 end;
    sithSector *sector;

    if (!thing || !thing->sector || !flatDir)
        return 0;

    rdVector_Copy3(&probe, &thing->position);
    probe.x += flatDir->x * probeDist;
    probe.y += flatDir->y * probeDist;
    rdVector_Copy3(&end, &probe);
    sector = sithCollision_GetSectorLookAt(thing->sector, &thing->position, &end, 0.03);
    if (!sithBot_IsSectorSafeForBot(sector))
        return 0;
    if (sithBot_IsDynamicHazardSector(sector) && sector != thing->sector)
        return 0;

    return sithBot_PositionHasWalkableFloorWithRise(thing, sector, &probe, stepHeight);
}

static int sithBot_PositionHasWalkableFootprint(sithThing *probeThing, sithSector *sector, const rdVector3 *pos, const rdVector3 *flatDir, flex_t stepHeight)
{
    rdVector3 dir;
    rdVector3 offsets[5];
    flex_t radius = 0.12;
    int numOffsets = 0;
    int i;

    if (!probeThing || !sector || !pos)
        return 0;
    if (!sithBot_PositionHasWalkableFloorWithRise(probeThing, sector, pos, stepHeight))
        return 0;

    radius = probeThing->moveSize;
    if (radius < 0.06)
        radius = 0.06;
    if (radius > 0.20)
        radius = 0.20;

    if (flatDir)
        rdVector_Copy3(&dir, flatDir);
    else
    {
        dir.x = 1.0;
        dir.y = 0.0;
        dir.z = 0.0;
    }
    dir.z = 0.0;
    if (rdVector_Normalize3Acc(&dir) <= 0.001)
    {
        dir.x = 1.0;
        dir.y = 0.0;
        dir.z = 0.0;
    }

    offsets[numOffsets].x = -dir.y * radius;
    offsets[numOffsets].y = dir.x * radius;
    offsets[numOffsets].z = 0.0;
    numOffsets++;
    offsets[numOffsets].x = dir.y * radius;
    offsets[numOffsets].y = -dir.x * radius;
    offsets[numOffsets].z = 0.0;
    numOffsets++;
    offsets[numOffsets].x = dir.x * radius;
    offsets[numOffsets].y = dir.y * radius;
    offsets[numOffsets].z = 0.0;
    numOffsets++;
    offsets[numOffsets].x = dir.x * radius - dir.y * radius * 0.7;
    offsets[numOffsets].y = dir.y * radius + dir.x * radius * 0.7;
    offsets[numOffsets].z = 0.0;
    numOffsets++;
    offsets[numOffsets].x = dir.x * radius + dir.y * radius * 0.7;
    offsets[numOffsets].y = dir.y * radius - dir.x * radius * 0.7;
    offsets[numOffsets].z = 0.0;
    numOffsets++;

    for (i = 0; i < numOffsets; i++)
    {
        rdVector3 sample;
        rdVector3 end;
        sithSector *sampleSector;

        rdVector_Add3(&sample, pos, &offsets[i]);
        rdVector_Copy3(&end, &sample);
        sampleSector = sithCollision_GetSectorLookAt(sector, pos, &end, 0.03);
        if (!sithBot_IsSectorSafeForBot(sampleSector))
            return 0;
        if (sithBot_IsDynamicHazardSector(sampleSector) && sampleSector != sector)
            return 0;
        if (i >= 2 && !sithBot_PositionHasWalkableFloorWithRise(probeThing, sampleSector, &sample, stepHeight))
            return 0;
    }

    return 1;
}

static void sithBot_RecordSafeAnchor(SithBotState *state, sithThing *thing)
{
    if (!state || !thing || !thing->sector || !sithBot_IsSectorSafeForBot(thing->sector))
        return;
    if (sithBot_IsDynamicHazardSector(thing->sector))
        return;
    if (!thing->attach_flags &&
        !sithBot_PositionHasWalkableFloor(thing, thing->sector, &thing->position))
    {
        return;
    }

    if (!state->safeAnchorSector)
    {
        rdVector_Copy3(&state->safeAnchorPos, &thing->position);
        state->safeAnchorSector = thing->sector;
        state->nextSafeAnchorMs = sithTime_curMs + 750;
        return;
    }

    if (state->nextSafeAnchorMs > sithTime_curMs || !thing->attach_flags)
        return;
    if (!sithBot_PositionHasWalkableFloor(thing, thing->sector, &thing->position))
        return;

    rdVector_Copy3(&state->safeAnchorPos, &thing->position);
    state->safeAnchorSector = thing->sector;
    state->nextSafeAnchorMs = sithTime_curMs + 750;
}

static int sithBot_TryRecoverFromFall(SithBotState *state, sithThing *thing)
{
    rdVector3 fallPos;
    sithSector *fallSector;
    int fallDeathSector;
    int unsupportedDrop;

    if (!state || !thing || !thing->sector || !state->safeAnchorSector ||
        (thing->thingflags & SITH_TF_DEAD) || thing->actorParams.health <= 0.0 ||
        state->nextFallRecoveryMs > sithTime_curMs)
    {
        return 0;
    }

    fallDeathSector = (thing->sector->flags & SITH_SECTOR_FALLDEATH) != 0;
    unsupportedDrop = !thing->attach_flags &&
        thing->physicsParams.vel.z < -1.5 &&
        thing->position.z < state->safeAnchorPos.z - 1.6 &&
        !sithBot_PositionHasWalkableFloor(thing, thing->sector, &thing->position);
    if (!fallDeathSector && !unsupportedDrop)
        return 0;

    rdVector_Copy3(&fallPos, &thing->position);
    fallSector = thing->sector;
    if (thing->attach_flags)
        sithThing_DetachThing(thing);
    sithThing_LeaveSector(thing);
    sithThing_SetPosAndRot(thing, &state->safeAnchorPos, &thing->lookOrientation);
    sithThing_EnterSector(thing, state->safeAnchorSector, 1, 0);
    sithPhysics_ThingStop(thing);
    sithPhysics_FindFloor(thing, 1);
    thing->thingflags &= ~SITH_TF_DEAD;
    thing->actorParams.typeflags &= ~SITH_AF_FALLING_TO_DEATH;

    state->goalNode = -1;
    state->nextNode = -1;
    state->nextGoalMs = 0;
    state->goalMode = SITHBOT_GOAL_ESCAPE;
    state->hazardFleeUntilMs = sithTime_curMs + 3600;
    rdVector_Copy3(&state->hazardPos, &fallPos);
    state->hazardSector = fallSector;
    state->stuckTicks = 0;
    state->blockedMoveTicks = 0;
    state->nextFallRecoveryMs = sithTime_curMs + 1200;
    state->nextSafeAnchorMs = sithTime_curMs + 750;
    rdVector_Copy3(&state->lastMovePos, &thing->position);
    state->lastMoveCheckMs = sithTime_curMs;

    if (sithComm_multiplayerFlags)
        sithDSSThing_SendSyncThing(thing, -1, 255);

    if (sithBot_debugFallRecoveriesLogged < 32)
    {
        sithBot_Logf("BotMatch: fall-recovery slot=%d reason=%s pos=(%.2f,%.2f,%.2f) anchor=(%.2f,%.2f,%.2f)\n",
                     state->playerIdx,
                     fallDeathSector ? "fall-sector" : "unsupported-drop",
                     fallPos.x,
                     fallPos.y,
                     fallPos.z,
                     state->safeAnchorPos.x,
                     state->safeAnchorPos.y,
                     state->safeAnchorPos.z);
        sithBot_debugFallRecoveriesLogged++;
    }

    return 1;
}

static int sithBot_FindSafeMoveDir(sithThing *thing, rdVector3 *flatDir, flex_t desiredSpeed, flex_t targetDz, flex_t *safeSpeed)
{
    rdVector3 candidates[8];
    flex_t probes[3];
    flex_t probeDist;
    flex_t stepHeight = 0.35;
    int i;
    int j;

    if (!thing || !flatDir)
        return 0;
    if (safeSpeed)
        *safeSpeed = desiredSpeed;

    if (targetDz > 0.25)
    {
        stepHeight = targetDz + 0.35;
        if (stepHeight > 1.80)
            stepHeight = 1.80;
    }

    probeDist = desiredSpeed * 0.38;
    if (probeDist < 0.55)
        probeDist = 0.55;
    if (probeDist > 1.15)
        probeDist = 1.15;

    if (!sithBot_PositionHasWalkableFloor(thing, thing->sector, &thing->position))
        return thing->physicsParams.vel.z > -1.0;

    rdVector_Copy3(&candidates[0], flatDir);
    candidates[1].x = flatDir->x - flatDir->y * 0.45;
    candidates[1].y = flatDir->y + flatDir->x * 0.45;
    candidates[1].z = 0.0;
    candidates[2].x = flatDir->x + flatDir->y * 0.45;
    candidates[2].y = flatDir->y - flatDir->x * 0.45;
    candidates[2].z = 0.0;
    candidates[3].x = -flatDir->y;
    candidates[3].y = flatDir->x;
    candidates[3].z = 0.0;
    candidates[4].x = flatDir->y;
    candidates[4].y = -flatDir->x;
    candidates[4].z = 0.0;
    candidates[5].x = flatDir->x * 0.35 - flatDir->y;
    candidates[5].y = flatDir->y * 0.35 + flatDir->x;
    candidates[5].z = 0.0;
    candidates[6].x = flatDir->x * 0.35 + flatDir->y;
    candidates[6].y = flatDir->y * 0.35 - flatDir->x;
    candidates[6].z = 0.0;
    candidates[7].x = -flatDir->x;
    candidates[7].y = -flatDir->y;
    candidates[7].z = 0.0;

    probes[0] = probeDist;
    probes[1] = probeDist * 0.55;
    probes[2] = 0.24;

    for (i = 0; i < 8; i++)
    {
        for (j = 0; j < 3; j++)
        {
            rdVector3 candidate;

            rdVector_Copy3(&candidate, &candidates[i]);
            if (rdVector_Normalize3Acc(&candidate) <= 0.001)
                continue;
            if (!sithBot_IsMoveStepSafeWithRise(thing, &candidate, probes[j], stepHeight))
                continue;

            rdVector_Copy3(flatDir, &candidate);
            if (safeSpeed && j > 0)
            {
                flex_t verifiedSpeed = probes[j] / 0.38;
                if (verifiedSpeed < 1.60)
                    verifiedSpeed = 1.60;
                if (*safeSpeed > verifiedSpeed)
                    *safeSpeed = verifiedSpeed;
            }
            return 1;
        }
    }

    if (sithBot_AbsFlex(targetDz) > 0.10)
    {
        for (i = 0; i < 3; i++)
        {
            rdVector3 candidate;

            rdVector_Copy3(&candidate, &candidates[i]);
            if (rdVector_Normalize3Acc(&candidate) <= 0.001)
                continue;
            if (!sithBot_IsMoveCenterSafeWithRise(thing, &candidate, 0.24, stepHeight))
                continue;

            rdVector_Copy3(flatDir, &candidate);
            if (safeSpeed && *safeSpeed > 1.60)
                *safeSpeed = 1.60;
            return 1;
        }
    }

    return 0;
}

static int sithBot_HasEdge(int a, int b)
{
    SithBotNode *node;
    int i;

    if (a < 0 || b < 0 || a == b || a >= sithBot_numNodes || b >= sithBot_numNodes)
        return 0;

    node = &sithBot_nodes[a];
    for (i = 0; i < node->edgeCount; i++)
    {
        if (node->edges[i] == b)
            return 1;
    }

    return 0;
}

static int sithBot_AddEdge(int a, int b)
{
    SithBotNode *node;

    if (a < 0 || b < 0 || a == b || a >= sithBot_numNodes || b >= sithBot_numNodes)
        return 0;
    if (sithBot_HasEdge(a, b))
        return 0;

    node = &sithBot_nodes[a];
    if (node->edgeCount < SITHBOT_MAX_EDGES)
    {
        node->edges[node->edgeCount++] = b;
        return 1;
    }

    return 0;
}

static int sithBot_AddPreferredEdge(int a, int b)
{
    SithBotNode *node;
    int i;

    if (a < 0 || b < 0 || a == b || a >= sithBot_numNodes || b >= sithBot_numNodes)
        return 0;
    if (sithBot_HasEdge(a, b))
        return 0;

    node = &sithBot_nodes[a];
    if (node->edgeCount >= SITHBOT_MAX_EDGES)
        return 0;

    for (i = node->edgeCount; i > 0; i--)
        node->edges[i] = node->edges[i - 1];
    node->edges[0] = b;
    node->edgeCount++;
    return 1;
}

static int sithBot_SectorsHaveDirectEdge(sithSector *fromSector, sithSector *toSector)
{
    int i;
    int j;

    if (!fromSector || !toSector || fromSector == toSector)
        return 1;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        if (sithBot_nodes[i].sector != fromSector)
            continue;
        for (j = 0; j < sithBot_nodes[i].edgeCount; j++)
        {
            int other = sithBot_nodes[i].edges[j];
            if (other >= 0 && other < sithBot_numNodes && sithBot_nodes[other].sector == toSector)
                return 1;
        }
    }

    return 0;
}

static int sithBot_GetSurfacePortalBounds(sithSurface *surface, rdVector3 *center, flex_t *minZ, flex_t *maxZ)
{
    rdFace *face;
    int i;

    if (!surface || !center || !minZ || !maxZ || !sithWorld_pCurrentWorld)
        return 0;

    face = &surface->surfaceInfo.face;
    if (face->numVertices < 3 || !face->vertexPosIdx)
        return 0;

    rdVector_Zero3(center);
    *minZ = 3.4e38f;
    *maxZ = -3.4e38f;
    for (i = 0; i < (int)face->numVertices; i++)
    {
        int vertexIdx = face->vertexPosIdx[i];
        rdVector3 *vertex;

        if (vertexIdx < 0 || vertexIdx >= sithWorld_pCurrentWorld->numVertices)
            return 0;
        vertex = &sithWorld_pCurrentWorld->vertices[vertexIdx];
        rdVector_Add3Acc(center, vertex);
        if (vertex->z < *minZ)
            *minZ = vertex->z;
        if (vertex->z > *maxZ)
            *maxZ = vertex->z;
    }
    rdVector_InvScale3Acc(center, (flex_t)face->numVertices);
    return 1;
}

static int sithBot_FindPortalNode(sithSector *sector, const rdVector3 *center, flex_t floorZ)
{
    int i;
    int best = -1;
    flex_t bestScore = 3.4e38f;

    if (!sector || !center)
        return -1;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        flex_t dx;
        flex_t dy;
        flex_t dz;
        flex_t score;

        if (sithBot_nodes[i].sector != sector || sithBot_nodes[i].kind == SITHBOT_NODE_ITEM ||
            sithBot_nodes[i].kind == SITHBOT_NODE_LIFT)
        {
            continue;
        }

        dx = sithBot_nodes[i].pos.x - center->x;
        dy = sithBot_nodes[i].pos.y - center->y;
        dz = sithBot_AbsFlex(sithBot_nodes[i].pos.z - floorZ);
        if (dx * dx + dy * dy > 16.0 || dz > 0.70)
            continue;

        score = dx * dx + dy * dy + dz * dz * 6.0;
        if (score < bestScore)
        {
            bestScore = score;
            best = i;
        }
    }

    return best;
}

static int sithBot_IsNarrowTransitSector(sithSector *sector)
{
    rdVector3 *low;
    rdVector3 *high;
    flex_t width;
    flex_t depth;
    flex_t minDim;
    flex_t maxDim;

    if (!sector)
        return 0;

    if (sector->flags & SITH_SECTOR_HAS_COLLIDE_BOX)
    {
        low = &sector->collidebox_onecorner;
        high = &sector->collidebox_othercorner;
    }
    else
    {
        low = &sector->boundingbox_onecorner;
        high = &sector->boundingbox_othercorner;
    }

    width = sithBot_AbsFlex(high->x - low->x);
    depth = sithBot_AbsFlex(high->y - low->y);
    minDim = width < depth ? width : depth;
    maxDim = width > depth ? width : depth;
    return minDim < 0.24 || (minDim < 0.48 && maxDim > 0.55);
}

static int sithBot_AddPortalApproachNodes(void)
{
    int i;
    int added = 0;

    if (!sithWorld_pCurrentWorld)
        return 0;

    for (i = 0; i < sithWorld_pCurrentWorld->numSurfaces && added < SITHBOT_MAX_PORTAL_NODES; i++)
    {
        sithSurface *surface = &sithWorld_pCurrentWorld->surfaces[i];
        sithAdjoin *adjoin = surface->adjoin;
        sithSurface *mirrorSurface;
        sithSector *fromSector;
        sithSector *toSector;
        rdVector3 center;
        rdVector3 fromPos;
        rdVector3 toPos;
        flex_t minZ;
        flex_t maxZ;
        flex_t floorZ;
        int fromNode;
        int toNode;
        int before;

        if (!adjoin || !adjoin->mirror || !adjoin->mirror->surface)
            continue;
        mirrorSurface = adjoin->mirror->surface;
        if (surface->index > mirrorSurface->index)
            continue;
        if (stdMath_Fabs(surface->surfaceInfo.face.normal.z) > 0.65)
            continue;

        fromSector = surface->parent_sector;
        toSector = adjoin->sector;
        if (!sithBot_IsNavSectorUsableForBot(fromSector) || !sithBot_IsNavSectorUsableForBot(toSector))
            continue;
        if (!sithBot_GetSurfacePortalBounds(surface, &center, &minZ, &maxZ) || maxZ - minZ < 0.16)
            continue;

        floorZ = minZ + 0.04;
        fromNode = sithBot_FindPortalNode(fromSector, &center, floorZ);
        toNode = sithBot_FindPortalNode(toSector, &center, floorZ);

        rdVector_Copy3(&fromPos, &center);
        fromPos.x += surface->surfaceInfo.face.normal.x * 0.06;
        fromPos.y += surface->surfaceInfo.face.normal.y * 0.06;
        fromPos.z = floorZ;
        if (fromNode < 0 ||
            (sithBot_IsNarrowTransitSector(fromSector) &&
             sithBot_DistSq(&sithBot_nodes[fromNode].pos, &fromPos) > 0.35 * 0.35))
        {
            before = sithBot_numNodes;
            sithBot_AddNode(&fromPos, fromSector, SITHBOT_NODE_PORTAL, -1, 0.18);
            if (sithBot_numNodes > before)
                added++;
        }

        if (added >= SITHBOT_MAX_PORTAL_NODES)
            break;

        rdVector_Copy3(&toPos, &center);
        toPos.x += mirrorSurface->surfaceInfo.face.normal.x * 0.06;
        toPos.y += mirrorSurface->surfaceInfo.face.normal.y * 0.06;
        toPos.z = floorZ;
        if (toNode < 0 ||
            (sithBot_IsNarrowTransitSector(toSector) &&
             sithBot_DistSq(&sithBot_nodes[toNode].pos, &toPos) > 0.35 * 0.35))
        {
            before = sithBot_numNodes;
            sithBot_AddNode(&toPos, toSector, SITHBOT_NODE_PORTAL, -1, 0.18);
            if (sithBot_numNodes > before)
                added++;
        }
    }

    return added;
}

static int sithBot_LinkAdjoinPortals(void)
{
    int i;
    int added = 0;

    if (!sithWorld_pCurrentWorld)
        return 0;

    for (i = 0; i < sithWorld_pCurrentWorld->numSurfaces; i++)
    {
        sithSurface *surface = &sithWorld_pCurrentWorld->surfaces[i];
        sithAdjoin *adjoin = surface->adjoin;
        sithSector *fromSector;
        sithSector *toSector;
        rdVector3 center;
        flex_t minZ;
        flex_t maxZ;
        flex_t floorZ;
        int fromNode;
        int toNode;

        if (!adjoin || !adjoin->mirror || !adjoin->mirror->surface)
            continue;
        if (surface->index > adjoin->mirror->surface->index)
            continue;
        if (stdMath_Fabs(surface->surfaceInfo.face.normal.z) > 0.65)
            continue;

        fromSector = surface->parent_sector;
        toSector = adjoin->sector;
        if (!sithBot_IsNavSectorUsableForBot(fromSector) || !sithBot_IsNavSectorUsableForBot(toSector))
            continue;
        if (sithBot_SectorsHaveDirectEdge(fromSector, toSector))
            continue;
        if (!sithBot_GetSurfacePortalBounds(surface, &center, &minZ, &maxZ) || maxZ - minZ < 0.16)
            continue;

        floorZ = minZ + 0.04;
        fromNode = sithBot_FindPortalNode(fromSector, &center, floorZ);
        toNode = sithBot_FindPortalNode(toSector, &center, floorZ);
        if (fromNode < 0 || toNode < 0 || fromNode == toNode)
            continue;
        if (sithBot_AbsFlex(sithBot_nodes[fromNode].pos.z - sithBot_nodes[toNode].pos.z) > 0.55)
            continue;

        added += sithBot_AddEdge(fromNode, toNode);
        added += sithBot_AddEdge(toNode, fromNode);
    }

    return added;
}

static void sithBot_ClearLinkCandidates(SithBotLinkCandidate *candidates)
{
    int i;

    for (i = 0; i < SITHBOT_LINK_CANDIDATES; i++)
    {
        candidates[i].nodeIdx = -1;
        candidates[i].distSq = 3.4e38f;
    }
}

static void sithBot_InsertLinkCandidate(SithBotLinkCandidate *candidates, int nodeIdx, flex_t distSq)
{
    int insertAt;
    int i;

    if (nodeIdx < 0 || distSq >= candidates[SITHBOT_LINK_CANDIDATES - 1].distSq)
        return;

    insertAt = SITHBOT_LINK_CANDIDATES - 1;
    while (insertAt > 0 && distSq < candidates[insertAt - 1].distSq)
    {
        candidates[insertAt] = candidates[insertAt - 1];
        insertAt--;
    }

    candidates[insertAt].nodeIdx = nodeIdx;
    candidates[insertAt].distSq = distSq;

    for (i = insertAt + 1; i < SITHBOT_LINK_CANDIDATES; i++)
    {
        if (candidates[i].nodeIdx == nodeIdx)
        {
            candidates[i].nodeIdx = -1;
            candidates[i].distSq = 3.4e38f;
        }
    }
}

static int sithBot_IsCheapLinkCandidate(int a, int b, flex_t *outDistSq)
{
    SithBotNode *from;
    SithBotNode *to;
    flex_t distSq;
    flex_t dz;
    flex_t dx;
    flex_t dy;
    flex_t horizontalDistSq;

    if (a < 0 || b < 0 || a == b || a >= sithBot_numNodes || b >= sithBot_numNodes)
        return 0;

    from = &sithBot_nodes[a];
    to = &sithBot_nodes[b];
    if (!sithBot_IsSectorSafeForBot(from->sector) || !sithBot_IsSectorSafeForBot(to->sector))
        return 0;

    dz = sithBot_AbsFlex(to->pos.z - from->pos.z);
    dx = to->pos.x - from->pos.x;
    dy = to->pos.y - from->pos.y;
    horizontalDistSq = dx * dx + dy * dy;
    if (dz > 1.45 && (dz > 12.0 || horizontalDistSq > 9.0))
        return 0;

    distSq = sithBot_DistSq(&from->pos, &to->pos);
    if (distSq > SITHBOT_LINK_RADIUS_SQ || distSq < 0.01)
        return 0;

    if (outDistSq)
        *outDistSq = distSq;
    return 1;
}

static void sithBot_LinkNodes(void)
{
    int i;
    int j;
    int candidates = 0;
    int tested = 0;
    int rejected = 0;
    int added = 0;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        SithBotLinkCandidate nearby[SITHBOT_LINK_CANDIDATES];

        if (sithBot_nodes[i].edgeCount >= SITHBOT_MAX_EDGES)
            continue;

        sithBot_ClearLinkCandidates(nearby);
        for (j = 0; j < sithBot_numNodes; j++)
        {
            flex_t distSq;
            if (!sithBot_IsCheapLinkCandidate(i, j, &distSq))
                continue;
            candidates++;
            sithBot_InsertLinkCandidate(nearby, j, distSq);
        }

        for (j = 0; j < SITHBOT_LINK_CANDIDATES && sithBot_nodes[i].edgeCount < SITHBOT_MAX_EDGES; j++)
        {
            int other = nearby[j].nodeIdx;
            int forwardWalkable;
            int reverseWalkable;
            if (other < 0)
                break;
            if (sithBot_HasEdge(i, other) && sithBot_HasEdge(other, i))
                continue;

            tested++;
            forwardWalkable = sithBot_HasEdge(i, other) ||
                sithBot_IsWalkableSegment(&sithBot_nodes[i], &sithBot_nodes[other]);
            reverseWalkable = sithBot_HasEdge(other, i) ||
                sithBot_IsWalkableSegment(&sithBot_nodes[other], &sithBot_nodes[i]);
            if (!forwardWalkable && !reverseWalkable)
            {
                rejected++;
                continue;
            }
            if (forwardWalkable)
                added += sithBot_AddEdge(i, other);
            if (reverseWalkable)
                added += sithBot_AddEdge(other, i);
        }
    }

    sithBot_Logf("BotNav: link candidates=%d tested=%d rejected=%d added=%d\n", candidates, tested, rejected, added);
}

static flex_t sithBot_GetPathMoverDeckArea(sithThing *thing)
{
    rdModel3 *model;
    rdGeoset *geoset;
    flex_t bestArea = 0.0;
    int meshIdx;

    if (!thing || thing->rdthing.type != RD_THINGTYPE_MODEL || !thing->rdthing.model3)
        return 0.0;
    model = thing->rdthing.model3;
    if (!model->numGeosets || !model->geosets[0].meshes)
        return 0.0;

    geoset = &model->geosets[0];
    for (meshIdx = 0; meshIdx < (int)geoset->numMeshes; meshIdx++)
    {
        rdMesh *mesh = &geoset->meshes[meshIdx];
        int faceIdx;

        if (!mesh->vertices || !mesh->faces)
            continue;
        for (faceIdx = 0; faceIdx < mesh->numFaces; faceIdx++)
        {
            rdFace *face = &mesh->faces[faceIdx];
            rdVector3 worldNormal;
            flex_t area = 0.0;
            int vertexIdx;

            if (!face->vertexPosIdx || face->numVertices < 3)
                continue;
            rdMatrix_TransformVector34(&worldNormal, &face->normal, &thing->lookOrientation);
            if (rdVector_Dot3(&worldNormal, &rdroid_zVector3) < 0.60)
                continue;

            for (vertexIdx = 1; vertexIdx + 1 < (int)face->numVertices; vertexIdx++)
            {
                int rootIdx = face->vertexPosIdx[0];
                int secondIdx = face->vertexPosIdx[vertexIdx];
                int thirdIdx = face->vertexPosIdx[vertexIdx + 1];
                rdVector3 edgeA;
                rdVector3 edgeB;
                rdVector3 worldA;
                rdVector3 worldB;
                rdVector3 cross;

                if (rootIdx < 0 || rootIdx >= mesh->numVertices ||
                    secondIdx < 0 || secondIdx >= mesh->numVertices ||
                    thirdIdx < 0 || thirdIdx >= mesh->numVertices)
                {
                    continue;
                }
                rdVector_Sub3(&edgeA, &mesh->vertices[secondIdx], &mesh->vertices[rootIdx]);
                rdVector_Sub3(&edgeB, &mesh->vertices[thirdIdx], &mesh->vertices[rootIdx]);
                rdMatrix_TransformVector34(&worldA, &edgeA, &thing->lookOrientation);
                rdMatrix_TransformVector34(&worldB, &edgeB, &thing->lookOrientation);
                rdVector_Cross3(&cross, &worldA, &worldB);
                area += sithBot_AbsFlex(cross.z) * 0.5;
            }
            if (area > bestArea)
                bestArea = area;
        }
    }
    return bestArea;
}

static int sithBot_IsPathLiftThing(sithThing *thing)
{
    int i;

    if (!thing || thing->moveType != SITH_MT_PATH || !(thing->thingflags & SITH_TF_STANDABLE) ||
        thing->trackParams.loadedFrames < 2 || !thing->trackParams.aFrames)
    {
        return 0;
    }

    if (sithBot_GetPathMoverDeckArea(thing) < 0.025)
        return 0;

    for (i = 1; i < thing->trackParams.loadedFrames; i++)
    {
        if (sithBot_DistSq(&thing->trackParams.aFrames[0].pos,
                           &thing->trackParams.aFrames[i].pos) > 0.25 * 0.25)
        {
            return 1;
        }
    }

    return 0;
}

static int sithBot_PathLiftHasCallControlAtStop(sithThing *lift, const rdVector3 *stopPos)
{
    int i;

    if (!lift || !stopPos)
        return 0;

    for (i = 0; i < sithCog_numSurfaceLinks; i++)
    {
        sithCogSurfaceLink *link = &sithCog_aSurfaceLinks[i];
        rdVector3 center;
        flex_t dx;
        flex_t dy;

        if (!link->surface || link->surface->adjoin ||
            !sithBot_CogControlsThing(link->cog, lift) ||
            !sithBot_CogHandlesMessage(link->cog, SITH_MESSAGE_ACTIVATE) ||
            !sithBot_GetSurfaceCenter(link->surface, &center))
        {
            continue;
        }

        dx = center.x - stopPos->x;
        dy = center.y - stopPos->y;
        if (dx * dx + dy * dy <= 16.0 && sithBot_AbsFlex(center.z - stopPos->z) <= 1.20)
            return 1;
    }

    for (i = 0; i < sithCog_numThingLinks; i++)
    {
        sithCogThingLink *link = &sithCog_aThingLinks[i];
        sithThing *control = link->thing;
        flex_t dx;
        flex_t dy;

        if (!control || control == lift || link->signature != control->signature ||
            !sithBot_CogControlsThing(link->cog, lift) ||
            !sithBot_CogHandlesMessage(link->cog, SITH_MESSAGE_ACTIVATE))
        {
            continue;
        }

        dx = control->position.x - stopPos->x;
        dy = control->position.y - stopPos->y;
        if (dx * dx + dy * dy <= 16.0 && sithBot_AbsFlex(control->position.z - stopPos->z) <= 1.20)
            return 1;
    }

    return 0;
}

static int sithBot_AddPathLiftStop(sithThing *lift, int frameIdx, int baseNodeCount, int *outApproachEdges)
{
    sithThingFrame *frame;
    rdVector3 expectedPos;
    rdVector3 stopPos;
    sithSector *stopSector;
    flex_t expectedOffset;
    flex_t approachRadius;
    flex_t maxTopOffset;
    flex_t bestScore = 3.4e38f;
    int bestNode = -1;
    int stopNode;
    int i;
    int approachEdges = 0;
    int carAtStop;
    int canCallCar;
    int canEnterCar;
    int fallbackNode0 = -1;
    int fallbackNode1 = -1;
    flex_t fallbackScore0 = 3.4e38f;
    flex_t fallbackScore1 = 3.4e38f;

    if (!lift || frameIdx < 0 || frameIdx >= lift->trackParams.loadedFrames ||
        sithBot_numNodes >= SITHBOT_MAX_NODES)
    {
        return -1;
    }

    frame = &lift->trackParams.aFrames[frameIdx];
    expectedOffset = lift->moveSize * 0.80;
    if (expectedOffset < 0.08)
        expectedOffset = 0.08;
    if (expectedOffset > 0.55)
        expectedOffset = 0.55;
    approachRadius = lift->moveSize + 1.25;
    if (approachRadius < 0.80)
        approachRadius = 0.80;
    if (approachRadius > 3.00)
        approachRadius = 3.00;
    maxTopOffset = lift->moveSize + 0.35;
    if (maxTopOffset < 0.35)
        maxTopOffset = 0.35;
    if (maxTopOffset > 1.25)
        maxTopOffset = 1.25;

    rdVector_Copy3(&expectedPos, &frame->pos);
    expectedPos.z += expectedOffset;
    for (i = 0; i < baseNodeCount; i++)
    {
        SithBotNode *candidate = &sithBot_nodes[i];
        flex_t dx;
        flex_t dy;
        flex_t horizontalSq;
        flex_t topOffset;
        flex_t zError;
        flex_t score;

        if (candidate->kind == SITHBOT_NODE_ITEM || candidate->kind == SITHBOT_NODE_LIFT ||
            !sithBot_IsNavSectorUsableForBot(candidate->sector))
        {
            continue;
        }

        dx = candidate->pos.x - frame->pos.x;
        dy = candidate->pos.y - frame->pos.y;
        horizontalSq = dx * dx + dy * dy;
        topOffset = candidate->pos.z - frame->pos.z;
        if (horizontalSq > approachRadius * approachRadius || topOffset < -0.10 || topOffset > maxTopOffset)
            continue;

        zError = candidate->pos.z - expectedPos.z;
        score = horizontalSq + zError * zError * 10.0;
        if (score < bestScore)
        {
            bestScore = score;
            bestNode = i;
        }
    }

    if (bestNode < 0)
        return -1;

    rdVector_Copy3(&stopPos, &frame->pos);
    stopPos.z = expectedPos.z;
    stopSector = sithBot_nodes[bestNode].sector;
    {
        rdVector3 end;
        sithSector *tracedSector;
        rdVector_Copy3(&end, &stopPos);
        tracedSector = sithCollision_GetSectorLookAt(stopSector, &sithBot_nodes[bestNode].pos, &end, 0.03);
        if (sithBot_IsNavSectorUsableForBot(tracedSector))
            stopSector = tracedSector;
    }

    stopNode = sithBot_numNodes++;
    rdVector_Copy3(&sithBot_nodes[stopNode].pos, &stopPos);
    sithBot_nodes[stopNode].sector = stopSector;
    sithBot_nodes[stopNode].thingIdx = lift->thingIdx;
    sithBot_nodes[stopNode].pathFrame = frameIdx;
    sithBot_nodes[stopNode].kind = SITHBOT_NODE_LIFT;
    sithBot_nodes[stopNode].edgeCount = 0;
    carAtStop = sithBot_DistSq(&lift->position, &frame->pos) < 0.24 * 0.24;
    canCallCar = sithBot_PathLiftHasCallControlAtStop(lift, &stopPos);
    canEnterCar = carAtStop || (lift->trackParams.flags & 3) != 0 || canCallCar;

    for (i = 0; i < baseNodeCount; i++)
    {
        SithBotNode *candidate = &sithBot_nodes[i];
        rdVector3 end;
        flex_t dx;
        flex_t dy;
        flex_t dz;
        flex_t thisStopScore;
        int belongsToOtherStop = 0;
        int j;

        if (candidate->kind == SITHBOT_NODE_ITEM || candidate->kind == SITHBOT_NODE_LIFT ||
            sithBot_AbsFlex(candidate->pos.z - stopPos.z) > 0.40)
        {
            continue;
        }
        dx = candidate->pos.x - stopPos.x;
        dy = candidate->pos.y - stopPos.y;
        if (dx * dx + dy * dy > approachRadius * approachRadius)
            continue;

        dz = candidate->pos.z - stopPos.z;
        thisStopScore = dx * dx + dy * dy + dz * dz * 6.0;
        for (j = 0; j < lift->trackParams.loadedFrames && j < SITHBOT_MAX_LIFT_FRAMES; j++)
        {
            sithThingFrame *otherFrame;
            flex_t otherDx;
            flex_t otherDy;
            flex_t otherDz;
            flex_t otherScore;

            if (j == frameIdx)
                continue;
            otherFrame = &lift->trackParams.aFrames[j];
            otherDx = candidate->pos.x - otherFrame->pos.x;
            otherDy = candidate->pos.y - otherFrame->pos.y;
            otherDz = candidate->pos.z - (otherFrame->pos.z + expectedOffset);
            otherScore = otherDx * otherDx + otherDy * otherDy + otherDz * otherDz * 6.0;
            if (otherScore + 0.01 < thisStopScore)
            {
                belongsToOtherStop = 1;
                break;
            }
        }
        if (belongsToOtherStop)
            continue;

        rdVector_Copy3(&end, &stopPos);
        if (!sithCollision_GetSectorLookAt(candidate->sector, &candidate->pos, &end, 0.03))
            continue;
        if (thisStopScore < fallbackScore0)
        {
            fallbackScore1 = fallbackScore0;
            fallbackNode1 = fallbackNode0;
            fallbackScore0 = thisStopScore;
            fallbackNode0 = i;
        }
        else if (thisStopScore < fallbackScore1)
        {
            fallbackScore1 = thisStopScore;
            fallbackNode1 = i;
        }
        if (carAtStop && !sithBot_IsWalkableSegment(candidate, &sithBot_nodes[stopNode]))
            continue;
        if (canEnterCar)
            approachEdges += sithBot_AddEdge(i, stopNode);
        approachEdges += sithBot_AddEdge(stopNode, i);
    }

    if (carAtStop && approachEdges == 0)
    {
        if (fallbackNode0 >= 0)
        {
            if (canEnterCar)
                approachEdges += sithBot_AddEdge(fallbackNode0, stopNode);
            approachEdges += sithBot_AddEdge(stopNode, fallbackNode0);
        }
        if (fallbackNode1 >= 0)
        {
            if (canEnterCar)
                approachEdges += sithBot_AddEdge(fallbackNode1, stopNode);
            approachEdges += sithBot_AddEdge(stopNode, fallbackNode1);
        }
    }

    if (outApproachEdges)
        *outApproachEdges += approachEdges;
    sithBot_Logf("BotNav: lift-stop thing=%d frame=%d node=%d sector=%d approaches=%d enter=%d call=%d pos=(%.2f,%.2f,%.2f)\n",
                 lift->thingIdx,
                 frameIdx,
                 stopNode,
                 sithBot_GetSectorIndex(stopSector),
                 approachEdges,
                 canEnterCar,
                 canCallCar,
                 stopPos.x,
                 stopPos.y,
                 stopPos.z);
    return stopNode;
}

static int sithBot_AddPathLiftNodes(int *outStops, int *outEdges)
{
    int baseNodeCount = sithBot_numNodes;
    int liftCount = 0;
    int stopCount = 0;
    int edgeCount = 0;
    int i;

    if (!sithWorld_pCurrentWorld)
        return 0;

    for (i = 0; i < sithWorld_pCurrentWorld->numThingsLoaded && liftCount < SITHBOT_MAX_PATH_LIFTS; i++)
    {
        sithThing *lift = &sithWorld_pCurrentWorld->things[i];
        int stopNodes[SITHBOT_MAX_LIFT_FRAMES];
        int frames;
        int liftStartNode;
        int liftEdges = 0;
        int usableStops = 0;
        int j;

        if (!sithBot_IsPathLiftThing(lift))
            continue;

        frames = lift->trackParams.loadedFrames;
        if (frames > SITHBOT_MAX_LIFT_FRAMES)
            frames = SITHBOT_MAX_LIFT_FRAMES;
        liftStartNode = sithBot_numNodes;
        for (j = 0; j < frames; j++)
        {
            stopNodes[j] = sithBot_AddPathLiftStop(lift, j, baseNodeCount, &liftEdges);
            if (stopNodes[j] >= 0 && sithBot_nodes[stopNodes[j]].edgeCount > 0)
                usableStops++;
        }

        if (usableStops < 2)
        {
            int nodeIdx;
            for (nodeIdx = 0; nodeIdx < liftStartNode; nodeIdx++)
            {
                SithBotNode *node = &sithBot_nodes[nodeIdx];
                int readIdx;
                int writeIdx = 0;
                for (readIdx = 0; readIdx < node->edgeCount; readIdx++)
                {
                    if (node->edges[readIdx] < liftStartNode)
                        node->edges[writeIdx++] = node->edges[readIdx];
                }
                node->edgeCount = writeIdx;
            }
            sithBot_numNodes = liftStartNode;
            continue;
        }

        edgeCount += liftEdges;

        for (j = 1; j < frames; j++)
        {
            if (stopNodes[j - 1] < 0 || stopNodes[j] < 0)
                continue;
            if (sithBot_nodes[stopNodes[j - 1]].edgeCount <= 0 ||
                sithBot_nodes[stopNodes[j]].edgeCount <= 0)
            {
                continue;
            }
            edgeCount += sithBot_AddEdge(stopNodes[j - 1], stopNodes[j]);
            edgeCount += sithBot_AddEdge(stopNodes[j], stopNodes[j - 1]);
        }
        for (j = 0; j < frames; j++)
        {
            if (stopNodes[j] >= 0)
                stopCount++;
        }
        liftCount++;
    }

    if (outStops)
        *outStops = stopCount;
    if (outEdges)
        *outEdges = edgeCount;
    return liftCount;
}

static void sithBot_LogNavComponents(void)
{
    int component[SITHBOT_MAX_NODES];
    int queue[SITHBOT_MAX_NODES];
    int componentId = 0;
    int i;

    for (i = 0; i < sithBot_numNodes; i++)
        component[i] = -1;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        int head = 0;
        int tail = 0;
        int sectorCount = 0;
        int spawnCount = 0;
        int itemCount = 0;
        int j;

        if (component[i] >= 0)
            continue;

        component[i] = componentId;
        queue[tail++] = i;
        while (head < tail)
        {
            int nodeIdx = queue[head++];
            SithBotNode *node = &sithBot_nodes[nodeIdx];
            int firstInSector = 1;

            if (node->kind == SITHBOT_NODE_SPAWN)
                spawnCount++;
            else if (node->kind == SITHBOT_NODE_ITEM)
                itemCount++;

            for (j = 0; j < head - 1; j++)
            {
                if (sithBot_nodes[queue[j]].sector == node->sector)
                {
                    firstInSector = 0;
                    break;
                }
            }
            if (firstInSector)
                sectorCount++;

            for (j = 0; j < node->edgeCount; j++)
            {
                int next = node->edges[j];
                if (next < 0 || next >= sithBot_numNodes || component[next] >= 0)
                    continue;
                component[next] = componentId;
                queue[tail++] = next;
            }
        }

        if (spawnCount || tail >= 8)
        {
            sithBot_Logf("BotNav: component id=%d nodes=%d sectors=%d spawns=%d items=%d\n",
                         componentId, tail, sectorCount, spawnCount, itemCount);
        }
        componentId++;
    }

    for (i = 0; i < sithBot_numNodes; i++)
    {
        if (sithBot_nodes[i].kind != SITHBOT_NODE_SPAWN)
            continue;
        {
            int visited[SITHBOT_MAX_NODES];
            int reachableQueue[SITHBOT_MAX_NODES];
            int head = 0;
            int tail = 0;
            int j;

            memset(visited, 0, sizeof(visited));
            visited[i] = 1;
            reachableQueue[tail++] = i;
            while (head < tail)
            {
                int nodeIdx = reachableQueue[head++];
                SithBotNode *node = &sithBot_nodes[nodeIdx];
                if (nodeIdx != i && node->kind == SITHBOT_NODE_ITEM)
                    continue;
                for (j = 0; j < node->edgeCount; j++)
                {
                    int next = node->edges[j];
                    if (next < 0 || next >= sithBot_numNodes || visited[next])
                        continue;
                    visited[next] = 1;
                    reachableQueue[tail++] = next;
                }
            }

            sithBot_Logf("BotNav: spawn node=%d sector=%d component=%d edges=%d reachable=%d\n",
                         i,
                         sithBot_GetSectorIndex(sithBot_nodes[i].sector),
                         component[i],
                         sithBot_nodes[i].edgeCount,
                         tail);
        }
    }
}

static void sithBot_AddSurfaceNode(sithSurface *surface)
{
    int i;
    rdVector3 pos;
    rdFace *face;

    if (!surface || !surface->parent_sector)
        return;
    if (!sithBot_IsSurfaceWalkableForBot(surface))
        return;

    face = &surface->surfaceInfo.face;
    if (face->numVertices <= 0 || !face->vertexPosIdx || face->normal.z < 0.35)
        return;

    rdVector_Zero3(&pos);
    for (i = 0; i < face->numVertices; i++)
    {
        int idx = face->vertexPosIdx[i];
        if (idx < 0 || idx >= sithWorld_pCurrentWorld->numVertices)
            return;
        rdVector_Add3Acc(&pos, &sithWorld_pCurrentWorld->vertices[idx]);
    }
    rdVector_InvScale3Acc(&pos, (flex_t)face->numVertices);
    pos.x += face->normal.x * 0.04;
    pos.y += face->normal.y * 0.04;
    pos.z += face->normal.z * 0.04;
    sithBot_AddNode(&pos, surface->parent_sector, SITHBOT_NODE_FLOOR, -1, 2.25);
}

static int sithBot_IsJumpPadCog(sithCog *cog)
{
    return cog &&
        sithBot_CogHandlesMessage(cog, SITH_MESSAGE_ENTERED) &&
        sithBot_CogScriptUsesVerb(cog, sithCogFunctionThing_ApplyForce);
}

static int sithBot_AddJumpPadNodes(void)
{
    int added = 0;
    int i;

    for (i = 0; i < sithCog_numSurfaceLinks; i++)
    {
        sithCogSurfaceLink *link = &sithCog_aSurfaceLinks[i];
        sithSurface *surface = link->surface;
        rdVector3 center;
        int nodeIdx;
        int j;

        if (!surface || !surface->parent_sector || !sithBot_IsJumpPadCog(link->cog) ||
            surface->surfaceInfo.face.normal.z < 0.55 ||
            !sithBot_GetSurfaceCenter(surface, &center))
        {
            continue;
        }

        for (j = 0; j < i; j++)
        {
            if (sithCog_aSurfaceLinks[j].surface == surface &&
                sithBot_IsJumpPadCog(sithCog_aSurfaceLinks[j].cog))
            {
                break;
            }
        }
        if (j < i)
            continue;

        center.x += surface->surfaceInfo.face.normal.x * 0.04;
        center.y += surface->surfaceInfo.face.normal.y * 0.04;
        center.z += surface->surfaceInfo.face.normal.z * 0.04;
        nodeIdx = sithBot_AddNode(&center, surface->parent_sector, SITHBOT_NODE_JUMPPAD,
                                  (int)surface->index, 0.45);
        if (nodeIdx < 0)
            continue;

        if (sithBot_nodes[nodeIdx].kind == SITHBOT_NODE_JUMPPAD)
        {
            sithBot_nodes[nodeIdx].pos.x = (sithBot_nodes[nodeIdx].pos.x + center.x) * 0.5;
            sithBot_nodes[nodeIdx].pos.y = (sithBot_nodes[nodeIdx].pos.y + center.y) * 0.5;
            sithBot_nodes[nodeIdx].pos.z = (sithBot_nodes[nodeIdx].pos.z + center.z) * 0.5;
            continue;
        }

        sithBot_nodes[nodeIdx].kind = SITHBOT_NODE_JUMPPAD;
        sithBot_nodes[nodeIdx].thingIdx = (int)surface->index;
        added++;
        sithBot_Logf("BotNav: jump-pad node=%d surface=%d sector=%d pos=(%.2f,%.2f,%.2f)\n",
                     nodeIdx,
                     (int)surface->index,
                     sithBot_GetSectorIndex(surface->parent_sector),
                     center.x,
                     center.y,
                     center.z);
    }

    return added;
}

static int sithBot_LinkJumpPadNodes(void)
{
    int added = 0;
    int i;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        SithBotNode *pad = &sithBot_nodes[i];
        SithBotLinkCandidate landingCandidates[SITHBOT_LINK_CANDIDATES];
        int incoming = 0;
        int landings = 0;
        int j;

        if (pad->kind != SITHBOT_NODE_JUMPPAD)
            continue;

        sithBot_ClearLinkCandidates(landingCandidates);
        for (j = 0; j < sithBot_numNodes; j++)
        {
            SithBotNode *candidate = &sithBot_nodes[j];
            flex_t dx;
            flex_t dy;
            flex_t dz;
            flex_t horizontalSq;
            int hasBetterNearby = 0;
            int k;

            if (j == i || candidate->kind == SITHBOT_NODE_ITEM ||
                candidate->kind == SITHBOT_NODE_LIFT ||
                candidate->kind == SITHBOT_NODE_JUMPPAD)
            {
                continue;
            }

            dx = candidate->pos.x - pad->pos.x;
            dy = candidate->pos.y - pad->pos.y;
            dz = candidate->pos.z - pad->pos.z;
            horizontalSq = dx * dx + dy * dy;

            if (sithBot_AbsFlex(dz) <= 0.35 && horizontalSq <= 1.44 &&
                (horizontalSq <= 0.25 ||
                 sithBot_CanSeePosition(candidate->sector, &candidate->pos,
                                        pad->sector, &pad->pos)))
            {
                incoming += sithBot_AddPreferredEdge(j, i);
            }

            if (dz < 0.35 || dz > 0.70 || horizontalSq < 0.95 || horizontalSq > 2.25 ||
                candidate->edgeCount <= 0)
            {
                continue;
            }

            for (k = 0; k < sithBot_numNodes; k++)
            {
                SithBotNode *nearby = &sithBot_nodes[k];
                flex_t nearbyDz;
                flex_t nearbyPadDx;
                flex_t nearbyPadDy;
                flex_t nearbyDx;
                flex_t nearbyDy;
                flex_t candidateScore;
                flex_t nearbyScore;

                if (k == j || nearby->kind == SITHBOT_NODE_ITEM ||
                    nearby->kind == SITHBOT_NODE_LIFT ||
                    nearby->kind == SITHBOT_NODE_JUMPPAD ||
                    nearby->edgeCount <= 0)
                {
                    continue;
                }
                nearbyDz = nearby->pos.z - pad->pos.z;
                nearbyPadDx = nearby->pos.x - pad->pos.x;
                nearbyPadDy = nearby->pos.y - pad->pos.y;
                if (nearbyDz < 0.35 || nearbyDz > 0.70 ||
                    nearbyPadDx * nearbyPadDx + nearbyPadDy * nearbyPadDy < 0.95 ||
                    nearbyPadDx * nearbyPadDx + nearbyPadDy * nearbyPadDy > 2.25)
                {
                    continue;
                }
                nearbyDx = nearby->pos.x - candidate->pos.x;
                nearbyDy = nearby->pos.y - candidate->pos.y;
                if (nearbyDx * nearbyDx + nearbyDy * nearbyDy > 0.1225 ||
                    sithBot_AbsFlex(nearby->pos.z - candidate->pos.z) > 0.15)
                {
                    continue;
                }
                candidateScore = horizontalSq + dz * dz * 1.50;
                nearbyScore = nearbyPadDx * nearbyPadDx + nearbyPadDy * nearbyPadDy +
                              nearbyDz * nearbyDz * 1.50;
                if (nearbyScore < candidateScore - 0.001 ||
                    (sithBot_AbsFlex(nearbyScore - candidateScore) <= 0.001 &&
                     nearby->edgeCount > candidate->edgeCount))
                {
                    hasBetterNearby = 1;
                    break;
                }
            }
            if (hasBetterNearby)
                continue;

            sithBot_InsertLinkCandidate(landingCandidates, j, horizontalSq + dz * dz * 1.50);
        }

        for (j = 0; j < 3 && j < SITHBOT_LINK_CANDIDATES; j++)
        {
            if (landingCandidates[j].nodeIdx < 0)
                break;
            landings += sithBot_AddEdge(i, landingCandidates[j].nodeIdx);
        }

        added += incoming + landings;
        sithBot_Logf("BotNav: jump-pad-links node=%d incoming=%d landings=%d edges=%d\n",
                     i, incoming, landings, pad->edgeCount);
    }

    return added;
}

static uint32_t sithBot_NavHashMix(uint32_t hash, uint32_t value)
{
    hash ^= value;
    hash *= 16777619u;
    return hash;
}

static uint32_t sithBot_NavHashString(uint32_t hash, const char *value)
{
    if (!value)
        return sithBot_NavHashMix(hash, 0);
    while (*value)
        hash = sithBot_NavHashMix(hash, (unsigned char)*value++);
    return sithBot_NavHashMix(hash, 0);
}

static uint32_t sithBot_NavHashFlex(uint32_t hash, flex_t value)
{
    int32_t quantized = (int32_t)(value * 4096.0);
    return sithBot_NavHashMix(hash, (uint32_t)quantized);
}

static uint32_t sithBot_GetNavWorldHash(void)
{
    sithWorld *world = sithWorld_pCurrentWorld;
    uint32_t hash = 2166136261u;
    int i;

    if (!world)
        return 0;

    hash = sithBot_NavHashString(hash, world->episodeName);
    hash = sithBot_NavHashString(hash, world->map_jkl_fname);
    hash = sithBot_NavHashMix(hash, (uint32_t)Main_bMotsCompat);
    hash = sithBot_NavHashMix(hash, (uint32_t)world->numSectors);
    hash = sithBot_NavHashMix(hash, (uint32_t)world->numVertices);
    hash = sithBot_NavHashMix(hash, (uint32_t)world->numSurfaces);
    hash = sithBot_NavHashMix(hash, (uint32_t)world->numThingsLoaded);
    hash = sithBot_NavHashMix(hash, (uint32_t)jkPlayer_maxPlayers);

    for (i = 0; i < world->numVertices; i++)
    {
        hash = sithBot_NavHashFlex(hash, world->vertices[i].x);
        hash = sithBot_NavHashFlex(hash, world->vertices[i].y);
        hash = sithBot_NavHashFlex(hash, world->vertices[i].z);
    }

    for (i = 0; i < world->numSurfaces; i++)
    {
        sithSurface *surface = &world->surfaces[i];
        rdFace *face = &surface->surfaceInfo.face;
        int j;

        hash = sithBot_NavHashMix(hash, surface->surfaceFlags);
        hash = sithBot_NavHashMix(hash, (uint32_t)(sithBot_GetSectorIndex(surface->parent_sector) + 1));
        hash = sithBot_NavHashMix(hash, (uint32_t)(surface->adjoin
            ? sithBot_GetSectorIndex(surface->adjoin->sector) + 1
            : 0));
        hash = sithBot_NavHashMix(hash, face->numVertices);
        if (!face->vertexPosIdx)
        {
            hash = sithBot_NavHashMix(hash, 0xFFFFFFFFu);
            continue;
        }
        for (j = 0; j < (int)face->numVertices; j++)
            hash = sithBot_NavHashMix(hash, (uint32_t)(face->vertexPosIdx[j] + 1));
    }

    for (i = 0; i < world->numThingsLoaded; i++)
    {
        sithThing *thing = &world->things[i];
        int j;

        if (thing->type != SITH_THING_ITEM && thing->moveType != SITH_MT_PATH)
            continue;
        hash = sithBot_NavHashMix(hash, (uint32_t)thing->thingIdx);
        hash = sithBot_NavHashMix(hash, thing->type);
        hash = sithBot_NavHashMix(hash, thing->moveType);
        hash = sithBot_NavHashMix(hash, thing->thingflags);
        hash = sithBot_NavHashString(hash, sithBot_GetThingDebugName(thing));
        hash = sithBot_NavHashFlex(hash, thing->position.x);
        hash = sithBot_NavHashFlex(hash, thing->position.y);
        hash = sithBot_NavHashFlex(hash, thing->position.z);
        hash = sithBot_NavHashFlex(hash, thing->moveSize);
        if (thing->moveType != SITH_MT_PATH || !thing->trackParams.aFrames)
        {
            hash = sithBot_NavHashMix(hash, 0);
            continue;
        }
        hash = sithBot_NavHashMix(hash, (uint32_t)thing->trackParams.loadedFrames);
        for (j = 0; j < thing->trackParams.loadedFrames; j++)
        {
            hash = sithBot_NavHashFlex(hash, thing->trackParams.aFrames[j].pos.x);
            hash = sithBot_NavHashFlex(hash, thing->trackParams.aFrames[j].pos.y);
            hash = sithBot_NavHashFlex(hash, thing->trackParams.aFrames[j].pos.z);
        }
    }

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        hash = sithBot_NavHashMix(hash, (uint32_t)(sithBot_GetSectorIndex(jkPlayer_playerInfos[i].pSpawnSector) + 1));
        hash = sithBot_NavHashFlex(hash, jkPlayer_playerInfos[i].spawnPosOrient.scale.x);
        hash = sithBot_NavHashFlex(hash, jkPlayer_playerInfos[i].spawnPosOrient.scale.y);
        hash = sithBot_NavHashFlex(hash, jkPlayer_playerInfos[i].spawnPosOrient.scale.z);
    }
    return hash;
}

static void sithBot_MakeNavCachePart(char *out, int outSize, const char *value, int stripExtension)
{
    int writeIdx = 0;

    if (!out || outSize <= 0)
        return;
    if (!value)
        value = "map";
    while (*value && writeIdx + 1 < outSize)
    {
        char ch = *value++;
        if (stripExtension && ch == '.')
            break;
        if (ch >= 'A' && ch <= 'Z')
            ch = (char)(ch - 'A' + 'a');
        if (!((ch >= 'a' && ch <= 'z') || (ch >= '0' && ch <= '9') || ch == '-' || ch == '_'))
            ch = '_';
        out[writeIdx++] = ch;
    }
    if (!writeIdx && outSize > 1)
    {
        out[0] = 'm';
        out[1] = 0;
        return;
    }
    out[writeIdx] = 0;
}

static void sithBot_GetNavCachePath(char *out, int outSize)
{
    char episode[40];
    char map[40];

    sithBot_MakeNavCachePart(episode, sizeof(episode),
                             sithWorld_pCurrentWorld ? sithWorld_pCurrentWorld->episodeName : "episode", 0);
    sithBot_MakeNavCachePart(map, sizeof(map),
                             sithWorld_pCurrentWorld ? sithWorld_pCurrentWorld->map_jkl_fname : "map", 1);
    _snprintf(out, outSize, "%s_%s.bnav", episode, map);
    out[outSize - 1] = 0;
}

static int sithBot_LoadNavCache(uint32_t worldHash)
{
    SithBotNavFileHeader header;
    SithBotNavFileNode diskNode;
    char path[96];
    int i;

    if (!sithWorld_pCurrentWorld)
        return 0;
    sithBot_GetNavCachePath(path, sizeof(path));
    if (!stdConffile_OpenReadBytesBypass(path))
        return 0;
    if (!stdConffile_Read(&header, sizeof(header)) ||
        header.magic != SITHBOT_BNAV_MAGIC || header.version != SITHBOT_BNAV_VERSION ||
        header.worldHash != worldHash || header.nodeCount == 0 || header.nodeCount > SITHBOT_MAX_NODES ||
        header.numSectors != sithWorld_pCurrentWorld->numSectors ||
        header.numThings != sithWorld_pCurrentWorld->numThingsLoaded ||
        header.numVertices != sithWorld_pCurrentWorld->numVertices ||
        header.numSurfaces != sithWorld_pCurrentWorld->numSurfaces)
    {
        stdConffile_Close();
        return 0;
    }

    sithBot_numNodes = 0;
    for (i = 0; i < (int)header.nodeCount; i++)
    {
        SithBotNode *node;
        int edgeIdx;

        if (!stdConffile_Read(&diskNode, sizeof(diskNode)) ||
            diskNode.sectorIdx < 0 || diskNode.sectorIdx >= sithWorld_pCurrentWorld->numSectors ||
            diskNode.thingIdx < -1 ||
            (diskNode.kind == SITHBOT_NODE_JUMPPAD
                ? diskNode.thingIdx >= sithWorld_pCurrentWorld->numSurfaces
                : diskNode.thingIdx >= sithWorld_pCurrentWorld->numThingsLoaded) ||
            diskNode.pathFrame < -1 || diskNode.pathFrame >= SITHBOT_MAX_LIFT_FRAMES ||
            diskNode.kind < SITHBOT_NODE_SPAWN || diskNode.kind > SITHBOT_NODE_JUMPPAD ||
            diskNode.edgeCount < 0 || diskNode.edgeCount > SITHBOT_MAX_EDGES ||
            diskNode.pos[0] != diskNode.pos[0] || diskNode.pos[1] != diskNode.pos[1] ||
            diskNode.pos[2] != diskNode.pos[2] ||
            sithBot_AbsFlex(diskNode.pos[0]) > 100000.0 ||
            sithBot_AbsFlex(diskNode.pos[1]) > 100000.0 ||
            sithBot_AbsFlex(diskNode.pos[2]) > 100000.0)
        {
            sithBot_numNodes = 0;
            stdConffile_Close();
            return 0;
        }

        if ((diskNode.kind == SITHBOT_NODE_LIFT &&
             (!sithBot_IsPathLiftThing(sithThing_GetThingByIdx(diskNode.thingIdx)) ||
              diskNode.pathFrame >= sithThing_GetThingByIdx(diskNode.thingIdx)->trackParams.loadedFrames)) ||
            (diskNode.kind != SITHBOT_NODE_LIFT && diskNode.pathFrame != -1))
        {
            sithBot_numNodes = 0;
            stdConffile_Close();
            return 0;
        }

        node = &sithBot_nodes[sithBot_numNodes++];
        node->pos.x = diskNode.pos[0];
        node->pos.y = diskNode.pos[1];
        node->pos.z = diskNode.pos[2];
        node->sector = &sithWorld_pCurrentWorld->sectors[diskNode.sectorIdx];
        node->thingIdx = diskNode.thingIdx;
        node->pathFrame = diskNode.pathFrame;
        node->kind = diskNode.kind;
        node->edgeCount = diskNode.edgeCount;
        for (edgeIdx = 0; edgeIdx < diskNode.edgeCount; edgeIdx++)
            node->edges[edgeIdx] = diskNode.edges[edgeIdx];
    }
    stdConffile_Close();

    for (i = 0; i < sithBot_numNodes; i++)
    {
        int edgeIdx;
        for (edgeIdx = 0; edgeIdx < sithBot_nodes[i].edgeCount; edgeIdx++)
        {
            if (sithBot_nodes[i].edges[edgeIdx] < 0 || sithBot_nodes[i].edges[edgeIdx] >= sithBot_numNodes)
            {
                sithBot_numNodes = 0;
                return 0;
            }
        }
    }

    sithBot_navWorld = sithWorld_pCurrentWorld;
    sithBot_navBuilt = 1;
    sithBot_Logf("BotNav: cache-load path='%s' nodes=%d hash=%08X\n",
                 path, sithBot_numNodes, (unsigned int)worldHash);
    return 1;
}

static int sithBot_SaveNavCache(uint32_t worldHash)
{
    SithBotNavFileHeader header;
    SithBotNavFileNode diskNode;
    char path[96];
    int i;

    if (!sithWorld_pCurrentWorld || sithBot_numNodes <= 0)
        return 0;
    sithBot_GetNavCachePath(path, sizeof(path));
    if (!stdConffile_OpenWriteBypass(path))
    {
        sithBot_Logf("BotNav: cache-write-failed path='%s'\n", path);
        return 0;
    }

    header.magic = SITHBOT_BNAV_MAGIC;
    header.version = SITHBOT_BNAV_VERSION;
    header.worldHash = worldHash;
    header.nodeCount = (uint32_t)sithBot_numNodes;
    header.numSectors = sithWorld_pCurrentWorld->numSectors;
    header.numThings = sithWorld_pCurrentWorld->numThingsLoaded;
    header.numVertices = sithWorld_pCurrentWorld->numVertices;
    header.numSurfaces = sithWorld_pCurrentWorld->numSurfaces;
    if (!stdConffile_Write((const char *)&header, sizeof(header)))
    {
        stdConffile_CloseWrite();
        return 0;
    }

    for (i = 0; i < sithBot_numNodes; i++)
    {
        SithBotNode *node = &sithBot_nodes[i];
        int edgeIdx;

        memset(&diskNode, 0, sizeof(diskNode));
        diskNode.pos[0] = (float)node->pos.x;
        diskNode.pos[1] = (float)node->pos.y;
        diskNode.pos[2] = (float)node->pos.z;
        diskNode.sectorIdx = sithBot_GetSectorIndex(node->sector);
        diskNode.thingIdx = node->thingIdx;
        diskNode.pathFrame = node->pathFrame;
        diskNode.kind = node->kind;
        diskNode.edgeCount = node->edgeCount;
        for (edgeIdx = 0; edgeIdx < node->edgeCount; edgeIdx++)
            diskNode.edges[edgeIdx] = node->edges[edgeIdx];
        if (!stdConffile_Write((const char *)&diskNode, sizeof(diskNode)))
        {
            stdConffile_CloseWrite();
            return 0;
        }
    }
    stdConffile_CloseWrite();
    sithBot_Logf("BotNav: cache-save path='%s' nodes=%d hash=%08X\n",
                 path, sithBot_numNodes, (unsigned int)worldHash);
    return 1;
}

static void sithBot_BuildNav(void)
{
    int i;
    int j;
    int edgeCount = 0;
    int portalEdges = 0;
    int portalNodes = 0;
    int liftSectors = 0;
    int inferredLiftSectors = 0;
    int pathLifts = 0;
    int pathLiftStops = 0;
    int pathLiftEdges = 0;
    int jumpPads = 0;
    int jumpPadEdges = 0;
    uint32_t startMs = stdPlatform_GetTimeMsec();
    uint32_t worldHash;

    sithBot_numNodes = 0;
    sithBot_numInferredLiftSectors = 0;
    memset(sithBot_inferredLiftSectors, 0, sizeof(sithBot_inferredLiftSectors));
    if (!sithWorld_pCurrentWorld)
        return;

    inferredLiftSectors = sithBot_InferLiftSectorsFromCogs();
    worldHash = sithBot_GetNavWorldHash();
    if (sithBot_LoadNavCache(worldHash))
    {
        sithBot_LogNavComponents();
        sithBot_Logf("BotNav: ready source=cache elapsedMs=%u map='%s' episode='%s'\n",
                     (unsigned int)(stdPlatform_GetTimeMsec() - startMs),
                     sithWorld_pCurrentWorld->map_jkl_fname,
                     sithWorld_pCurrentWorld->episodeName);
        return;
    }

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        if (sithBot_IsSectorSafeForBot(jkPlayer_playerInfos[i].pSpawnSector))
            sithBot_AddNode(&jkPlayer_playerInfos[i].spawnPosOrient.scale, jkPlayer_playerInfos[i].pSpawnSector, SITHBOT_NODE_SPAWN, i, 1.0);
    }

    for (i = 0; i < sithWorld_pCurrentWorld->numThingsLoaded; i++)
    {
        sithThing *thing = &sithWorld_pCurrentWorld->things[i];
        if (thing->type == SITH_THING_ITEM && sithBot_IsSectorSafeForBot(thing->sector))
            sithBot_AddNode(&thing->position, thing->sector, SITHBOT_NODE_ITEM, thing->thingIdx, 1.25);
    }

    for (i = 0; i < sithWorld_pCurrentWorld->numSectors; i++)
    {
        sithSector *sector = &sithWorld_pCurrentWorld->sectors[i];
        for (j = 0; j < (int)sector->numSurfaces; j++)
            sithBot_AddSurfaceNode(&sector->surfaces[j]);
        if (sithBot_IsUpwardThrustSector(sector))
        {
            if (sithBot_AddNode(&sector->center, sector, SITHBOT_NODE_LIFT, -1, 1.0) >= 0)
                liftSectors++;
        }
    }

    jumpPads = sithBot_AddJumpPadNodes();
    portalNodes = sithBot_AddPortalApproachNodes();
    pathLifts = sithBot_AddPathLiftNodes(&pathLiftStops, &pathLiftEdges);

    /* Reserve graph capacity for room-to-room travel before dense floor samples
       consume every edge slot with local alternatives. */
    portalEdges = sithBot_LinkAdjoinPortals();
    sithBot_LinkNodes();
    jumpPadEdges = sithBot_LinkJumpPadNodes();
    sithBot_LogNavComponents();
    for (i = 0; i < sithBot_numNodes; i++)
        edgeCount += sithBot_nodes[i].edgeCount;

    sithBot_navWorld = sithWorld_pCurrentWorld;
    sithBot_navBuilt = 1;
    sithBot_SaveNavCache(worldHash);
    sithBot_Logf("BotNav: generated nodes=%d directedEdges=%d portalNodes=%d portalEdges=%d liftSectors=%d inferredLiftSectors=%d pathLifts=%d pathLiftStops=%d pathLiftEdges=%d jumpPads=%d jumpPadEdges=%d map='%s' episode='%s'\n",
                 sithBot_numNodes,
                 edgeCount,
                 portalNodes,
                 portalEdges,
                 liftSectors,
                 inferredLiftSectors,
                 pathLifts,
                 pathLiftStops,
                 pathLiftEdges,
                 jumpPads,
                 jumpPadEdges,
                 sithWorld_pCurrentWorld->map_jkl_fname,
                 sithWorld_pCurrentWorld->episodeName);
    sithBot_Logf("BotNav: ready source=generated elapsedMs=%u map='%s' episode='%s'\n",
                 (unsigned int)(stdPlatform_GetTimeMsec() - startMs),
                 sithWorld_pCurrentWorld->map_jkl_fname,
                 sithWorld_pCurrentWorld->episodeName);
}

static int sithBot_FindNearestNodeAt(sithSector *sector, const rdVector3 *pos)
{
    int i;
    int best = -1;
    flex_t bestDist = 3.4e38f;

    if (!sector || !pos)
        return -1;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        flex_t distSq;
        if (!sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
            continue;
        distSq = sithBot_DistSq(pos, &sithBot_nodes[i].pos);
        if (distSq >= bestDist || distSq > 64.0)
            continue;
        if (!sithBot_CanSeePosition(sector, pos, sithBot_nodes[i].sector, &sithBot_nodes[i].pos))
            continue;
        if (distSq < bestDist)
        {
            SithBotNode here;
            rdVector_Copy3(&here.pos, pos);
            here.sector = sector;
            here.kind = sithBot_IsUpwardThrustSector(sector) ? SITHBOT_NODE_LIFT : SITHBOT_NODE_FLOOR;
            here.thingIdx = -1;
            here.edgeCount = 0;
            if (sithBot_IsWalkableSegment(&here, &sithBot_nodes[i]))
            {
                bestDist = distSq;
                best = i;
            }
        }
    }
    return best;
}

static int sithBot_FindNearestNode(sithThing *thing)
{
    if (!thing)
        return -1;
    return sithBot_FindNearestNodeAt(thing->sector, &thing->position);
}

static int sithBot_FindNavigableNodeAt(sithSector *sector, const rdVector3 *pos)
{
    SithBotNode here;
    int nearest;
    int best = -1;
    int i;
    flex_t bestDist = 3.4e38f;

    nearest = sithBot_FindNearestNodeAt(sector, pos);
    if (nearest >= 0 && sithBot_nodes[nearest].edgeCount > 0)
        return nearest;

    if (!sector || !pos)
        return -1;

    rdVector_Copy3(&here.pos, pos);
    here.sector = sector;
    here.kind = sithBot_IsUpwardThrustSector(sector) ? SITHBOT_NODE_LIFT : SITHBOT_NODE_FLOOR;
    here.thingIdx = -1;
    here.edgeCount = 0;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        flex_t distSq;

        if (sithBot_nodes[i].edgeCount <= 0 || !sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
            continue;
        distSq = sithBot_DistSq(pos, &sithBot_nodes[i].pos);
        if (distSq >= bestDist || distSq > 64.0)
            continue;
        if (!sithBot_CanSeePosition(sector, pos, sithBot_nodes[i].sector, &sithBot_nodes[i].pos))
            continue;
        if (!sithBot_IsWalkableSegment(&here, &sithBot_nodes[i]))
            continue;
        bestDist = distSq;
        best = i;
    }

    return best;
}

static int sithBot_IsSpawnNavigable(int spawnIdx)
{
    int visited[SITHBOT_MAX_NODES];
    int queue[SITHBOT_MAX_NODES];
    int head = 0;
    int tail = 0;
    int startNode;

    if (spawnIdx < 0 || spawnIdx >= jkPlayer_maxPlayers || !jkPlayer_playerInfos[spawnIdx].pSpawnSector)
        return 0;

    startNode = sithBot_FindNavigableNodeAt(jkPlayer_playerInfos[spawnIdx].pSpawnSector,
                                            &jkPlayer_playerInfos[spawnIdx].spawnPosOrient.scale);
    if (startNode < 0)
        return 0;

    memset(visited, 0, sizeof(visited));
    visited[startNode] = 1;
    queue[tail++] = startNode;
    while (head < tail)
    {
        int nodeIdx = queue[head++];
        SithBotNode *node = &sithBot_nodes[nodeIdx];
        int i;

        if (nodeIdx != startNode && node->kind == SITHBOT_NODE_ITEM)
            continue;

        for (i = 0; i < node->edgeCount; i++)
        {
            int next = node->edges[i];
            if (next < 0 || next >= sithBot_numNodes || visited[next])
                continue;
            visited[next] = 1;
            queue[tail++] = next;
        }
    }

    return tail >= 12 || tail * 2 >= sithBot_numNodes;
}

static int sithBot_ChooseSpawnIdx(sithThing *thing, int preferredIdx)
{
    int playerIdx;
    int attempt;
    int candidate;
    int start;

    if (sithBot_IsSpawnNavigable(preferredIdx))
        return preferredIdx;

    for (attempt = 0; attempt < jkPlayer_maxPlayers * 2; attempt++)
    {
        candidate = sithMulti_GetSpawnIdx(thing);
        if (sithBot_IsSpawnNavigable(candidate))
            return candidate;
    }

    playerIdx = sithBot_GetPlayerSlotForThing(thing);
    start = playerIdx >= 0 ? playerIdx : 0;
    for (attempt = 0; attempt < jkPlayer_maxPlayers; attempt++)
    {
        candidate = (start + attempt) % jkPlayer_maxPlayers;
        if (sithBot_IsSpawnNavigable(candidate))
            return candidate;
    }

    return preferredIdx;
}

static int sithBot_IsRouteEdgeBlocked(int fromNode, int toNode)
{
    int i;

    for (i = 0; i < SITHBOT_MAX_BLOCKED_EDGES; i++)
    {
        SithBotBlockedEdge *blocked = &sithBot_blockedEdges[i];
        if (blocked->untilMs > sithTime_curMs &&
            blocked->fromNode == fromNode && blocked->toNode == toNode)
        {
            return 1;
        }
    }
    return 0;
}

static void sithBot_BlockRouteEdge(int fromNode, int toNode)
{
    int i;
    int slot = -1;
    uint32_t oldestUntil = 0xFFFFFFFFu;

    if (fromNode < 0 || toNode < 0 || fromNode >= sithBot_numNodes ||
        toNode >= sithBot_numNodes || fromNode == toNode)
    {
        return;
    }
    for (i = 0; i < sithBot_nodes[fromNode].edgeCount; i++)
    {
        if (sithBot_nodes[fromNode].edges[i] == toNode)
            break;
    }
    if (i >= sithBot_nodes[fromNode].edgeCount)
        return;

    for (i = 0; i < SITHBOT_MAX_BLOCKED_EDGES; i++)
    {
        SithBotBlockedEdge *blocked = &sithBot_blockedEdges[i];
        if (blocked->untilMs > sithTime_curMs &&
            blocked->fromNode == fromNode && blocked->toNode == toNode)
        {
            slot = i;
            break;
        }
        if (blocked->untilMs <= sithTime_curMs)
        {
            slot = i;
            break;
        }
        if (blocked->untilMs < oldestUntil)
        {
            oldestUntil = blocked->untilMs;
            slot = i;
        }
    }
    if (slot < 0)
        return;

    sithBot_blockedEdges[slot].fromNode = fromNode;
    sithBot_blockedEdges[slot].toNode = toNode;
    sithBot_blockedEdges[slot].untilMs = sithTime_curMs + 600000;
    sithBot_Logf("BotMatch: route-edge-blocked from=%d to=%d until=%u\n",
                 fromNode, toNode, (unsigned int)sithBot_blockedEdges[slot].untilMs);
}

static flex_t sithBot_GetRouteEdgeCost(int fromNode, int toNode)
{
    flex_t cost;
    flex_t rise;

    cost = rdVector_Dist3(&sithBot_nodes[fromNode].pos, &sithBot_nodes[toNode].pos);
    if (cost < 0.05)
        cost = 0.05;

    rise = sithBot_nodes[toNode].pos.z - sithBot_nodes[fromNode].pos.z;
    if (rise > 0.20 && sithBot_nodes[fromNode].kind != SITHBOT_NODE_JUMPPAD)
        cost += rise * 1.75;
    if (sithBot_nodes[toNode].kind == SITHBOT_NODE_SPAWN)
        cost += 0.12;
    return cost;
}

static int sithBot_FindPathNextWeighted(int startNode, int goalNode, sithSector *avoidSector)
{
    int prev[SITHBOT_MAX_NODES];
    int heap[SITHBOT_MAX_NODES];
    int heapPos[SITHBOT_MAX_NODES];
    unsigned char closed[SITHBOT_MAX_NODES];
    flex_t routeCost[SITHBOT_MAX_NODES];
    flex_t priority[SITHBOT_MAX_NODES];
    int heapCount = 0;
    int i;
    int node;

    if (startNode < 0 || goalNode < 0 || startNode >= sithBot_numNodes || goalNode >= sithBot_numNodes)
        return -1;
    if (startNode == goalNode)
        return goalNode;
    if (avoidSector && sithBot_nodes[goalNode].sector == avoidSector)
        return -1;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        prev[i] = -2;
        heapPos[i] = -1;
        closed[i] = 0;
        routeCost[i] = 3.4e38f;
        priority[i] = 3.4e38f;
    }

    routeCost[startNode] = 0.0;
    priority[startNode] = rdVector_Dist3(&sithBot_nodes[startNode].pos, &sithBot_nodes[goalNode].pos);
    prev[startNode] = -1;
    heap[0] = startNode;
    heapPos[startNode] = 0;
    heapCount = 1;

    while (heapCount > 0)
    {
        int root;
        int last;
        int pos;

        node = heap[0];
        heapPos[node] = -1;
        heapCount--;
        if (heapCount > 0)
        {
            heap[0] = heap[heapCount];
            heapPos[heap[0]] = 0;
            pos = 0;
            for (;;)
            {
                int left = pos * 2 + 1;
                int right = left + 1;
                int best = pos;
                if (left < heapCount && priority[heap[left]] < priority[heap[best]])
                    best = left;
                if (right < heapCount && priority[heap[right]] < priority[heap[best]])
                    best = right;
                if (best == pos)
                    break;
                root = heap[pos];
                heap[pos] = heap[best];
                heap[best] = root;
                heapPos[heap[pos]] = pos;
                heapPos[heap[best]] = best;
                pos = best;
            }
        }

        if (closed[node])
            continue;
        closed[node] = 1;
        if (node == goalNode)
            break;
        if (node != startNode && sithBot_nodes[node].kind == SITHBOT_NODE_ITEM)
            continue;

        for (i = 0; i < sithBot_nodes[node].edgeCount; i++)
        {
            int next = sithBot_nodes[node].edges[i];
            flex_t candidateCost;
            flex_t candidatePriority;

            if (closed[next] || sithBot_IsRouteEdgeBlocked(node, next))
                continue;
            if (avoidSector && sithBot_nodes[next].sector == avoidSector && next != goalNode)
                continue;
            if (sithBot_IsDynamicHazardSector(sithBot_nodes[next].sector) && next != goalNode)
                continue;

            candidateCost = routeCost[node] + sithBot_GetRouteEdgeCost(node, next);
            if (candidateCost >= routeCost[next])
                continue;

            routeCost[next] = candidateCost;
            candidatePriority = candidateCost +
                rdVector_Dist3(&sithBot_nodes[next].pos, &sithBot_nodes[goalNode].pos);
            priority[next] = candidatePriority;
            prev[next] = node;

            if (heapPos[next] < 0)
            {
                heapPos[next] = heapCount;
                heap[heapCount++] = next;
            }
            pos = heapPos[next];
            while (pos > 0)
            {
                int parent = (pos - 1) / 2;
                if (priority[heap[parent]] <= priority[heap[pos]])
                    break;
                last = heap[parent];
                heap[parent] = heap[pos];
                heap[pos] = last;
                heapPos[heap[parent]] = parent;
                heapPos[heap[pos]] = pos;
                pos = parent;
            }
        }
    }

    if (prev[goalNode] == -2)
        return -1;

    node = goalNode;
    while (prev[node] != -1 && prev[node] != startNode)
        node = prev[node];
    return node;
}

static int sithBot_FindPathNext(int startNode, int goalNode)
{
    return sithBot_FindPathNextWeighted(startNode, goalNode, 0);
}

static int sithBot_FindPathNextAvoidSector(int startNode, int goalNode, sithSector *avoidSector)
{
    return sithBot_FindPathNextWeighted(startNode, goalNode, avoidSector);
}

static void sithBot_MarkReachableNodes(int startNode, sithSector *avoidSector,
                                       unsigned char reachable[SITHBOT_MAX_NODES])
{
    int queue[SITHBOT_MAX_NODES];
    int head = 0;
    int tail = 0;
    int i;

    memset(reachable, 0, SITHBOT_MAX_NODES);
    if (startNode < 0 || startNode >= sithBot_numNodes)
        return;

    reachable[startNode] = 1;
    queue[tail++] = startNode;
    while (head < tail)
    {
        int node = queue[head++];
        if (node != startNode && sithBot_nodes[node].kind == SITHBOT_NODE_ITEM)
            continue;
        for (i = 0; i < sithBot_nodes[node].edgeCount; i++)
        {
            int next = sithBot_nodes[node].edges[i];
            if (reachable[next] || sithBot_IsRouteEdgeBlocked(node, next))
                continue;
            if (avoidSector && sithBot_nodes[next].sector == avoidSector)
                continue;
            if (sithBot_IsDynamicHazardSector(sithBot_nodes[next].sector))
                continue;
            reachable[next] = 1;
            queue[tail++] = next;
        }
    }
}

static int sithBot_CanSkipRouteNode(sithThing *thing, int candidateNode)
{
    SithBotNode here;
    rdVector3 flatDir;
    flex_t flatDist;
    flex_t probeDist;

    if (!thing || !thing->sector || candidateNode < 0 || candidateNode >= sithBot_numNodes)
        return 0;

    rdVector_Sub3(&flatDir, &sithBot_nodes[candidateNode].pos, &thing->position);
    flatDir.z = 0.0;
    flatDist = rdVector_Normalize3Acc(&flatDir);
    if (flatDist <= 0.001)
        return 0;

    probeDist = flatDist;
    if (probeDist > 0.55)
        probeDist = 0.55;
    if (!sithBot_IsMoveStepSafe(thing, &flatDir, probeDist))
        return 0;

    rdVector_Copy3(&here.pos, &thing->position);
    here.sector = thing->sector;
    here.kind = sithBot_IsUpwardThrustSector(thing->sector) ? SITHBOT_NODE_LIFT : SITHBOT_NODE_FLOOR;
    here.thingIdx = -1;
    here.edgeCount = 0;
    return sithBot_IsWalkableSegment(&here, &sithBot_nodes[candidateNode]);
}

static int sithBot_FindRouteMoveNode(int startNode, int goalNode, sithThing *thing)
{
    int nextNode;
    int skipped = 0;

    nextNode = sithBot_FindPathNext(startNode, goalNode);
    while (thing && nextNode >= 0 && nextNode != goalNode && skipped < 3)
    {
        int candidateNode;
        flex_t dx = thing->position.x - sithBot_nodes[nextNode].pos.x;
        flex_t dy = thing->position.y - sithBot_nodes[nextNode].pos.y;
        flex_t dz = sithBot_AbsFlex(thing->position.z - sithBot_nodes[nextNode].pos.z);
        flex_t reachRadius = sithBot_nodes[nextNode].kind == SITHBOT_NODE_PORTAL ? 0.24 : 0.45;
        int sameSectorReached = thing->sector == sithBot_nodes[nextNode].sector &&
            sithBot_DistSq(&thing->position, &sithBot_nodes[nextNode].pos) < reachRadius * reachRadius;
        int overlappingTransition = dx * dx + dy * dy < 0.15 * 0.15 && dz < 0.40;
        int reachedPortal = sithBot_nodes[nextNode].kind == SITHBOT_NODE_PORTAL &&
            thing->sector == sithBot_nodes[nextNode].sector &&
            sithBot_DistSq(&thing->position, &sithBot_nodes[nextNode].pos) < 0.24 * 0.24;

        if (!sameSectorReached && !overlappingTransition)
            break;
        startNode = nextNode;
        candidateNode = sithBot_FindPathNext(startNode, goalNode);
        if (!reachedPortal && !overlappingTransition &&
            !sithBot_CanSkipRouteNode(thing, candidateNode))
            break;
        nextNode = candidateNode;
        skipped++;
    }
    return nextNode;
}

static int sithBot_FindCommittedRouteMoveNode(SithBotState *state, int startNode, int goalNode, sithThing *thing)
{
    flex_t dist;
    int nextNode;
    int anchoredAtStart = 0;

    if (!state || !thing || goalNode < 0 || goalNode >= sithBot_numNodes)
        return -1;

    if (state->routeGoalNode == goalNode && state->nextNode >= 0 && state->nextNode < sithBot_numNodes)
    {
        flex_t dx;
        flex_t dy;
        flex_t dz;
        int reachedCommittedNode;

        dist = rdVector_Dist3(&thing->position, &sithBot_nodes[state->nextNode].pos);
        dx = thing->position.x - sithBot_nodes[state->nextNode].pos.x;
        dy = thing->position.y - sithBot_nodes[state->nextNode].pos.y;
        dz = sithBot_AbsFlex(thing->position.z - sithBot_nodes[state->nextNode].pos.z);
        reachedCommittedNode =
            (dist < (sithBot_nodes[state->nextNode].kind == SITHBOT_NODE_PORTAL ? 0.24 : 0.55) &&
             thing->sector == sithBot_nodes[state->nextNode].sector) ||
            (dx * dx + dy * dy < 0.15 * 0.15 && dz < 0.40);
        if (!sithBot_CanSeePosition(thing->sector, &thing->position,
                                    sithBot_nodes[state->nextNode].sector,
                                    &sithBot_nodes[state->nextNode].pos))
        {
            state->nextNode = -1;
            state->routeCommitUntilMs = 0;
        }
        else if (reachedCommittedNode)
        {
            startNode = state->nextNode;
            state->routeCommitUntilMs = 0;
        }
        else if (dist + 0.12 < state->routeBestDist)
        {
            state->routeBestDist = dist;
            state->routeCommitUntilMs = sithTime_curMs + SITHBOT_ROUTE_COMMIT_MS;
            return state->nextNode;
        }
        else if (sithTime_curMs < state->routeCommitUntilMs)
        {
            return state->nextNode;
        }
    }

    nextNode = sithBot_FindRouteMoveNode(startNode, goalNode, thing);
    if (startNode >= 0 && startNode < sithBot_numNodes &&
        ((thing->sector == sithBot_nodes[startNode].sector &&
          sithBot_DistSq(&thing->position, &sithBot_nodes[startNode].pos) < 0.55 * 0.55) ||
         ((thing->position.x - sithBot_nodes[startNode].pos.x) *
              (thing->position.x - sithBot_nodes[startNode].pos.x) +
          (thing->position.y - sithBot_nodes[startNode].pos.y) *
              (thing->position.y - sithBot_nodes[startNode].pos.y) < 0.15 * 0.15 &&
          sithBot_AbsFlex(thing->position.z - sithBot_nodes[startNode].pos.z) < 0.40)))
    {
        anchoredAtStart = 1;
    }
    if (nextNode >= 0 &&
        !anchoredAtStart &&
        !sithBot_CanSeePosition(thing->sector, &thing->position,
                                sithBot_nodes[nextNode].sector,
                                &sithBot_nodes[nextNode].pos))
    {
        /* Re-enter the path corridor through the nearest reachable anchor.
           Pursuit can otherwise drift beside a doorway and steer through the
           wall toward the waypoint on the far side. */
        if (startNode >= 0 && startNode < sithBot_numNodes && startNode != nextNode &&
            sithBot_CanSeePosition(thing->sector, &thing->position,
                                   sithBot_nodes[startNode].sector,
                                   &sithBot_nodes[startNode].pos))
        {
            nextNode = startNode;
        }
        else
        {
            nextNode = -1;
        }
    }

    state->routeGoalNode = goalNode;
    state->nextNode = nextNode;
    state->routeCommitUntilMs = sithTime_curMs + SITHBOT_ROUTE_COMMIT_MS;
    state->routeBestDist = nextNode >= 0 && nextNode < sithBot_numNodes
        ? rdVector_Dist3(&thing->position, &sithBot_nodes[nextNode].pos)
        : 3.4e38f;
    return nextNode;
}

static int sithBot_FindRouteMoveNodeAvoidSector(int startNode, int goalNode, sithSector *avoidSector, sithThing *thing)
{
    int nextNode;
    int skipped = 0;

    nextNode = sithBot_FindPathNextAvoidSector(startNode, goalNode, avoidSector);
    while (thing && nextNode >= 0 && nextNode != goalNode && skipped < 3)
    {
        int candidateNode;
        flex_t dx = thing->position.x - sithBot_nodes[nextNode].pos.x;
        flex_t dy = thing->position.y - sithBot_nodes[nextNode].pos.y;
        flex_t dz = sithBot_AbsFlex(thing->position.z - sithBot_nodes[nextNode].pos.z);
        flex_t reachRadius = sithBot_nodes[nextNode].kind == SITHBOT_NODE_PORTAL ? 0.24 : 0.45;
        int sameSectorReached = thing->sector == sithBot_nodes[nextNode].sector &&
            sithBot_DistSq(&thing->position, &sithBot_nodes[nextNode].pos) < reachRadius * reachRadius;
        int overlappingTransition = dx * dx + dy * dy < 0.15 * 0.15 && dz < 0.40;
        int reachedPortal = sithBot_nodes[nextNode].kind == SITHBOT_NODE_PORTAL &&
            thing->sector == sithBot_nodes[nextNode].sector &&
            sithBot_DistSq(&thing->position, &sithBot_nodes[nextNode].pos) < 0.24 * 0.24;

        if (!sameSectorReached && !overlappingTransition)
            break;
        startNode = nextNode;
        candidateNode = sithBot_FindPathNextAvoidSector(startNode, goalNode, avoidSector);
        if (!reachedPortal && !overlappingTransition &&
            !sithBot_CanSkipRouteNode(thing, candidateNode))
            break;
        nextNode = candidateNode;
        skipped++;
    }
    return nextNode;
}

static int sithBot_IsNodeReachableFromThing(sithThing *thing, int startNode, int nodeIdx)
{
    SithBotNode here;

    if (!thing || nodeIdx < 0 || nodeIdx >= sithBot_numNodes)
        return 0;

    if (startNode >= 0 && (nodeIdx == startNode || sithBot_FindPathNext(startNode, nodeIdx) >= 0))
        return 1;

    rdVector_Copy3(&here.pos, &thing->position);
    here.sector = thing->sector;
    here.kind = sithBot_IsUpwardThrustSector(thing->sector) ? SITHBOT_NODE_LIFT : SITHBOT_NODE_FLOOR;
    here.thingIdx = -1;
    here.edgeCount = 0;
    return sithBot_IsWalkableSegment(&here, &sithBot_nodes[nodeIdx]);
}

static int sithBot_IsThingAlivePlayer(sithThing *thing)
{
    return thing
        && thing->type == SITH_THING_PLAYER
        && thing->actorParams.playerinfo
        && (thing->actorParams.playerinfo->flags & 1)
        && !(thing->thingflags & (SITH_TF_DISABLED | SITH_TF_DEAD | SITH_TF_WILLBEREMOVED))
        && thing->actorParams.health > 0.0;
}

static int sithBot_FindEnemy(SithBotState *state, sithThing *botThing)
{
    int i;
    int best = -1;
    flex_t bestDist = 3.4e38f;
    int botIdx;

    if (!state || !botThing)
        return -1;

    botIdx = state->playerIdx;

    /* Keep the current opponent until it is dead or genuinely out of sight.
       Both UT and JA preserve an enemy state; changing to the nearest visible
       player every tick makes aim and strafing oscillate in group fights. */
    if (state->enemyIdx >= 0 && state->enemyIdx < jkPlayer_maxPlayers &&
        state->enemyIdx != botIdx &&
        !sithBot_IsAutostartServerPlaceholder(state->enemyIdx))
    {
        sithThing *current = jkPlayer_playerInfos[state->enemyIdx].playerThing;
        if (sithBot_IsThingAlivePlayer(current) &&
            !((sithNet_MultiModeFlags & MULTIMODEFLAG_TEAMS) &&
              sithPlayer_sub_4C9060(botThing, current)))
        {
            if (sithBot_HasCombatLos(botThing, current) ||
                (state->lastSeenEnemyIdx == state->enemyIdx &&
                 sithTime_curMs - state->lastEnemySeenMs <= SITHBOT_TARGET_COMMIT_MS))
            {
                return state->enemyIdx;
            }
        }
    }

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        sithThing *candidate;
        flex_t distSq;

        if (i == botIdx)
            continue;
        if (sithBot_IsAutostartServerPlaceholder(i))
            continue;
        candidate = jkPlayer_playerInfos[i].playerThing;
        if (!sithBot_IsThingAlivePlayer(candidate))
            continue;
        if ((sithNet_MultiModeFlags & MULTIMODEFLAG_TEAMS) && sithPlayer_sub_4C9060(botThing, candidate))
            continue;

        distSq = sithBot_DistSq(&botThing->position, &candidate->position);
        if (!sithBot_HasCombatLos(botThing, candidate))
            continue;
        if (distSq < bestDist)
        {
            bestDist = distSq;
            best = i;
        }
    }
    return best;
}

static int sithBot_WeaponAvailable(sithThing *thing, int bin)
{
    sithItemDescriptor *desc;
    const SithBotWeaponSpec *spec = 0;
    int i;

    if (!thing || bin <= 0 || bin >= SITHBIN_NUMBINS)
        return 0;

    desc = sithInventory_GetBinByIdx(bin);
    if (!(desc
        && (desc->flags & ITEMINFO_WEAPON)
        && sithInventory_GetAvailable(thing, bin)
        && sithInventory_GetBinAmount(thing, bin) > 0.0))
        return 0;

    for (i = 0; i < (int)(sizeof(sithBot_weaponSpecs) / sizeof(sithBot_weaponSpecs[0])); i++)
    {
        int specBin = Main_bMotsCompat ? sithBot_weaponSpecs[i].motsBin : sithBot_weaponSpecs[i].jkBin;
        if (specBin == bin)
        {
            spec = &sithBot_weaponSpecs[i];
            break;
        }
    }

    if (!spec || spec->ammoBin <= 0 || spec->ammoCost <= 0.0)
        return 1;

    return sithInventory_GetBinAmount(thing, spec->ammoBin) >= spec->ammoCost;
}

static int sithBot_IsSpecSafeForFireAtDist(sithThing *thing, const SithBotWeaponSpec *spec, flex_t enemyDist)
{
    if (!spec)
        return 0;
    if (spec->selfSafeDist > 0.0 && enemyDist < spec->selfSafeDist)
        return 0;
    if (sithBot_IsBlastWeaponSpec(spec) && thing && thing->actorParams.health < 85.0 &&
        enemyDist < spec->selfSafeDist + 1.5)
        return 0;
    return 1;
}

static int sithBot_ChooseWeaponEx(sithThing *thing, flex_t enemyDist, int safeForFire)
{
    int best = -1;
    int hasRangedWeapon = 0;
    flex_t bestScore = -3.4e38f;
    int saber = Main_bMotsCompat ? SITHBIN_MOTS_LIGHTSABER : SITHBIN_LIGHTSABER;
    int i;

    if (safeForFire && enemyDist < 0.45 && sithBot_WeaponAvailable(thing, saber))
        return saber;

    for (i = 0; i < (int)(sizeof(sithBot_weaponSpecs) / sizeof(sithBot_weaponSpecs[0])); i++)
    {
        const SithBotWeaponSpec *spec = &sithBot_weaponSpecs[i];
        int bin = Main_bMotsCompat ? spec->motsBin : spec->jkBin;
        flex_t distPenalty;
        flex_t score;

        if (!sithBot_WeaponAvailable(thing, bin))
            continue;
        hasRangedWeapon = 1;
        if (safeForFire && !sithBot_IsSpecSafeForFireAtDist(thing, spec, enemyDist))
            continue;

        distPenalty = enemyDist - spec->idealDist;
        if (distPenalty < 0.0)
            distPenalty = -distPenalty;
        score = spec->score - distPenalty * 16.0 + _frand() * 6.0;
        if (enemyDist < spec->minDist)
            score -= (spec->minDist - enemyDist) * 320.0;
        if (enemyDist > spec->maxDist)
            score -= (enemyDist - spec->maxDist) * 28.0;

        if (score > bestScore)
        {
            bestScore = score;
            best = bin;
        }
    }

    if (best >= 0)
    {
        if (enemyDist < 0.40 && sithBot_WeaponAvailable(thing, saber))
            return saber;
        return best;
    }
    if (safeForFire && hasRangedWeapon && enemyDist >= 1.25)
        return -1;

    if (sithBot_WeaponAvailable(thing, saber))
        return saber;

    {
        int bryar = Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL;
        if (sithBot_WeaponAvailable(thing, bryar))
            return bryar;
    }

    return Main_bMotsCompat ? SITHBIN_MOTS_FISTS : SITHBIN_FISTS;
}

static int sithBot_ChooseWeapon(sithThing *thing, flex_t enemyDist)
{
    return sithBot_ChooseWeaponEx(thing, enemyDist, 0);
}

static int sithBot_ChooseFireWeapon(sithThing *thing, flex_t enemyDist)
{
    return sithBot_ChooseWeaponEx(thing, enemyDist, 1);
}

static int sithBot_IsSaberBin(int weaponBin)
{
    if (Main_bMotsCompat)
        return weaponBin == SITHBIN_MOTS_LIGHTSABER;
    return weaponBin == SITHBIN_LIGHTSABER;
}

static const SithBotWeaponSpec *sithBot_GetWeaponSpec(int weaponBin)
{
    int i;

    for (i = 0; i < (int)(sizeof(sithBot_weaponSpecs) / sizeof(sithBot_weaponSpecs[0])); i++)
    {
        int specBin = Main_bMotsCompat ? sithBot_weaponSpecs[i].motsBin : sithBot_weaponSpecs[i].jkBin;
        if (specBin == weaponBin)
            return &sithBot_weaponSpecs[i];
    }
    return 0;
}

static int sithBot_IsBlastWeaponSpec(const SithBotWeaponSpec *spec)
{
    return spec && spec->projectileName &&
        (strstr(spec->projectileName, "conc") ||
         strstr(spec->projectileName, "rail") ||
         strstr(spec->projectileName, "crossbow"));
}

static int sithBot_ChooseNonBlastWeapon(sithThing *thing, flex_t enemyDist)
{
    int bins[6];
    int i;

    bins[0] = Main_bMotsCompat ? SITHBIN_MOTS_REPEATER : SITHBIN_REPEATER;
    bins[1] = Main_bMotsCompat ? SITHBIN_MOTS_STORMTROOPER_RIFLE : SITHBIN_STORMTROOPER_RIFLE;
    bins[2] = Main_bMotsCompat ? SITHBIN_MOTS_STORMTROOPER_SCOPE : -1;
    bins[3] = Main_bMotsCompat ? SITHBIN_MOTS_BLASTECH : -1;
    bins[4] = Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL;
    bins[5] = Main_bMotsCompat ? SITHBIN_MOTS_LIGHTSABER : SITHBIN_LIGHTSABER;

    for (i = 0; i < 6; i++)
    {
        const SithBotWeaponSpec *spec;
        if (!sithBot_WeaponAvailable(thing, bins[i]))
            continue;
        spec = sithBot_GetWeaponSpec(bins[i]);
        if (!spec || sithBot_IsSpecSafeForFireAtDist(thing, spec, enemyDist))
            return bins[i];
    }
    return -1;
}

static const SithBotWeaponSpec *sithBot_GetBestOwnedRangedSpec(sithThing *thing, int *weaponBinOut)
{
    const SithBotWeaponSpec *best = 0;
    flex_t bestScore = -3.4e38f;
    int bestBin = -1;
    int i;

    for (i = 0; i < (int)(sizeof(sithBot_weaponSpecs) / sizeof(sithBot_weaponSpecs[0])); i++)
    {
        const SithBotWeaponSpec *spec = &sithBot_weaponSpecs[i];
        int bin = Main_bMotsCompat ? spec->motsBin : spec->jkBin;
        if (!sithBot_WeaponAvailable(thing, bin))
            continue;
        if (spec->score > bestScore)
        {
            bestScore = spec->score;
            best = spec;
            bestBin = bin;
        }
    }

    if (weaponBinOut)
        *weaponBinOut = bestBin;
    return best;
}

static const char *sithBot_GetItemTemplateName(sithThing *item)
{
    if (!item)
        return "";
    if (item->templateBase && item->templateBase->template_name[0])
        return item->templateBase->template_name;
    if (item->template_name[0])
        return item->template_name;
    return "";
}

static int sithBot_GetWeaponBinForItemName(const char *name)
{
    if (!name || !name[0])
        return -1;

    if (strstr(name, "railcharge") ||
        strstr(name, "powercell") ||
        strstr(name, "energycell") ||
        strstr(name, "shield") ||
        strstr(name, "armor") ||
        strstr(name, "bacta"))
        return -1;

    if (strstr(name, "concrifle") || strstr(name, "concussion"))
        return Main_bMotsCompat ? SITHBIN_MOTS_CONCUSSION_RIFLE : SITHBIN_CONCUSSION_RIFLE;
    if (strstr(name, "railgun") || strstr(name, "raildet"))
        return Main_bMotsCompat ? SITHBIN_MOTS_RAIL_DETONATOR : SITHBIN_RAIL_DETONATOR;
    if (strstr(name, "crossbow") || strstr(name, "tusken"))
        return Main_bMotsCompat ? SITHBIN_MOTS_TUSKEN_PROD : SITHBIN_TUSKEN_PROD;
    if (strstr(name, "repeater"))
        return Main_bMotsCompat ? SITHBIN_MOTS_REPEATER : SITHBIN_REPEATER;
    if (Main_bMotsCompat &&
        (strstr(name, "stormtrooper_scope") || strstr(name, "stscope") || strstr(name, "scope")))
        return SITHBIN_MOTS_STORMTROOPER_SCOPE;
    if (Main_bMotsCompat &&
        (strstr(name, "blastech") || strstr(name, "greedopistol") || strstr(name, "bryarpistol")))
        return SITHBIN_MOTS_BLASTECH;
    if (strstr(name, "strifle") || strstr(name, "storm"))
        return Main_bMotsCompat ? SITHBIN_MOTS_STORMTROOPER_RIFLE : SITHBIN_STORMTROOPER_RIFLE;
    if (strstr(name, "bryar"))
        return Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL;
    if (strstr(name, "saber"))
        return Main_bMotsCompat ? SITHBIN_MOTS_LIGHTSABER : SITHBIN_LIGHTSABER;

    return -1;
}

static flex_t sithBot_GetItemDesire(sithThing *thing, sithThing *item)
{
    const char *name;
    int weaponBin;
    flex_t desire = 3.0;
    flex_t shields;
    const SithBotWeaponSpec *bestSpec;
    const SithBotWeaponSpec *itemSpec;

    if (!thing || !sithBot_IsItemAvailable(item))
        return -12.0;

    name = sithBot_GetItemTemplateName(item);
    weaponBin = sithBot_GetWeaponBinForItemName(name);
    bestSpec = sithBot_GetBestOwnedRangedSpec(thing, 0);
    itemSpec = weaponBin > 0 ? sithBot_GetWeaponSpec(weaponBin) : 0;

    if (weaponBin > 0)
    {
        if (!sithBot_WeaponAvailable(thing, weaponBin))
        {
            desire += bestSpec && bestSpec->score >= 820.0 ? 20.0 : 44.0;
            if (itemSpec)
                desire += itemSpec->score * 0.018;
        }
        else if (itemSpec && bestSpec && itemSpec->score > bestSpec->score + 60.0)
        {
            desire += 14.0;
        }
        else
            desire += 4.0;
    }

    if (strstr(name, "shield") || strstr(name, "armor"))
    {
        shields = sithInventory_GetBinAmount(thing, SITHBIN_SHIELDS);
        desire += shields < 75.0 ? 30.0 : (shields < 150.0 ? 14.0 : 3.0);
    }
    else if (strstr(name, "health") || strstr(name, "bacta"))
    {
        flex_t healthFraction = thing->actorParams.maxHealth > 0.0
            ? thing->actorParams.health / thing->actorParams.maxHealth
            : 1.0;
        desire += healthFraction <= 0.40 ? 72.0 :
            (healthFraction < 0.60 ? 40.0 :
             (healthFraction < 0.95 ? 16.0 : 2.0));
    }
    else if (strstr(name, "railcharge") || strstr(name, "rail"))
    {
        if (sithBot_WeaponAvailable(thing, Main_bMotsCompat ? SITHBIN_MOTS_RAIL_DETONATOR : SITHBIN_RAIL_DETONATOR))
            desire += sithInventory_GetBinAmount(thing, SITHBIN_RAILCHARGES) < 8.0 ? 18.0 : 6.0;
    }
    else if (strstr(name, "powercell") || strstr(name, "energycell") || strstr(name, "cell"))
    {
        if (!bestSpec || bestSpec->score < 700.0)
            desire += 8.0;
        else if (strstr(name, "powercell"))
            desire += sithInventory_GetBinAmount(thing, SITHBIN_POWER) < 40.0 ? 16.0 : 5.0;
        else
            desire += sithInventory_GetBinAmount(thing, SITHBIN_ENERGY) < 80.0 ? 14.0 : 4.0;
    }
    else if (strstr(name, "powerboost"))
    {
        desire += sithInventory_GetBinAmount(thing, SITHBIN_POWERBOOST) < 1.0 ? 14.0 : 1.0;
    }
    else if (strstr(name, "force"))
    {
        desire += sithInventory_GetBinAmount(thing, SITHBIN_FORCEMANA) < 60.0 ? 12.0 : 2.0;
    }

    return desire;
}

static flex_t sithBot_GetBestOwnedRangedScore(sithThing *thing)
{
    const SithBotWeaponSpec *bestSpec = sithBot_GetBestOwnedRangedSpec(thing, 0);
    return bestSpec ? bestSpec->score : 0.0;
}

static int sithBot_ShouldSeekWeaponPickup(sithThing *thing, sithThing *enemy)
{
    flex_t bestScore;

    if (!thing)
        return 0;

    bestScore = sithBot_GetBestOwnedRangedScore(thing);
    if (bestScore >= 700.0)
        return 0;

    if (enemy)
    {
        flex_t enemyDist = rdVector_Dist3(&thing->position, &enemy->position);
        int canSee = sithCollision_HasLos(thing, enemy, 0) ||
                     sithBot_CanSeePosition(thing->sector, &thing->position, enemy->sector, &enemy->position);

        /* UT gives finding a weapon priority over charging with the default
           melee attack. Only commit to fists when the enemy is already close. */
        if (bestScore <= 0.0 && enemyDist > 0.90)
            return 1;
        if (enemyDist < 3.5 && canSee)
            return 0;
        if (bestScore >= 700.0 && enemyDist < 10.0 && canSee)
            return 0;
    }

    return 1;
}

static void sithBot_ClearArmRejects(SithBotState *state)
{
    int i;

    if (!state)
        return;

    for (i = 0; i < SITHBOT_ARM_REJECT_SLOTS; i++)
    {
        state->armRejectThingIdx[i] = -1;
        state->armRejectUntilMs[i] = 0;
    }
}

static int sithBot_IsArmThingRejected(SithBotState *state, int thingIdx)
{
    int i;

    if (!state)
        return 0;

    for (i = 0; i < SITHBOT_ARM_REJECT_SLOTS; i++)
    {
        if (state->armRejectThingIdx[i] == thingIdx && state->armRejectUntilMs[i] > sithTime_curMs)
            return 1;
    }
    return 0;
}

static void sithBot_RejectArmThing(SithBotState *state, int thingIdx)
{
    int i;
    int replaceIdx = 0;

    if (!state)
        return;

    for (i = 0; i < SITHBOT_ARM_REJECT_SLOTS; i++)
    {
        if (state->armRejectThingIdx[i] == thingIdx || state->armRejectUntilMs[i] <= sithTime_curMs)
        {
            replaceIdx = i;
            break;
        }
        if (state->armRejectUntilMs[i] < state->armRejectUntilMs[replaceIdx])
            replaceIdx = i;
    }

    state->armRejectThingIdx[replaceIdx] = thingIdx;
    state->armRejectUntilMs[replaceIdx] = sithTime_curMs + 45000;
}

static int sithBot_ChooseArmGoalNode(SithBotState *state, sithThing *thing)
{
    unsigned char reachableNodes[SITHBOT_MAX_NODES];
    int i;
    int startNode;
    int best = -1;
    flex_t bestScore = -3.4e38f;
    flex_t bestOwnedScore;

    if (!thing || sithBot_numNodes <= 0)
        return -1;

    startNode = sithBot_FindNearestNode(thing);
    sithBot_MarkReachableNodes(startNode, 0, reachableNodes);
    bestOwnedScore = sithBot_GetBestOwnedRangedScore(thing);

    for (i = 0; i < sithBot_numNodes; i++)
    {
        sithThing *item;
        const char *name;
        const SithBotWeaponSpec *itemSpec;
        int weaponBin;
        flex_t distSq;
        flex_t score;

        if (sithBot_nodes[i].kind != SITHBOT_NODE_ITEM)
            continue;
        if (!sithBot_nodes[i].sector || !sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
            continue;
        if (startNode >= 0 ? !reachableNodes[i] : !sithBot_IsNodeReachableFromThing(thing, startNode, i))
            continue;
        item = sithThing_GetThingByIdx(sithBot_nodes[i].thingIdx);
        if (!sithBot_IsItemAvailable(item))
            continue;
        if (sithBot_IsArmThingRejected(state, item->thingIdx))
            continue;
        name = sithBot_GetItemTemplateName(item);
        weaponBin = sithBot_GetWeaponBinForItemName(name);
        itemSpec = sithBot_GetWeaponSpec(weaponBin);
        if (!itemSpec)
            continue;
        if (sithBot_WeaponAvailable(thing, weaponBin) && itemSpec->score <= bestOwnedScore + 60.0)
            continue;

        distSq = sithBot_DistSq(&thing->position, &sithBot_nodes[i].pos);
        score = itemSpec->score * 0.07 - distSq * 0.045 + _frand() * 4.0;
        if (!sithBot_WeaponAvailable(thing, weaponBin))
            score += 48.0;
        else
            score += 12.0;
        if (sithBot_nodes[i].sector == thing->sector)
            score += 8.0;
        if (itemSpec->score >= 900.0)
            score += 10.0;

        if (score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }

    return best;
}

static int sithBot_ChooseTacticalPickupNode(sithThing *thing)
{
    unsigned char reachableNodes[SITHBOT_MAX_NODES];
    int startNode;
    int best = -1;
    flex_t bestScore = 12.0;
    int i;

    if (!thing || sithBot_numNodes <= 0)
        return -1;

    startNode = sithBot_FindNearestNode(thing);
    sithBot_MarkReachableNodes(startNode, 0, reachableNodes);
    for (i = 0; i < sithBot_numNodes; i++)
    {
        sithThing *item;
        flex_t distSq;
        flex_t score;

        if (sithBot_nodes[i].kind != SITHBOT_NODE_ITEM ||
            !sithBot_nodes[i].sector ||
            !sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
        {
            continue;
        }
        distSq = sithBot_DistSq(&thing->position, &sithBot_nodes[i].pos);
        if (distSq > 16.0)
            continue;
        item = sithThing_GetThingByIdx(sithBot_nodes[i].thingIdx);
        if (!sithBot_IsItemAvailable(item) ||
            (startNode >= 0 ? !reachableNodes[i] : !sithBot_IsNodeReachableFromThing(thing, startNode, i)) ||
            !sithBot_CanSeePosition(thing->sector, &thing->position, item->sector, &item->position))
        {
            continue;
        }

        score = sithBot_GetItemDesire(thing, item) - stdMath_Sqrt(distSq) * 5.0;
        if (score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

static flex_t sithBot_GetCombatHoldMin(const SithBotWeaponSpec *spec)
{
    flex_t holdMin;

    if (!spec)
        return 1.0;

    holdMin = spec->minDist;
    if (spec->selfSafeDist > 0.0 && holdMin < spec->selfSafeDist + 0.45)
        holdMin = spec->selfSafeDist + 0.45;
    if (holdMin < 1.6)
        holdMin = 1.6;
    return holdMin;
}

static int sithBot_IsItemAvailable(sithThing *item)
{
    return item
        && item->type == SITH_THING_ITEM
        && item->sector
        && !(item->thingflags & (SITH_TF_DISABLED | SITH_TF_WILLBEREMOVED))
        && item->itemParams.respawnTime <= sithTime_curMs;
}

static int sithBot_IsItemNodeAvailable(int nodeIdx)
{
    sithThing *item;

    if (nodeIdx < 0 || nodeIdx >= sithBot_numNodes)
        return 0;
    if (sithBot_nodes[nodeIdx].kind != SITHBOT_NODE_ITEM)
        return 1;

    item = sithThing_GetThingByIdx(sithBot_nodes[nodeIdx].thingIdx);
    return sithBot_IsItemAvailable(item);
}

static int sithBot_IsTrackedInventoryBin(int bin)
{
    sithItemDescriptor *desc;

    if (bin <= 0 || bin >= SITHBIN_NUMBINS)
        return 0;

    desc = sithInventory_GetBinByIdx(bin);
    if (!desc || !(desc->flags & ITEMINFO_VALID))
        return 0;

    if (desc->flags & (ITEMINFO_WEAPON | ITEMINFO_ITEM | ITEMINFO_POWER | ITEMINFO_MP_BACKPACK))
        return 1;

    return bin == SITHBIN_ENERGY
        || bin == SITHBIN_POWER
        || bin == SITHBIN_BATTERY
        || bin == SITHBIN_FORCEMANA
        || bin == SITHBIN_RAILCHARGES
        || bin == SITHBIN_CARBPELLETS
        || bin == SITHBIN_SEEKRAILS
        || bin == SITHBIN_EWEB_ROUNDS
        || bin == SITHBIN_SHIELDS
        || bin == SITHBIN_SUPERSHIELDS
        || bin == SITHBIN_POWERBOOST;
}

static void sithBot_SnapshotInventory(sithThing *thing, flex_t amounts[SITHBIN_NUMBINS], int available[SITHBIN_NUMBINS])
{
    int i;

    for (i = 0; i < SITHBIN_NUMBINS; i++)
    {
        amounts[i] = 0.0;
        available[i] = 0;
        if (sithBot_IsTrackedInventoryBin(i))
        {
            amounts[i] = sithInventory_GetBinAmount(thing, i);
            available[i] = sithInventory_GetAvailable(thing, i);
        }
    }
}

static int sithBot_LogInventoryDelta(SithBotState *state, sithThing *thing, sithThing *item, int weaponBefore, flex_t beforeAmounts[SITHBIN_NUMBINS], int beforeAvailable[SITHBIN_NUMBINS])
{
    int i;
    int changed = 0;
    int firstBin = -1;
    flex_t firstBefore = 0.0;
    flex_t firstAfter = 0.0;
    int weaponAfter;
    const char *itemName = "";

    if (!thing)
        return 0;

    for (i = 0; i < SITHBIN_NUMBINS; i++)
    {
        flex_t afterAmount;
        int afterAvailable;

        if (!sithBot_IsTrackedInventoryBin(i))
            continue;

        afterAmount = sithInventory_GetBinAmount(thing, i);
        afterAvailable = sithInventory_GetAvailable(thing, i);
        if (afterAvailable != beforeAvailable[i] || afterAmount > beforeAmounts[i] + 0.01)
        {
            if (firstBin < 0)
            {
                firstBin = i;
                firstBefore = beforeAmounts[i];
                firstAfter = afterAmount;
            }
            changed++;
        }
    }

    itemName = sithBot_GetItemTemplateName(item);

    if (!changed)
        return 0;

    weaponAfter = sithBot_ChooseWeapon(thing, 12.0);
    if (weaponAfter != weaponBefore && sithBot_WeaponAvailable(thing, weaponAfter))
    {
        sithInventory_SetCurWeapon(thing, weaponAfter);
        sithBot_ApplyWeaponPresentation(thing, weaponAfter);
    }

    if (sithBot_debugPickupsLogged < 80)
    {
        sithBot_Logf("BotMatch: pickup slot=%d itemThing=%d item='%s' changed=%d firstBin=%d before=%.2f after=%.2f weaponBefore=%d weaponAfter=%d\n",
                     state ? state->playerIdx : -1,
                     item ? (int)item->thingIdx : -1,
                     itemName,
                     changed,
                     firstBin,
                     firstBefore,
                     firstAfter,
                     weaponBefore,
                     weaponAfter);
        sithBot_debugPickupsLogged++;
    }

    return 1;
}

static int sithBot_AddInventoryAmount(sithThing *thing, int bin, flex_t amount)
{
    flex_t before;
    flex_t after;

    if (!thing || bin <= 0 || bin >= SITHBIN_NUMBINS || amount <= 0.0)
        return 0;

    before = sithInventory_GetBinAmount(thing, bin);
    sithInventory_SetCarries(thing, bin, 1);
    sithInventory_SetAvailable(thing, bin, 1);
    after = sithInventory_ChangeInv(thing, bin, amount);
    return after > before + 0.01;
}

static int sithBot_SetInventoryAtLeast(sithThing *thing, int bin, flex_t amount)
{
    flex_t before;
    flex_t after;

    if (!thing || bin <= 0 || bin >= SITHBIN_NUMBINS)
        return 0;

    before = sithInventory_GetBinAmount(thing, bin);
    sithInventory_SetCarries(thing, bin, 1);
    sithInventory_SetAvailable(thing, bin, 1);
    if (before >= amount)
        return 0;

    after = sithInventory_SetBinAmount(thing, bin, amount);
    return after > before + 0.01;
}

static int sithBot_ApplyKnownPickupFallback(SithBotState *state, sithThing *thing, sithThing *item)
{
    const char *name;
    int weaponBin;
    int changed = 0;

    if (!thing || !item)
        return 0;

    name = sithBot_GetItemTemplateName(item);
    weaponBin = sithBot_GetWeaponBinForItemName(name);

    if (weaponBin > 0)
    {
        changed |= sithBot_SetInventoryAtLeast(thing, weaponBin, 1.0);

        if (weaponBin == (Main_bMotsCompat ? SITHBIN_MOTS_CONCUSSION_RIFLE : SITHBIN_CONCUSSION_RIFLE))
            changed |= sithBot_AddInventoryAmount(thing, SITHBIN_POWER, 40.0);
        else if (weaponBin == (Main_bMotsCompat ? SITHBIN_MOTS_RAIL_DETONATOR : SITHBIN_RAIL_DETONATOR))
            changed |= sithBot_AddInventoryAmount(thing, SITHBIN_RAILCHARGES, 5.0);
        else if (weaponBin == (Main_bMotsCompat ? SITHBIN_MOTS_TUSKEN_PROD : SITHBIN_TUSKEN_PROD))
            changed |= sithBot_AddInventoryAmount(thing, SITHBIN_POWER, 16.0);
        else if (weaponBin == (Main_bMotsCompat ? SITHBIN_MOTS_REPEATER : SITHBIN_REPEATER))
            changed |= sithBot_AddInventoryAmount(thing, SITHBIN_POWER, 45.0);
        else if (weaponBin == (Main_bMotsCompat ? SITHBIN_MOTS_STORMTROOPER_RIFLE : SITHBIN_STORMTROOPER_RIFLE))
            changed |= sithBot_AddInventoryAmount(thing, SITHBIN_ENERGY, 60.0);
        else if (Main_bMotsCompat && weaponBin == SITHBIN_MOTS_STORMTROOPER_SCOPE)
            changed |= sithBot_AddInventoryAmount(thing, SITHBIN_ENERGY, 60.0);
        else if (Main_bMotsCompat && weaponBin == SITHBIN_MOTS_BLASTECH)
            changed |= sithBot_AddInventoryAmount(thing, SITHBIN_ENERGY, 30.0);
        else if (weaponBin == (Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL))
            changed |= sithBot_AddInventoryAmount(thing, SITHBIN_ENERGY, 30.0);
    }
    else if (strstr(name, "railcharge"))
    {
        changed |= sithBot_AddInventoryAmount(thing, SITHBIN_RAILCHARGES, 5.0);
    }
    else if (strstr(name, "powercell"))
    {
        changed |= sithBot_AddInventoryAmount(thing, SITHBIN_POWER, 25.0);
    }
    else if (strstr(name, "powerboost"))
    {
        changed |= sithBot_SetInventoryAtLeast(thing, SITHBIN_POWERBOOST, 1.0);
    }
    else if (strstr(name, "energycell") || strstr(name, "energy"))
    {
        changed |= sithBot_AddInventoryAmount(thing, SITHBIN_ENERGY, 50.0);
    }
    else if (strstr(name, "shield") || strstr(name, "armor"))
    {
        changed |= sithBot_AddInventoryAmount(thing, SITHBIN_SHIELDS, strstr(name, "full") ? 100.0 : 20.0);
    }
    else if (strstr(name, "health") || strstr(name, "bacta"))
    {
        flex_t before = thing->actorParams.health;
        flex_t amount = strstr(name, "bacta") ? 30.0 : 25.0;
        thing->actorParams.health += amount;
        if (thing->actorParams.health > thing->actorParams.maxHealth)
            thing->actorParams.health = thing->actorParams.maxHealth;
        changed |= thing->actorParams.health > before + 0.01;
    }

    if (changed && sithBot_debugPickupsLogged < 96)
    {
        sithBot_Logf("BotMatch: pickup-fallback slot=%d itemThing=%d item='%s' weaponBin=%d\n",
                     state ? state->playerIdx : -1,
                     item->thingIdx,
                     name,
                     weaponBin);
    }

    return changed;
}

static int sithBot_IsWithinPickupRange(sithThing *thing, sithThing *item)
{
    flex_t radius;

    if (!thing || !item)
        return 0;
    radius = SITHBOT_PICKUP_RADIUS + thing->moveSize + item->moveSize;
    return sithBot_DistSq(&thing->position, &item->position) <= radius * radius;
}

static int sithBot_TryPickupItem(SithBotState *state, sithThing *thing, sithThing *item, int targeted)
{
    flex_t beforeAmounts[SITHBIN_NUMBINS];
    int beforeAvailable[SITHBIN_NUMBINS];
    int weaponBefore;
    int inventoryChanged;
    int healthLogged = 0;
    flex_t healthBefore;

    if (!thing || !sithBot_IsItemAvailable(item))
        return 0;

    if (!sithBot_IsWithinPickupRange(thing, item))
        return 0;

    if (!sithCollision_HasLos(thing, item, 0))
    {
        if (!targeted ||
            (thing->sector != item->sector &&
             !sithBot_CanSeePosition(thing->sector, &thing->position, item->sector, &item->position)))
            return 0;
    }

    weaponBefore = sithInventory_GetCurWeapon(thing);
    healthBefore = thing->actorParams.health;
    sithBot_SnapshotInventory(thing, beforeAmounts, beforeAvailable);

    sithCog_SendMessageFromThing(item, thing, SITH_MESSAGE_TOUCHED);
    if (item->type == SITH_THING_ITEM)
        item->itemParams.respawnTime = sithTime_curMs + 500;

    inventoryChanged = sithBot_LogInventoryDelta(state, thing, item, weaponBefore, beforeAmounts, beforeAvailable);
    if (thing->actorParams.health > healthBefore + 0.01)
    {
        if (sithBot_debugPickupsLogged < 96)
        {
            sithBot_Logf("BotMatch: pickup-health slot=%d itemThing=%d item='%s' healthBefore=%.2f healthAfter=%.2f\n",
                         state ? state->playerIdx : -1,
                         item->thingIdx,
                         sithBot_GetItemTemplateName(item),
                         healthBefore,
                         thing->actorParams.health);
            sithBot_debugPickupsLogged++;
        }
        inventoryChanged = 1;
        healthLogged = 1;
    }
    if (!inventoryChanged)
    {
        int fallbackChanged = sithBot_ApplyKnownPickupFallback(state, thing, item);
        if (fallbackChanged)
        {
            inventoryChanged = sithBot_LogInventoryDelta(state, thing, item, weaponBefore, beforeAmounts, beforeAvailable);
            if (!inventoryChanged)
                inventoryChanged = 1;
        }
    }
    if (!healthLogged && thing->actorParams.health > healthBefore + 0.01 &&
        sithBot_debugPickupsLogged < 96)
    {
        sithBot_Logf("BotMatch: pickup-health slot=%d itemThing=%d item='%s' healthBefore=%.2f healthAfter=%.2f\n",
                     state ? state->playerIdx : -1,
                     item->thingIdx,
                     sithBot_GetItemTemplateName(item),
                     healthBefore,
                     thing->actorParams.health);
        sithBot_debugPickupsLogged++;
    }

    if (inventoryChanged && item->type == SITH_THING_ITEM &&
        !(item->thingflags & (SITH_TF_DISABLED | SITH_TF_WILLBEREMOVED)))
    {
        sithItem_Take(item, thing, 0);
    }

    return inventoryChanged;
}

static int sithBot_TryPickupNearby(SithBotState *state, sithThing *thing)
{
    int i;
    int pickedUp = 0;

    if (!thing || !sithWorld_pCurrentWorld)
        return 0;
    if (state && state->nextPickupMs > sithTime_curMs)
        return 0;
    if (state)
        state->nextPickupMs = sithTime_curMs + SITHBOT_PICKUP_CHECK_MS;

    for (i = 0; i < sithWorld_pCurrentWorld->numThingsLoaded; i++)
    {
        sithThing *item = &sithWorld_pCurrentWorld->things[i];
        if (sithBot_TryPickupItem(state, thing, item, 0))
            pickedUp = 1;
    }

    return pickedUp;
}

static void sithBot_DampHorizontalVelocity(SithBotState *state, sithThing *thing, flex_t rate)
{
    flex_t deltaSeconds;
    flex_t factor;

    if (!thing)
        return;
    deltaSeconds = state ? state->frameDeltaSeconds : 1.0 / 60.0;
    if (deltaSeconds <= 0.0 || deltaSeconds > 0.10)
        deltaSeconds = 1.0 / 60.0;
    factor = 1.0 - rate * deltaSeconds;
    if (factor < 0.20)
        factor = 0.20;
    if (factor > 0.995)
        factor = 0.995;
    thing->physicsParams.vel.x *= factor;
    thing->physicsParams.vel.y *= factor;
}

static void sithBot_DriveGroundVelocity(SithBotState *state, sithThing *thing, const rdVector3 *flatDir, flex_t dist, flex_t targetDz, int combat)
{
    rdVector3 horiz;
    rdVector3 safeDir;
    flex_t desiredSpeed;
    flex_t desiredX;
    flex_t desiredY;
    flex_t response;
    flex_t clampSpeed;
    flex_t beforeSpeed;
    flex_t afterSpeed;
    flex_t probeDist;
    flex_t stepHeight;
    int moveSafe;
    int steeringActive = 0;

    if (!thing || !flatDir)
        return;

    desiredSpeed = thing->physicsParams.maxVel;
    if (desiredSpeed < 2.4)
        desiredSpeed = 2.4;
    if (desiredSpeed > 4.2)
        desiredSpeed = 4.2;

    desiredSpeed *= combat ? 0.98 : 0.92;
    if (dist < 1.0)
        desiredSpeed *= (0.45 + dist * 0.55);
    if (dist < 0.55 && desiredSpeed > 0.90)
        desiredSpeed = 0.90;
    if (desiredSpeed < 0.25)
        desiredSpeed = 0.25;

    probeDist = dist;
    if (probeDist > 0.55)
        probeDist = 0.55;
    stepHeight = targetDz > 0.35 ? targetDz + 0.08 : 0.35;
    if (stepHeight > 0.85)
        stepHeight = 0.85;

    if (state && state->steeringUntilMs > sithTime_curMs && state->steeringCombat == combat &&
        (combat || state->steeringTargetNode < 0 || state->steeringTargetNode == state->nextNode))
    {
        rdVector_Copy3(&safeDir, &state->steeringDir);
        steeringActive = rdVector_Normalize3Acc(&safeDir) > 0.001 &&
            sithBot_IsMoveStepSafeWithRise(thing, &safeDir, probeDist, stepHeight);
    }

    if (!steeringActive)
    {
        rdVector_Copy3(&safeDir, flatDir);
        safeDir.z = 0.0;
        moveSafe = rdVector_Normalize3Acc(&safeDir) > 0.001 &&
            sithBot_IsMoveStepSafeWithRise(thing, &safeDir, probeDist, stepHeight);
        if (!moveSafe && state)
        {
            rdVector_Copy3(&safeDir, flatDir);
            safeDir.z = 0.0;
            moveSafe = rdVector_Normalize3Acc(&safeDir) > 0.001 &&
                sithBot_FindSafeMoveDir(thing, &safeDir, desiredSpeed, targetDz, &desiredSpeed);
            if (moveSafe)
            {
                rdVector_Copy3(&state->steeringDir, &safeDir);
                state->steeringUntilMs = sithTime_curMs + 650;
                state->steeringTargetNode = state->nextNode;
                state->steeringCombat = combat;
            }
        }
    }
    else
    {
        moveSafe = 1;
    }
    if (!moveSafe && state && !combat && dist <= 0.75 &&
        targetDz <= 0.45 && targetDz >= -0.85 &&
        state->nextNode >= 0 && state->nextNode < sithBot_numNodes)
    {
        int startNode = sithBot_FindNearestNode(thing);
        if (startNode == state->nextNode || sithBot_HasEdge(startNode, state->nextNode))
        {
            rdVector_Copy3(&safeDir, flatDir);
            safeDir.z = 0.0;
            if (rdVector_Normalize3Acc(&safeDir) > 0.001)
            {
                moveSafe = 1;
                steeringActive = 0;
                state->steeringUntilMs = 0;
                if (desiredSpeed > 0.90)
                    desiredSpeed = 0.90;
                sithBot_qualityRouteNudges++;
                if (sithBot_debugRouteNudgesLogged < 40)
                {
                    sithBot_Logf("BotMatch: route-nudge slot=%d start=%d next=%d dist=%.2f dz=%.2f speed=%.2f\n",
                                 state->playerIdx, startNode, state->nextNode, dist, targetDz, desiredSpeed);
                    sithBot_debugRouteNudgesLogged++;
                }
            }
        }
    }
    if (!moveSafe)
    {
        thing->physicsParams.acceleration.x = 0.0;
        thing->physicsParams.acceleration.y = 0.0;
        sithBot_DampHorizontalVelocity(state, thing, 7.0);
        if (state)
        {
            if (!state->blockedSinceMs)
                state->blockedSinceMs = sithTime_curMs;
            state->blockedMoveTicks++;
            if (sithTime_curMs - state->blockedSinceMs >= SITHBOT_STUCK_MS &&
                state->interactionRepeatUntilMs <= sithTime_curMs)
            {
                if (!combat && state->goalNode >= 0 && state->goalNode < sithBot_numNodes)
                {
                    int failedFrom = sithBot_FindNearestNode(thing);
                    int failedTo = failedFrom >= 0
                        ? sithBot_FindPathNext(failedFrom, state->goalNode)
                        : -1;
                    sithBot_BlockRouteEdge(failedFrom, failedTo);
                    sithBot_qualityRouteStalls++;
                    sithBot_Logf("BotMatch: route-probe-blocked slot=%d mode=%d from=%d to=%d goal=%d pos=(%.2f,%.2f,%.2f)\n",
                                 state->playerIdx,
                                 state->goalMode,
                                 failedFrom,
                                 failedTo,
                                 state->goalNode,
                                 thing->position.x,
                                 thing->position.y,
                                 thing->position.z);
                    if (state->goalMode == SITHBOT_GOAL_HUNT)
                    {
                        state->enemyIdx = -1;
                        state->lastSeenEnemyIdx = -1;
                        state->lastEnemySeenMs = 0;
                        state->lastEnemySeenSector = 0;
                        state->combatTargetIdx = -1;
                    }
                    state->goalNode = -1;
                    state->nextGoalMs = 0;
                    state->routeWatchGoal = -1;
                    state->routeWatchStartMs = 0;
                }
                state->nextNode = -1;
                state->routeGoalNode = -1;
                state->routeCommitUntilMs = 0;
                state->steeringUntilMs = 0;
                state->blockedMoveTicks = 0;
                state->blockedSinceMs = 0;
            }
        }
        if (state && state->nextMoveLogMs <= sithTime_curMs && sithBot_debugLedgeAvoidLogged < 40)
        {
            sithBot_Logf("BotMatch: ledge-stop slot=%d combat=%d dist=%.2f goal=%d next=%d sectorFlags=%X attachFlags=0x%X standableSupport=%d pos=(%.2f,%.2f,%.2f)\n",
                         state->playerIdx,
                         combat,
                         dist,
                         state->goalNode,
                         state->nextNode,
                         thing->sector ? (unsigned int)thing->sector->flags : 0,
                         (unsigned int)thing->attach_flags,
                         sithBot_IsSupportedByStandableThing(thing),
                         thing->position.x,
                         thing->position.y,
                         thing->position.z);
            sithBot_debugLedgeAvoidLogged++;
            state->nextMoveLogMs = sithTime_curMs + 700;
        }
        return;
    }

    if (state)
    {
        state->blockedMoveTicks = 0;
        state->blockedSinceMs = 0;
        if (!steeringActive && rdVector_Dot3(&safeDir, flatDir) > 0.985)
            state->steeringUntilMs = 0;
    }

    desiredX = safeDir.x * desiredSpeed;
    desiredY = safeDir.y * desiredSpeed;
    response = (state ? state->frameDeltaSeconds : 1.0 / 60.0) * (combat ? 5.5 : 4.5);
    if (response < 0.01)
        response = 0.01;
    if (response > 0.20)
        response = 0.20;

    horiz.x = thing->physicsParams.vel.x;
    horiz.y = thing->physicsParams.vel.y;
    horiz.z = 0.0;
    beforeSpeed = rdVector_Normalize3Acc(&horiz);

    thing->physicsParams.vel.x += (desiredX - thing->physicsParams.vel.x) * response;
    thing->physicsParams.vel.y += (desiredY - thing->physicsParams.vel.y) * response;

    horiz.x = thing->physicsParams.vel.x;
    horiz.y = thing->physicsParams.vel.y;
    horiz.z = 0.0;
    afterSpeed = rdVector_Normalize3Acc(&horiz);
    clampSpeed = desiredSpeed * 1.25;
    if (afterSpeed > clampSpeed && afterSpeed > 0.001)
    {
        thing->physicsParams.vel.x = horiz.x * clampSpeed;
        thing->physicsParams.vel.y = horiz.y * clampSpeed;
        afterSpeed = clampSpeed;
    }

    if (state && state->nextMoveLogMs <= sithTime_curMs && sithBot_debugMovesLogged < 48)
    {
        sithBot_Logf("BotMatch: move slot=%d combat=%d dist=%.2f desired=%.2f beforeSpeed=%.2f afterSpeed=%.2f maxVel=%.2f\n",
                     state->playerIdx,
                     combat,
                     dist,
                     desiredSpeed,
                     beforeSpeed,
                     afterSpeed,
                     thing->physicsParams.maxVel);
        sithBot_debugMovesLogged++;
        state->nextMoveLogMs = sithTime_curMs + 1800;
    }
}

static void sithBot_InitPlayerRenderInfo(int playerIdx, sithThing *thing)
{
    jkPlayerInfo *info;
    sithThing *saberSparks;
    sithThing *bloodSparks;
    sithThing *wallSparks;

    if (playerIdx < 0 || playerIdx >= JKPLAYER_NUM_INFOS || !thing)
        return;

    info = &playerThings[playerIdx];
    info->actorThing = thing;
    thing->playerInfo = info;
    thing->thingflags |= SITH_TF_RENDERWEAPON;
    info->maxTwinkles = 8;
    info->twinkleSpawnRate = 16;
    info->bHasSuperWeapon = 0;
    info->bHasSuperShields = 0;
    info->bHasForceSurge = 0;
#ifdef JKM_DSS
    info->thing_id = thing->thing_id;
    info->jkmUnk4 = 0;
    info->jkmUnk5 = 0;
    info->jkmUnk6 = 0.0;
    info->personality = 0;
#endif

    if (!info->polylineThing.polyline)
    {
        saberSparks = sithTemplate_GetEntryByName("+ssparks_saber");
        bloodSparks = sithTemplate_GetEntryByName("+ssparks_blood");
        wallSparks = sithTemplate_GetEntryByName("+ssparks_wall");
        jkSaber_InitializeSaberInfo(thing, "sabergreen1.mat", "sabergreen0.mat", 0.0032, 0.0018, 0.12, wallSparks, bloodSparks, saberSparks);
    }
}

static void sithBot_ApplyCharacterModel(int playerIdx, sithThing *thing)
{
    static const char *models[] = {
        "ky.3do",
        "kyh4.3do",
        "kyb3.3do",
        "kym13.3do",
        "kyt0.3do",
        "kyu0.3do",
        "kyv0.3do",
        "kyx0.3do"
    };
    const char *modelName;
    rdModel3 *model;
    sithSoundClass *soundclass;
    int modelIdx;
    int changed;

    if (!thing)
        return;

    modelIdx = playerIdx > 0 ? (playerIdx - 1) : 0;
    modelName = models[modelIdx % (int)(sizeof(models) / sizeof(models[0]))];
    model = sithModel_LoadEntry(modelName, 1);
    if (!model && thing->templateBase)
        model = thing->templateBase->rdthing.model3;
    if (!model)
        return;

    changed = sithThing_SetNewModel(thing, model);
    soundclass = sithSoundClass_LoadFile("ky.snd");
    if (soundclass)
        sithSoundClass_SetThingSoundClass(thing, soundclass);

    if (changed && sithComm_multiplayerFlags)
        sithDSSThing_SendSetThingModel(thing, -1);
    if (changed)
    {
        sithBot_Logf("BotMatch: model slot=%d model='%s' rdtype=%d puppet=%p\n",
                     playerIdx,
                     model->filename,
                     thing->rdthing.type,
                     (void *)thing->rdthing.puppet);
    }
}

static void sithBot_GiveBin(sithThing *thing, int bin, flex_t amount)
{
    sithInventory_SetCarries(thing, bin, 1);
    sithInventory_SetAvailable(thing, bin, 1);
    sithInventory_SetBinAmount(thing, bin, amount);
}

static void sithBot_GiveLoadout(sithThing *thing)
{
    int playerIdx = sithBot_GetPlayerSlotForThing(thing);
    int darkSide = playerIdx > 0 && (playerIdx & 1);

    sithInventory_ClearInventory(thing);

    if (Main_bMotsCompat)
    {
        sithBot_GiveBin(thing, SITHBIN_MOTS_FISTS, 1.0);
        sithBot_GiveBin(thing, SITHBIN_MOTS_BRYARPISTOL, 1.0);
        sithBot_GiveBin(thing, SITHBIN_ENERGY, 50.0);
    }
    else
    {
        sithBot_GiveBin(thing, SITHBIN_FISTS, 1.0);
        sithBot_GiveBin(thing, SITHBIN_BRYARPISTOL, 1.0);
        sithBot_GiveBin(thing, SITHBIN_ENERGY, 50.0);
    }

    sithBot_GiveBin(thing, SITHBIN_SHIELDS, 100.0);
    sithBot_GiveBin(thing, SITHBIN_FORCEMANA, SITHBOT_FORCE_MANA_MAX);
    if (darkSide)
    {
        sithBot_GiveBin(thing, Main_bMotsCompat ? SITHBIN_F_PUSH : SITHBIN_F_LIGHTNING, 2.0);
    }
    else
    {
        sithBot_GiveBin(thing, SITHBIN_F_HEALING, 2.0);
    }
    if (!thing->actorParams.templateWeapon)
        thing->actorParams.templateWeapon = sithTemplate_GetEntryByName("+bryarbolt");
    sithInventory_SetCurWeapon(thing, Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL);
    sithInventory_SetCurItem(thing, 0);
    sithInventory_SetCurPower(thing, 0);
}

static const char *sithBot_GetWeaponMeshName(int weaponBin)
{
    switch (weaponBin)
    {
        case SITHBIN_FISTS:
        case SITHBIN_MOTS_FISTS:
            return "fistg.3do";
        case SITHBIN_BRYARPISTOL:
        case SITHBIN_MOTS_BRYARPISTOL:
            return "bryg.3do";
        case SITHBIN_STORMTROOPER_RIFLE:
        case SITHBIN_MOTS_STORMTROOPER_RIFLE:
            return "strg.3do";
        case SITHBIN_TUSKEN_PROD:
        case SITHBIN_MOTS_TUSKEN_PROD:
            return "BowG.3do";
        case SITHBIN_REPEATER:
        case SITHBIN_MOTS_REPEATER:
            return "rptg.3do";
        case SITHBIN_RAIL_DETONATOR:
        case SITHBIN_MOTS_RAIL_DETONATOR:
            return "rldg.3do";
        case SITHBIN_CONCUSSION_RIFLE:
        case SITHBIN_MOTS_CONCUSSION_RIFLE:
            return "cong.3do";
        case SITHBIN_MOTS_BLASTECH:
            return "blsg.3do";
        case SITHBIN_MOTS_STORMTROOPER_SCOPE:
            return "sscg.3do";
        default:
            return 0;
    }
}

static void sithBot_ApplyWeaponPresentation(sithThing *thing, int weaponBin)
{
    const char *modelName;
    rdModel3 *model;
    jkPlayerInfo *renderInfo;
    int changed = 0;

    if (!thing || !thing->playerInfo)
        return;

    modelName = sithBot_GetWeaponMeshName(weaponBin);
    if (!modelName)
        return;
    model = sithModel_LoadEntry(modelName, 1);
    if (!model)
        return;

    renderInfo = thing->playerInfo;
    if (renderInfo->rd_thing.type != RD_THINGTYPE_MODEL || renderInfo->rd_thing.model3 != model)
    {
        rdThing_FreeEntry(&renderInfo->rd_thing);
        rdThing_NewEntry(&renderInfo->rd_thing, thing);
        rdThing_SetModel3(&renderInfo->rd_thing, model);
        changed = 1;
    }
    sithPuppet_SetArmedMode(thing, weaponBin == SITHBIN_FISTS || weaponBin == SITHBIN_MOTS_FISTS ? 0 : 1);

    if (changed && sithComm_multiplayerFlags)
        jkDSS_SendJKSetWeaponMesh(thing);
}

static void sithBot_ApplyMovementTuning(sithThing *thing)
{
    if (!thing)
        return;

    if (thing->physicsParams.maxVel < SITHBOT_MIN_MAXVEL)
        thing->physicsParams.maxVel = SITHBOT_MIN_MAXVEL;
    if (thing->actorParams.maxThrust < SITHBOT_MIN_MAXTHRUST)
        thing->actorParams.maxThrust = SITHBOT_MIN_MAXTHRUST;
}

static void sithBot_Respawn(int botIdx)
{
    int spawnIdx;
    int preferredSpawnIdx;
    int spawnNode;
    int standTrack;
    int bryarBin;
    sithThing *thing;
    sithThing *tmpl;

    if (botIdx < 0 || botIdx >= jkPlayer_maxPlayers)
        return;

    thing = jkPlayer_playerInfos[botIdx].playerThing;
    if (!thing || !jkPlayer_playerInfos[botIdx].pSpawnSector)
        return;

    tmpl = thing->templateBase ? thing->templateBase : thing;
    if (thing->rdthing.puppet && thing->puppet)
    {
        if (thing->puppet->field_18 >= 0)
            sithPuppet_StopKey(thing->rdthing.puppet, thing->puppet->field_18, 0.0);
        sithPuppet_ResetTrack(thing);
    }
    thing->actorParams.msUnderwater = 0;
    thing->actorParams.health = tmpl->actorParams.health;
    if (thing->actorParams.health <= 0.0)
        thing->actorParams.health = 100.0;

    thing->type = SITH_THING_PLAYER;
    thing->thingflags &= ~(SITH_TF_10 | SITH_TF_DISABLED | SITH_TF_INVULN | SITH_TF_DEAD | SITH_TF_WILLBEREMOVED);
    thing->actorParams.typeflags &= ~(SITH_AF_FALLING_TO_DEATH | SITH_AF_INVULNERABLE | SITH_AF_DISABLED);
    thing->physicsParams.physflags &= ~(SITH_PF_CROUCHING | SITH_PF_800 | SITH_PF_100);
    thing->physicsParams.physflags |= SITH_PF_SURFACEALIGN | SITH_PF_USEGRAVITY;
    if (tmpl->physicsParams.physflags & SITH_PF_800)
    {
        thing->physicsParams.physflags &= ~(SITH_PF_100 | SITH_PF_SURFACEALIGN);
        thing->physicsParams.physflags |= SITH_PF_800;
    }
    thing->lifeLeftMs = 0;
    sithBot_InitPlayerRenderInfo(botIdx, thing);
    sithBot_ApplyCharacterModel(botIdx, thing);
    sithActor_MoveJointsForEyePYR(thing, &rdroid_zeroVector3);

    preferredSpawnIdx = sithMulti_GetSpawnIdx(thing);
    if (preferredSpawnIdx < 0 || preferredSpawnIdx >= jkPlayer_maxPlayers || !jkPlayer_playerInfos[preferredSpawnIdx].pSpawnSector)
        preferredSpawnIdx = botIdx;
    spawnIdx = sithBot_ChooseSpawnIdx(thing, preferredSpawnIdx);
    if (spawnIdx < 0 || spawnIdx >= jkPlayer_maxPlayers || !jkPlayer_playerInfos[spawnIdx].pSpawnSector)
        spawnIdx = botIdx;
    spawnNode = sithBot_FindNavigableNodeAt(jkPlayer_playerInfos[spawnIdx].pSpawnSector,
                                            &jkPlayer_playerInfos[spawnIdx].spawnPosOrient.scale);
    sithBot_Logf("BotMatch: spawn-choice slot=%d preferred=%d preferredSector=%d chosen=%d chosenSector=%d node=%d edges=%d\n",
                 botIdx,
                 preferredSpawnIdx,
                 sithBot_GetSectorIndex(jkPlayer_playerInfos[preferredSpawnIdx].pSpawnSector),
                 spawnIdx,
                 sithBot_GetSectorIndex(jkPlayer_playerInfos[spawnIdx].pSpawnSector),
                 spawnNode,
                 spawnNode >= 0 ? sithBot_nodes[spawnNode].edgeCount : 0);

    if (thing->attach_flags)
        sithThing_DetachThing(thing);
    sithThing_LeaveSector(thing);
    sithThing_SetPosAndRot(thing, &jkPlayer_playerInfos[spawnIdx].spawnPosOrient.scale, &jkPlayer_playerInfos[spawnIdx].spawnPosOrient);
    sithThing_EnterSector(thing, jkPlayer_playerInfos[spawnIdx].pSpawnSector, 1, 0);
    sithPhysics_ThingStop(thing);
    sithPhysics_FindFloor(thing, 1);
    sithBot_GiveLoadout(thing);
    sithBot_ApplyWeaponPresentation(thing, Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL);
    sithWeapon_SyncPuppet(thing);
    sithBot_ApplyMovementTuning(thing);
    if (thing->rdthing.puppet && thing->puppet)
    {
        sithPuppet_ResetTrack(thing);
        standTrack = sithPuppet_PlayMode(thing, SITH_ANIM_STAND, 0);
        if (sithComm_multiplayerFlags && standTrack >= 0)
            sithDSSThing_SendPlayKeyMode(thing, SITH_ANIM_STAND, thing->rdthing.puppet->tracks[standTrack].field_130, -1, 255);
    }
    sithCog_SendSimpleMessageToAll(SITH_MESSAGE_NEWPLAYER, SENDERTYPE_THING, thing->thingIdx, SENDERTYPE_THING, thing->thingIdx);
    if (sithComm_multiplayerFlags)
        sithDSSThing_SendSyncThing(thing, -1, 255);

    bryarBin = Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL;
    sithBot_Logf("BotMatch: spawn-loadout slot=%d weapon=%d available=%d carries=%d ammo=%.2f model='%s' weaponMesh='%s' armedMode=%d\n",
                 botIdx,
                 sithInventory_GetCurWeapon(thing),
                 sithInventory_GetAvailable(thing, bryarBin),
                 sithInventory_GetCarries(thing, bryarBin),
                 sithInventory_GetBinAmount(thing, SITHBIN_ENERGY),
                 thing->rdthing.model3 ? thing->rdthing.model3->filename : "none",
                 thing->playerInfo && thing->playerInfo->rd_thing.model3 ? thing->playerInfo->rd_thing.model3->filename : "none",
                 thing->puppet ? thing->puppet->field_0 : -1);
}

static void sithBot_ResetState(SithBotState *bot, int playerIdx)
{
    memset(bot, 0, sizeof(*bot));
    bot->active = 1;
    bot->playerIdx = playerIdx;
    bot->goalNode = -1;
    bot->nextNode = -1;
    bot->enemyIdx = -1;
    bot->lastSeenEnemyIdx = -1;
    bot->combatTargetIdx = -1;
    bot->steeringTargetNode = -1;
    bot->routeGoalNode = -1;
    bot->ridingLiftThingIdx = -1;
    bot->ridingLiftTargetNode = -1;
    bot->jumpPadLaunchNode = -1;
    bot->jumpPadTargetNode = -1;
    bot->routeWatchGoal = -1;
    bot->routeFailureGoal = -1;
    bot->routeFailureCount = 0;
    bot->routeHistoryGoal = -1;
    bot->routeLastNode = -1;
    bot->routePriorNode = -1;
    bot->routeFlipCount = 0;
    bot->lastInteractionSurfaceIdx = -1;
    bot->lastInteractionThingIdx = -1;
    bot->routeBestDist = 3.4e38f;
    sithBot_ClearArmRejects(bot);
}

static int sithBot_BotStateForPlayer(int playerIdx)
{
    int i;

    for (i = 0; i < SITHBOT_MAX_BOTS; i++)
    {
        if (sithBot_bots[i].active && sithBot_bots[i].playerIdx == playerIdx)
            return i;
    }
    return -1;
}

void sithBot_ClearActiveBotInvulnerability(void)
{
    int i;

    for (i = 0; i < SITHBOT_MAX_BOTS; i++)
    {
        int playerIdx;
        sithThing *thing;

        if (!sithBot_bots[i].active)
            continue;
        playerIdx = sithBot_bots[i].playerIdx;
        if (playerIdx < 0 || playerIdx >= jkPlayer_maxPlayers)
            continue;
        thing = jkPlayer_playerInfos[playerIdx].playerThing;
        if (thing)
            thing->thingflags &= ~SITH_TF_INVULN;
    }
}

static int sithBot_NewBotState(int playerIdx)
{
    int i;

    for (i = 0; i < SITHBOT_MAX_BOTS; i++)
    {
        if (!sithBot_bots[i].active)
        {
            sithBot_ResetState(&sithBot_bots[i], playerIdx);
            return i;
        }
    }
    return -1;
}

static void sithBot_ActivateSlot(int playerIdx, int botNumber)
{
    sithPlayerInfo *info;
    sithThing *thing;
    const wchar_t *botNames[] = {
        L"Bot 1",
        L"Bot 2",
        L"Bot 3",
        L"Bot 4",
        L"Bot 5",
        L"Bot 6",
        L"Bot 7",
        L"Bot 8"
    };

    if (playerIdx <= 0 || playerIdx >= jkPlayer_maxPlayers)
        return;
    info = &jkPlayer_playerInfos[playerIdx];
    thing = info->playerThing;
    if (!thing || !info->pSpawnSector)
        return;

    sithPlayer_sub_4C8910(playerIdx);
    sithPlayer_Startup(playerIdx);
    sithPlayer_sub_4C87C0(playerIdx, SITHBOT_NETID_BASE + playerIdx);

    info->lastUpdateMs = sithTime_curMs;
    info->numKills = 0;
    info->numKilled = 0;
    info->numSuicides = 0;
    info->score = 0;
    info->respawnMask = 0;
    info->teamNum = (sithNet_MultiModeFlags & MULTIMODEFLAG_TEAMS) ? ((botNumber & 1) + 1) : 0;

    _wcsncpy(info->player_name, botNames[botNumber % (int)(sizeof(botNames) / sizeof(botNames[0]))], 31);
    info->player_name[31] = 0;
    _wcsncpy(info->multi_name, info->player_name, 31);
    info->multi_name[31] = 0;

    thing->type = SITH_THING_PLAYER;
    thing->actorParams.playerinfo = info;
    sithBot_InitPlayerRenderInfo(playerIdx, thing);
    thing->thingflags &= ~(SITH_TF_10 | SITH_TF_DISABLED | SITH_TF_INVULN | SITH_TF_DEAD | SITH_TF_WILLBEREMOVED);
    thing->actorParams.typeflags &= ~(SITH_AF_INVULNERABLE | SITH_AF_DISABLED);

    sithBot_Respawn(playerIdx);
    sithCog_SendSimpleMessageToAll(SITH_MESSAGE_JOIN, SENDERTYPE_THING, thing->thingIdx, 0, playerIdx);
    if (sithComm_multiplayerFlags)
        sithDSSThing_SendSyncThing(thing, -1, 255);
    sithMulti_SyncScores();
    sithBot_NewBotState(playerIdx);

    sithBot_Logf("BotMatch: joined slot=%d net=%u name='%S'\n", playerIdx, info->net_id, info->player_name);
}

static void sithBot_EnsureBots(void)
{
    int desired;
    int have = 0;
    int created = 0;
    int reservedLocalPlayers = 1;
    int i;

    desired = Main_numBots;
    if (desired < 0)
        desired = 0;
    if (desired > SITHBOT_MAX_BOTS)
        desired = SITHBOT_MAX_BOTS;
#ifdef TARGET_XBOX
    if (xboxSplitScreen_IsRequested())
        reservedLocalPlayers = xboxSplitScreen_GetRequestedLocalPlayerCount();
#endif
    if (reservedLocalPlayers < 1)
        reservedLocalPlayers = 1;
    if (reservedLocalPlayers > jkPlayer_maxPlayers)
        reservedLocalPlayers = jkPlayer_maxPlayers;
    if (desired > jkPlayer_maxPlayers - reservedLocalPlayers)
        desired = jkPlayer_maxPlayers - reservedLocalPlayers;

    for (i = 1; i < jkPlayer_maxPlayers; i++)
    {
        if (sithBot_IsBotNetId(jkPlayer_playerInfos[i].net_id))
        {
            have++;
            if (sithBot_BotStateForPlayer(i) < 0)
                sithBot_NewBotState(i);
        }
    }

    for (i = 1; i < jkPlayer_maxPlayers && have < desired; i++)
    {
#ifdef TARGET_XBOX
        if (xboxSplitScreen_IsRequested() &&
            i >= xboxSplitScreen_GetFirstPlayerIndex() &&
            i < xboxSplitScreen_GetFirstPlayerIndex() + reservedLocalPlayers)
        {
            continue;
        }
#endif
        if (!jkPlayer_playerInfos[i].playerThing || !jkPlayer_playerInfos[i].pSpawnSector)
            continue;
        if (jkPlayer_playerInfos[i].flags & 1)
            continue;
        sithBot_ActivateSlot(i, created);
        created++;
        have++;
    }

    if (!sithBot_spawnedForWorld)
    {
        sithBot_spawnedForWorld = 1;
        sithBot_matchStartMs = sithTime_curMs;
        sithBot_scoreLogged = 0;
        sithBot_Logf("BotMatch: start bots=%d activeBots=%d maxPlayers=%d timedSeconds=%d\n",
                     desired,
                     have,
                     jkPlayer_maxPlayers,
                     Main_botMatchSeconds);
    }
}

static void sithBot_UpdateCamera(void)
{
    sithThing *target;
    int targetIdx = Main_botCamPlayer;

    if (targetIdx <= 0 || targetIdx >= jkPlayer_maxPlayers)
        return;
    if (!sithBot_IsBotNetId(jkPlayer_playerInfos[targetIdx].net_id))
        return;

    target = jkPlayer_playerInfos[targetIdx].playerThing;
    if (!target || !target->sector)
        return;

    if (sithBot_cameraPlayer != targetIdx || sithWorld_pCurrentWorld->cameraFocus != target ||
        sithCamera_cameras[1].primaryFocus != target)
    {
        sithWorld_pCurrentWorld->cameraFocus = target;
        sithCamera_SetsFocus();
        sithBot_cameraPlayer = targetIdx;
        sithBot_Logf("BotMatch: bot-camera slot=%d mode=third-person\n", targetIdx);
    }

    if (sithCamera_currentCamera != &sithCamera_cameras[1])
        sithCamera_SetCurrentCamera(&sithCamera_cameras[1]);
}

static int sithBot_ChooseGoalNode(SithBotState *state, sithThing *thing)
{
    unsigned char reachableNodes[SITHBOT_MAX_NODES];
    int i;
    int startNode;
    int best = -1;
    flex_t bestScore = -3.4e38f;

    if (!thing || sithBot_numNodes <= 0)
        return -1;

    startNode = sithBot_FindNearestNode(thing);
    sithBot_MarkReachableNodes(startNode, 0, reachableNodes);

    for (i = 0; i < sithBot_numNodes; i++)
    {
        flex_t distSq = sithBot_DistSq(&thing->position, &sithBot_nodes[i].pos);
        flex_t score = _frand() * 4.0;
        if (!sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
            continue;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_LIFT)
            continue;
        if (startNode >= 0 ? !reachableNodes[i] : !sithBot_IsNodeReachableFromThing(thing, startNode, i))
            continue;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM)
        {
            sithThing *item = sithThing_GetThingByIdx(sithBot_nodes[i].thingIdx);
            if (item && sithBot_IsArmThingRejected(state, item->thingIdx))
                continue;
            if (sithBot_IsItemAvailable(item))
                score += sithBot_GetItemDesire(thing, item);
            else
                score -= 8.0;
        }
        else if (sithBot_nodes[i].kind == SITHBOT_NODE_SPAWN)
        {
            score += 4.0;
        }
        else if (sithBot_nodes[i].kind == SITHBOT_NODE_JUMPPAD)
        {
            score += 2.5;
        }
        score -= distSq * 0.015;
        if (score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    return best;
}

static void sithBot_ResetRouteProgressWatch(SithBotState *state)
{
    if (!state)
        return;

    state->routeWatchGoal = -1;
    state->routeWatchStartMs = 0;
    state->routeFailureGoal = -1;
    state->routeFailureCount = 0;
    state->routeHistoryGoal = -1;
    state->routeLastNode = -1;
    state->routePriorNode = -1;
    state->routeFlipCount = 0;
}

static int sithBot_CheckRouteGoalProgress(SithBotState *state, sithThing *thing)
{
    flex_t movedSq;
    int failedFrom;
    int failedTo;
    int alternateNext = -1;
    int edgeIdx;
    int routeCycling = 0;

    if (!state || !thing ||
        (state->goalMode != SITHBOT_GOAL_ROAM &&
         state->goalMode != SITHBOT_GOAL_HUNT &&
         state->goalMode != SITHBOT_GOAL_TACTICAL_ITEM) ||
        state->goalNode < 0 || state->goalNode >= sithBot_numNodes)
    {
        if (state)
        {
            state->routeWatchGoal = -1;
            state->routeWatchStartMs = 0;
            state->routeHistoryGoal = -1;
            state->routeLastNode = -1;
            state->routePriorNode = -1;
            state->routeFlipCount = 0;
        }
        return 0;
    }

    if (state->routeHistoryGoal != state->goalNode)
    {
        state->routeHistoryGoal = state->goalNode;
        state->routeLastNode = state->nextNode;
        state->routePriorNode = -1;
        state->routeFlipCount = 0;
    }
    else if (state->nextNode >= 0 && state->nextNode != state->routeLastNode)
    {
        if (state->nextNode == state->routePriorNode)
            state->routeFlipCount++;
        else
            state->routeFlipCount = 0;
        state->routePriorNode = state->routeLastNode;
        state->routeLastNode = state->nextNode;
        routeCycling = state->routeFlipCount >= 8;
    }

    if (state->routeWatchGoal != state->goalNode || !state->routeWatchStartMs)
    {
        state->routeWatchGoal = state->goalNode;
        state->routeWatchStartMs = sithTime_curMs;
        rdVector_Copy3(&state->routeWatchPos, &thing->position);
        return 0;
    }
    if (state->interactionWaitUntilMs > sithTime_curMs)
    {
        state->routeWatchStartMs = sithTime_curMs;
        rdVector_Copy3(&state->routeWatchPos, &thing->position);
        return 0;
    }
    if (!routeCycling && sithTime_curMs - state->routeWatchStartMs < SITHBOT_ROUTE_WATCH_MS)
        return 0;

    movedSq = sithBot_DistSq(&state->routeWatchPos, &thing->position);
    state->routeWatchStartMs = sithTime_curMs;
    rdVector_Copy3(&state->routeWatchPos, &thing->position);
    if (!routeCycling && (movedSq >= 0.09 ||
        sithBot_DistSq(&thing->position, &sithBot_nodes[state->goalNode].pos) < 0.80)
       )
    {
        return 0;
    }

    if (state->routeFailureGoal == state->goalNode)
        state->routeFailureCount++;
    else
    {
        state->routeFailureGoal = state->goalNode;
        state->routeFailureCount = 1;
    }
    sithBot_qualityRouteStalls++;
    failedFrom = sithBot_FindNearestNode(thing);
    failedTo = state->nextNode;
    if (failedFrom >= 0 && failedTo >= 0)
    {
        for (edgeIdx = 0; edgeIdx < sithBot_nodes[failedFrom].edgeCount; edgeIdx++)
        {
            if (sithBot_nodes[failedFrom].edges[edgeIdx] == failedTo)
                break;
        }
        if (edgeIdx >= sithBot_nodes[failedFrom].edgeCount)
            failedTo = sithBot_FindPathNext(failedFrom, state->goalNode);
        sithBot_BlockRouteEdge(failedFrom, failedTo);
        alternateNext = sithBot_FindPathNext(failedFrom, state->goalNode);
    }
    sithBot_Logf("BotMatch: route-stalled slot=%d mode=%d goal=%d next=%d moved=%.2f pos=(%.2f,%.2f,%.2f)\n",
                 state->playerIdx,
                 state->goalMode,
                 state->goalNode,
                 state->nextNode,
                 stdMath_Sqrt(movedSq),
                 thing->position.x,
                 thing->position.y,
                 thing->position.z);
    if (routeCycling)
    {
        sithBot_Logf("BotMatch: route-cycle slot=%d goal=%d prior=%d current=%d flips=%d\n",
                     state->playerIdx,
                     state->goalNode,
                     state->routePriorNode,
                     state->routeLastNode,
                     state->routeFlipCount);
    }
    if (state->routeFailureCount == 1 &&
        alternateNext >= 0 && alternateNext != failedTo)
    {
        sithBot_Logf("BotMatch: route-reroute slot=%d goal=%d blocked=%d->%d alternate=%d\n",
                     state->playerIdx,
                     state->goalNode,
                     failedFrom,
                     failedTo,
                     alternateNext);
        state->nextNode = -1;
        state->routeGoalNode = -1;
        state->routeCommitUntilMs = 0;
        state->routeWatchGoal = -1;
        state->routeWatchStartMs = 0;
        state->routeLastNode = -1;
        state->routePriorNode = -1;
        state->routeFlipCount = 0;
        return 1;
    }

    if (sithBot_nodes[state->goalNode].kind == SITHBOT_NODE_ITEM)
    {
        sithThing *item = sithThing_GetThingByIdx(sithBot_nodes[state->goalNode].thingIdx);
        if (item)
            sithBot_RejectArmThing(state, item->thingIdx);
    }
    if (state->goalMode == SITHBOT_GOAL_HUNT)
    {
        state->enemyIdx = -1;
        state->lastSeenEnemyIdx = -1;
        state->lastEnemySeenMs = 0;
        state->lastEnemySeenSector = 0;
        state->combatTargetIdx = -1;
    }
    state->goalNode = -1;
    state->nextNode = -1;
    state->nextGoalMs = 0;
    state->routeGoalNode = -1;
    state->routeCommitUntilMs = 0;
    state->routeWatchGoal = -1;
    state->routeWatchStartMs = 0;
    state->routeFailureGoal = -1;
    state->routeFailureCount = 0;
    state->routeHistoryGoal = -1;
    state->routeLastNode = -1;
    state->routePriorNode = -1;
    state->routeFlipCount = 0;
    return 1;
}

static int sithBot_ChooseHuntGoalNode(sithThing *thing, const rdVector3 *enemyPos, sithSector *enemySector)
{
    unsigned char reachableNodes[SITHBOT_MAX_NODES];
    int i;
    int startNode;
    int enemyNode;
    int searchAroundTarget = 0;
    int best = -1;
    flex_t bestScore = -3.4e38f;

    if (!thing || !enemyPos || !enemySector || sithBot_numNodes <= 0)
        return -1;

    startNode = sithBot_FindNearestNode(thing);
    sithBot_MarkReachableNodes(startNode, 0, reachableNodes);
    enemyNode = sithBot_FindNearestNodeAt(enemySector, enemyPos);
    if (startNode >= 0)
    {
        if (enemyNode >= 0 &&
            sithBot_nodes[enemyNode].kind != SITHBOT_NODE_PORTAL &&
            enemyNode != startNode &&
            sithBot_DistSq(&thing->position, &sithBot_nodes[enemyNode].pos) >= 0.64 &&
            reachableNodes[enemyNode])
        {
            return enemyNode;
        }
        searchAroundTarget = enemyNode == startNode ||
            (enemyNode >= 0 && sithBot_DistSq(&thing->position, &sithBot_nodes[enemyNode].pos) < 0.64);
    }

    for (i = 0; i < sithBot_numNodes; i++)
    {
        flex_t enemyDistSq;
        flex_t botDistSq;
        flex_t score;
        int reachable;

        if (!sithBot_nodes[i].sector || !sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
            continue;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_LIFT ||
            sithBot_nodes[i].kind == SITHBOT_NODE_PORTAL)
            continue;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM && !sithBot_IsItemNodeAvailable(i))
            continue;
        if (searchAroundTarget && i == startNode)
            continue;

        reachable = 0;
        if (startNode >= 0)
            reachable = reachableNodes[i] != 0;
        if (!reachable)
        {
            SithBotNode here;
            rdVector_Copy3(&here.pos, &thing->position);
            here.sector = thing->sector;
            here.kind = SITHBOT_NODE_FLOOR;
            here.thingIdx = -1;
            here.edgeCount = 0;
            reachable = sithBot_IsWalkableSegment(&here, &sithBot_nodes[i]);
        }
        if (!reachable)
            continue;

        enemyDistSq = sithBot_DistSq(enemyPos, &sithBot_nodes[i].pos);
        botDistSq = sithBot_DistSq(&thing->position, &sithBot_nodes[i].pos);
        if (searchAroundTarget && botDistSq < 0.81)
            continue;
        score = -enemyDistSq - botDistSq * 0.035 + _frand() * 3.0;

        if (sithBot_nodes[i].sector == enemySector)
            score += 16.0;
        if (sithBot_CanSeePosition(enemySector, enemyPos, sithBot_nodes[i].sector, &sithBot_nodes[i].pos))
            score += 8.0;
        if (searchAroundTarget)
        {
            /* At the last known position, sweep a nearby sightline instead of
               repeatedly selecting the node already under the bot. */
            if (enemyDistSq >= 0.64 && enemyDistSq <= 9.0)
                score += 14.0;
            else if (enemyDistSq > 16.0)
                score -= (enemyDistSq - 16.0) * 0.35;
        }
        if (sithBot_nodes[i].kind == SITHBOT_NODE_FLOOR)
            score += 4.0;
        else if (sithBot_nodes[i].kind == SITHBOT_NODE_SPAWN)
            score += 2.0;
        else if (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM)
            score -= 1.5;

        if (score > bestScore)
        {
            bestScore = score;
            best = i;
        }
    }

    if (best >= 0)
        return best;
    return enemyNode;
}

static int sithBot_ChooseEscapeNode(sithThing *thing, sithSector *hazardSector, const rdVector3 *hazardPos)
{
    SithBotNode here;
    unsigned char reachableNodes[SITHBOT_MAX_NODES];
    int startNode;
    int i;
    int pass;

    if (!thing || !hazardSector || sithBot_numNodes <= 0)
        return -1;

    rdVector_Copy3(&here.pos, &thing->position);
    here.sector = thing->sector;
    here.kind = SITHBOT_NODE_FLOOR;
    here.thingIdx = -1;
    here.edgeCount = 0;
    startNode = sithBot_FindNearestNode(thing);
    sithBot_MarkReachableNodes(startNode, hazardSector, reachableNodes);

    for (pass = 0; pass < 4; pass++)
    {
        int best = -1;
        flex_t bestScore = -3.4e38f;
        int requireDeepTarget = (pass & 1) == 0;
        int requireNonSpike = pass < 2;

        for (i = 0; i < sithBot_numNodes; i++)
        {
            flex_t distSq;
            flex_t hazardDistSq = 36.0;
            flex_t minHazardDistSq;
            flex_t score;
            int direct;
            int reachable;

            if (sithBot_nodes[i].sector == hazardSector)
                continue;
            if (!sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
                continue;
            if (sithBot_nodes[i].kind == SITHBOT_NODE_LIFT)
                continue;
            if (requireNonSpike && sithBot_IsCollisionSpikeSectorForBot(sithBot_nodes[i].sector))
                continue;
            if (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM && !sithBot_IsItemNodeAvailable(i))
                continue;

            distSq = sithBot_DistSq(&thing->position, &sithBot_nodes[i].pos);
            if (requireDeepTarget && distSq < 3.24)
                continue;
            if (hazardPos)
            {
                hazardDistSq = sithBot_DistSq(hazardPos, &sithBot_nodes[i].pos);
                minHazardDistSq = pass == 0 ? 16.0 : (pass == 1 ? 9.0 : (pass == 2 ? 4.0 : 0.0));
                if (hazardDistSq < minHazardDistSq)
                    continue;
            }

            direct = sithBot_IsWalkableSegment(&here, &sithBot_nodes[i]);
            reachable = direct || (startNode >= 0 && reachableNodes[i]);
            if (!reachable)
                continue;

            score = -distSq;
            if (hazardPos)
            {
                if (hazardDistSq > 36.0)
                    hazardDistSq = 36.0;
                score += hazardDistSq * 0.20;
            }
            if (direct)
                score += 18.0;
            if (sithBot_nodes[i].kind == SITHBOT_NODE_FLOOR)
                score += 4.0;
            else if (sithBot_nodes[i].kind == SITHBOT_NODE_SPAWN)
                score += 2.0;
            else if (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM)
                score += 1.0;

            if (score > bestScore)
            {
                bestScore = score;
                best = i;
            }
        }

        if (best >= 0)
            return best;
    }

    return -1;
}

static int sithBot_FindNearestSafeEmergencyNode(sithThing *thing, sithSector *hazardSector)
{
    int i;
    int best = -1;
    flex_t bestScore = 3.4e38f;

    if (!thing || sithBot_numNodes <= 0)
        return -1;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        flex_t distSq;
        flex_t score;

        if (!sithBot_nodes[i].sector || sithBot_nodes[i].sector == hazardSector)
            continue;
        if (!sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
            continue;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_LIFT)
            continue;
        if (sithBot_IsCollisionSpikeSectorForBot(sithBot_nodes[i].sector))
            continue;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM && !sithBot_IsItemNodeAvailable(i))
            continue;

        distSq = sithBot_DistSq(&thing->position, &sithBot_nodes[i].pos);
        score = distSq;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_FLOOR)
            score -= 1.0;
        else if (sithBot_nodes[i].kind == SITHBOT_NODE_SPAWN)
            score -= 0.5;

        if (score < bestScore)
        {
            bestScore = score;
            best = i;
        }
    }

    return best;
}

static int sithBot_EmergencyMoveOutOfHazard(int victimSlot, sithThing *thing)
{
    int stateIdx;
    int nodeIdx;
    SithBotState *state;
    SithBotNode *node;

    if (!thing || !thing->sector || sithBot_numNodes <= 0)
        return 0;
    if (!sithBot_IsRiskyNavSectorForBot(thing->sector) && !sithBot_IsCollisionSpikeSectorForBot(thing->sector))
        return 0;

    nodeIdx = sithBot_ChooseEscapeNode(thing, thing->sector, &thing->position);
    if (nodeIdx >= 0 && nodeIdx < sithBot_numNodes && sithBot_IsCollisionSpikeSectorForBot(sithBot_nodes[nodeIdx].sector))
        nodeIdx = sithBot_FindNearestSafeEmergencyNode(thing, thing->sector);
    if (nodeIdx < 0 || nodeIdx >= sithBot_numNodes)
        nodeIdx = sithBot_FindNearestSafeEmergencyNode(thing, thing->sector);
    if (nodeIdx < 0 || nodeIdx >= sithBot_numNodes)
        return 0;

    node = &sithBot_nodes[nodeIdx];
    if (!node->sector || node->sector == thing->sector || !sithBot_IsNavSectorUsableForBot(node->sector))
        return 0;

    if (thing->attach_flags)
        sithThing_DetachThing(thing);
    sithThing_LeaveSector(thing);
    sithThing_SetPosAndRot(thing, &node->pos, &thing->lookOrientation);
    sithThing_EnterSector(thing, node->sector, 1, 0);
    sithPhysics_ThingStop(thing);
    sithPhysics_FindFloor(thing, 1);

    stateIdx = sithBot_BotStateForPlayer(victimSlot);
    if (stateIdx >= 0)
    {
        state = &sithBot_bots[stateIdx];
        state->goalNode = -1;
        state->nextNode = -1;
        state->nextGoalMs = 0;
        state->goalMode = SITHBOT_GOAL_ROAM;
        state->stuckTicks = 0;
        state->blockedMoveTicks = 0;
        rdVector_Copy3(&state->lastMovePos, &thing->position);
        state->lastMoveCheckMs = sithTime_curMs;
    }

    if (sithComm_multiplayerFlags)
        sithDSSThing_SendSyncThing(thing, -1, 255);

    if (sithBot_debugHazardMovesLogged < 48)
    {
        sithBot_Logf("BotMatch: hazard-unstuck slot=%d node=%d sectorFlags=%X pos=(%.2f,%.2f,%.2f)\n",
                     victimSlot,
                     nodeIdx,
                     node->sector ? (unsigned int)node->sector->flags : 0,
                     thing->position.x,
                     thing->position.y,
                     thing->position.z);
        sithBot_debugHazardMovesLogged++;
    }

    return 1;
}

static void sithBot_FaceToward(SithBotState *state, sithThing *thing, const rdVector3 *target, int combat)
{
    rdVector3 fullDesired;
    rdVector3 desired;
    rdVector3 current;
    rdVector3 blended;
    rdVector3 eyePYR;
    flex_t deltaSeconds;
    flex_t turnScale;
    flex_t forwardDot;
    flex_t sideDot;
    flex_t targetPitch;
    flex_t pitchDelta;
    flex_t pitchStep;

    if (!state || !thing || !target)
        return;

    rdVector_Sub3(&fullDesired, target, &thing->position);
    if (rdVector_Normalize3Acc(&fullDesired) <= 0.001)
        return;

    deltaSeconds = state->frameDeltaSeconds;
    if (deltaSeconds <= 0.0 || deltaSeconds > 0.10)
        deltaSeconds = 1.0 / 60.0;

    if (thing->actorParams.typeflags & SITH_AF_CAN_ROTATE_HEAD)
    {
        targetPitch = stdMath_ArcSin3(fullDesired.z);
        targetPitch = stdMath_Clamp(targetPitch,
                                    thing->actorParams.minHeadPitch,
                                    thing->actorParams.maxHeadPitch);
        rdVector_Copy3(&eyePYR, &thing->actorParams.eyePYR);
        pitchDelta = targetPitch - eyePYR.x;
        pitchStep = deltaSeconds * (combat ? 110.0 : 80.0);
        if (pitchDelta > pitchStep)
            pitchDelta = pitchStep;
        else if (pitchDelta < -pitchStep)
            pitchDelta = -pitchStep;
        eyePYR.x += pitchDelta;
        eyePYR.y = 0.0;
        eyePYR.z = 0.0;
        sithActor_MoveJointsForEyePYR(thing, &eyePYR);
    }

    rdVector_Copy3(&desired, &fullDesired);
    desired.z = 0.0;
    if (rdVector_Normalize3Acc(&desired) <= 0.001)
        return;

    rdVector_Copy3(&current, &thing->lookOrientation.lvec);
    current.z = 0.0;
    if (rdVector_Normalize3Acc(&current) <= 0.001)
    {
        rdMatrix_BuildFromLook34(&thing->lookOrientation, &desired);
        rdVector_Zero3(&thing->lookOrientation.scale);
        return;
    }

    turnScale = deltaSeconds * (combat ? 7.0 : 5.0);
    if (turnScale < 0.008)
        turnScale = 0.008;
    if (turnScale > 0.18)
        turnScale = 0.18;

    forwardDot = rdVector_Dot3(&current, &desired);
    sideDot = rdVector_Dot3(&thing->lookOrientation.rvec, &desired);
    rdVector_Copy3(&blended, &current);

    /* JA eases view angles toward the ideal rather than snapping them. The
       side-step handles the exactly-behind case where a normalized blend has
       no preferred turn direction. */
    if (stdMath_Fabs(sideDot) < 0.02 && forwardDot < -0.75)
    {
        flex_t sign = (state->playerIdx & 1) ? 1.0 : -1.0;
        blended.x += thing->lookOrientation.rvec.x * turnScale * sign;
        blended.y += thing->lookOrientation.rvec.y * turnScale * sign;
    }
    else
    {
        blended.x += desired.x * turnScale;
        blended.y += desired.y * turnScale;
    }
    blended.z = 0.0;

    if (rdVector_Normalize3Acc(&blended) <= 0.001)
        return;
    rdMatrix_BuildFromLook34(&thing->lookOrientation, &blended);
    rdVector_Zero3(&thing->lookOrientation.scale);
}

static int sithBot_IsAimAligned(sithThing *thing, const rdVector3 *target, flex_t minDot)
{
    rdVector3 dir;
    rdMatrix34 aimOrientation;

    if (!thing || !target)
        return 0;
    rdVector_Sub3(&dir, target, &thing->position);
    if (rdVector_Normalize3Acc(&dir) <= 0.001)
        return 1;
    rdMatrix_Copy34(&aimOrientation, &thing->lookOrientation);
    rdMatrix_PreRotate34(&aimOrientation, &thing->actorParams.eyePYR);
    return rdVector_Dot3(&aimOrientation.lvec, &dir) >= minDot;
}

static int sithBot_TryActivateNearbyInteraction(SithBotState *state, sithThing *thing, const rdVector3 *target, int routeOnly)
{
    sithSurface *bestSurface = 0;
    sithThing *bestThing = 0;
    rdVector3 routeDir;
    flex_t bestDistSq = routeOnly ? 0.81 : 0.3025;
    flex_t bestDist = 0.0;
    int hasRouteDir = 0;
    int i;

    if (!state || !thing || !thing->sector || !sithWorld_pCurrentWorld)
        return 0;

    if (target)
    {
        rdVector_Sub3(&routeDir, target, &thing->position);
        routeDir.z = 0.0;
        hasRouteDir = rdVector_Normalize3Acc(&routeDir) > 0.001;
    }

    for (i = 0; i < sithWorld_pCurrentWorld->numSurfaces; i++)
    {
        sithSurface *surface = &sithWorld_pCurrentWorld->surfaces[i];
        rdFace *face = &surface->surfaceInfo.face;
        rdVector3 center;
        int validVertices = 0;
        int j;

        if (!(surface->surfaceFlags & SITH_SURFACE_COG_LINKED) || face->numVertices <= 0 || !face->vertexPosIdx)
            continue;
        if (routeOnly && stdMath_Fabs(face->normal.z) > 0.65)
            continue;

        rdVector_Zero3(&center);
        for (j = 0; j < (int)face->numVertices; j++)
        {
            int vertexIdx = face->vertexPosIdx[j];
            if (vertexIdx < 0 || vertexIdx >= sithWorld_pCurrentWorld->numVertices)
                continue;
            rdVector_Add3Acc(&center, &sithWorld_pCurrentWorld->vertices[vertexIdx]);
            validVertices++;
        }
        if (!validVertices)
            continue;

        rdVector_InvScale3Acc(&center, (flex_t)validVertices);
        {
            rdVector3 toCandidate;
            rdVector3 traceEnd;
            flex_t distSq = sithBot_DistSq(&thing->position, &center);
            if (distSq >= bestDistSq)
                continue;
            rdVector_Copy3(&traceEnd, &center);
            if (!sithCollision_GetSectorLookAt(thing->sector, &thing->position, &traceEnd, 0.03))
                continue;
            if (routeOnly && hasRouteDir)
            {
                rdVector_Sub3(&toCandidate, &center, &thing->position);
                toCandidate.z = 0.0;
                if (rdVector_Normalize3Acc(&toCandidate) > 0.001 && rdVector_Dot3(&routeDir, &toCandidate) < 0.20)
                    continue;
            }
            bestDistSq = distSq;
            bestSurface = surface;
            bestThing = 0;
        }
    }

    {
        int thingIdx;
        for (thingIdx = 0; thingIdx < sithWorld_pCurrentWorld->numThingsLoaded; thingIdx++)
        {
            sithThing *candidate = &sithWorld_pCurrentWorld->things[thingIdx];
            rdVector3 toCandidate;
            rdVector3 traceEnd;
            flex_t distSq;

            if (candidate == thing || (candidate->type != SITH_THING_COG && candidate->type != SITH_THING_GHOST) ||
                !(candidate->thingflags & SITH_TF_CAPTURED) ||
                (candidate->thingflags & (SITH_TF_DISABLED | SITH_TF_WILLBEREMOVED)))
                continue;

            distSq = sithBot_DistSq(&thing->position, &candidate->position);
            if (distSq >= bestDistSq)
                continue;
            rdVector_Copy3(&traceEnd, &candidate->position);
            if (!sithCollision_GetSectorLookAt(thing->sector, &thing->position, &traceEnd, 0.03))
                continue;
            if (routeOnly && hasRouteDir)
            {
                rdVector_Sub3(&toCandidate, &candidate->position, &thing->position);
                toCandidate.z = 0.0;
                if (rdVector_Normalize3Acc(&toCandidate) > 0.001 && rdVector_Dot3(&routeDir, &toCandidate) < 0.20)
                    continue;
            }
            bestDistSq = distSq;
            bestThing = candidate;
            bestSurface = 0;
        }
    }

    if (!bestSurface && !bestThing)
        return 0;

    if (state->interactionRepeatUntilMs > sithTime_curMs &&
        ((bestSurface && state->lastInteractionSurfaceIdx == (int)bestSurface->index) ||
         (bestThing && state->lastInteractionThingIdx == bestThing->thingIdx)))
    {
        return 0;
    }

    bestDist = stdMath_Sqrt(bestDistSq);
    if (bestSurface)
    {
        state->lastInteractionSurfaceIdx = (int)bestSurface->index;
        state->lastInteractionThingIdx = -1;
        sithCog_SendMessageFromSurface(bestSurface, thing, SITH_MESSAGE_ACTIVATE);
        if (sithBot_debugUsesLogged < 40)
        {
            sithBot_Logf("BotMatch: use-interaction slot=%d kind=surface index=%u dist=%.2f\n",
                         state->playerIdx, (unsigned int)bestSurface->index, bestDist);
            sithBot_debugUsesLogged++;
        }
    }
    else
    {
        state->lastInteractionSurfaceIdx = -1;
        state->lastInteractionThingIdx = bestThing->thingIdx;
        sithCog_SendMessageFromThing(bestThing, thing, SITH_MESSAGE_ACTIVATE);
        if (sithBot_debugUsesLogged < 40)
        {
            sithBot_Logf("BotMatch: use-interaction slot=%d kind=thing index=%d dist=%.2f\n",
                         state->playerIdx, bestThing->thingIdx, bestDist);
            sithBot_debugUsesLogged++;
        }
    }

    if (thing->rdthing.puppet && thing->puppet)
    {
        int track = sithPuppet_PlayMode(thing, SITH_ANIM_ACTIVATE, 0);
        if (sithComm_multiplayerFlags && track >= 0)
            sithDSSThing_SendPlayKeyMode(thing, SITH_ANIM_ACTIVATE, thing->rdthing.puppet->tracks[track].field_130, -1, 255);
    }
    state->interactionWaitUntilMs = sithTime_curMs + SITHBOT_INTERACTION_WAIT_MS;
    state->interactionRepeatUntilMs = sithTime_curMs + SITHBOT_INTERACTION_REPEAT_MS;

    return 1;
}

static int sithBot_CogControlsThing(sithCog *cog, sithThing *thing)
{
    int i;

    if (!cog || !thing)
        return 0;
    if (thing->capture_cog == cog || thing->class_cog == cog)
        return 1;

    for (i = 0; i < sithCog_numThingLinks; i++)
    {
        sithCogThingLink *link = &sithCog_aThingLinks[i];
        if (link->thing == thing && link->signature == thing->signature && link->cog == cog)
            return 1;
    }
    return 0;
}

static int sithBot_CogHandlesMessage(sithCog *cog, int message)
{
    int i;

    if (!cog || !cog->cogscript || message < 0 || message >= SITH_MESSAGE_MAX)
        return 0;
    for (i = 0; i < (int)cog->cogscript->num_triggers; i++)
    {
        if ((int)cog->cogscript->triggers[i].trigId == message)
            return 1;
    }
    return 0;
}

static int sithBot_GetSurfaceCenter(sithSurface *surface, rdVector3 *center)
{
    rdFace *face;
    int i;

    if (!surface || !center || !sithWorld_pCurrentWorld)
        return 0;
    face = &surface->surfaceInfo.face;
    if (face->numVertices <= 0 || !face->vertexPosIdx)
        return 0;

    rdVector_Zero3(center);
    for (i = 0; i < (int)face->numVertices; i++)
    {
        int vertexIdx = face->vertexPosIdx[i];
        if (vertexIdx < 0 || vertexIdx >= sithWorld_pCurrentWorld->numVertices)
            return 0;
        rdVector_Add3Acc(center, &sithWorld_pCurrentWorld->vertices[vertexIdx]);
    }
    rdVector_InvScale3Acc(center, (flex_t)face->numVertices);
    return 1;
}

static int sithBot_FindNearestFloorRouteNode(sithThing *thing)
{
    SithBotNode here;
    int best = -1;
    int i;
    flex_t bestDistSq = 3.4e38f;

    if (!thing || !thing->sector)
        return -1;

    rdVector_Copy3(&here.pos, &thing->position);
    here.sector = thing->sector;
    here.kind = SITHBOT_NODE_FLOOR;
    here.thingIdx = -1;
    here.pathFrame = -1;
    here.edgeCount = 0;
    for (i = 0; i < sithBot_numNodes; i++)
    {
        flex_t distSq;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM || sithBot_nodes[i].kind == SITHBOT_NODE_LIFT ||
            sithBot_nodes[i].edgeCount <= 0 || !sithBot_IsNavSectorUsableForBot(sithBot_nodes[i].sector))
        {
            continue;
        }
        distSq = sithBot_DistSq(&thing->position, &sithBot_nodes[i].pos);
        if (distSq >= bestDistSq || distSq > 4.0)
            continue;
        if (!sithBot_CanSeePosition(thing->sector, &thing->position,
                                    sithBot_nodes[i].sector, &sithBot_nodes[i].pos))
        {
            continue;
        }
        if (!sithBot_IsWalkableSegment(&here, &sithBot_nodes[i]))
            continue;
        best = i;
        bestDistSq = distSq;
    }
    return best;
}

static void sithBot_BuildFloorRoute(int startNode, int *steps, int *first)
{
    int queue[SITHBOT_MAX_NODES];
    int head = 0;
    int tail = 0;
    int i;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        steps[i] = -1;
        first[i] = -1;
    }
    if (startNode < 0 || startNode >= sithBot_numNodes)
        return;

    steps[startNode] = 0;
    first[startNode] = startNode;
    queue[tail++] = startNode;
    while (head < tail)
    {
        int nodeIdx = queue[head++];
        SithBotNode *node = &sithBot_nodes[nodeIdx];
        for (i = 0; i < node->edgeCount; i++)
        {
            int next = node->edges[i];
            if (next < 0 || next >= sithBot_numNodes || steps[next] >= 0 ||
                sithBot_nodes[next].kind == SITHBOT_NODE_ITEM ||
                sithBot_nodes[next].kind == SITHBOT_NODE_LIFT)
            {
                continue;
            }
            steps[next] = steps[nodeIdx] + 1;
            first[next] = nodeIdx == startNode ? next : first[nodeIdx];
            queue[tail++] = next;
        }
    }
}

static int sithBot_FindControlApproach(const rdVector3 *controlPos, sithSector *controlSector,
                                       const SithBotNode *liftNode, const int *steps, flex_t *outScore)
{
    int best = -1;
    int i;
    flex_t bestScore = 3.4e38f;

    if (!controlPos || !liftNode || !steps)
        return -1;
    for (i = 0; i < sithBot_numNodes; i++)
    {
        rdVector3 traceEnd;
        flex_t controlDistSq;
        flex_t floorError;
        flex_t score;

        if (steps[i] < 0 || sithBot_nodes[i].kind == SITHBOT_NODE_ITEM ||
            sithBot_nodes[i].kind == SITHBOT_NODE_LIFT)
        {
            continue;
        }
        floorError = sithBot_AbsFlex(sithBot_nodes[i].pos.z - liftNode->pos.z);
        if (floorError > 0.80 || sithBot_AbsFlex(controlPos->z - sithBot_nodes[i].pos.z) > 1.60)
            continue;
        controlDistSq = sithBot_DistSq(controlPos, &sithBot_nodes[i].pos);
        if (controlDistSq > 2.25)
            continue;
        rdVector_Copy3(&traceEnd, controlPos);
        if (!sithBot_CanSeePosition(sithBot_nodes[i].sector, &sithBot_nodes[i].pos,
                                    controlSector, &traceEnd))
        {
            continue;
        }
        score = (flex_t)steps[i] * 4.0 + controlDistSq * 2.0 + floorError * 4.0;
        if (score < bestScore)
        {
            bestScore = score;
            best = i;
        }
    }
    if (outScore)
        *outScore = bestScore;
    return best;
}

static int sithBot_MoveToLiftControl(SithBotState *state, sithThing *bot, sithThing *lift,
                                     const SithBotNode *liftNode)
{
    int steps[SITHBOT_MAX_NODES];
    int first[SITHBOT_MAX_NODES];
    int startNode;
    int bestApproach = -1;
    int bestNext = -1;
    sithSurface *bestSurface = 0;
    sithThing *bestThing = 0;
    rdVector3 bestPos;
    flex_t bestScore = 3.4e38f;
    flex_t dist;
    int i;

    if (!state || !bot || !lift || !liftNode)
        return 0;
    startNode = sithBot_FindNearestFloorRouteNode(bot);
    if (startNode < 0)
        return 0;
    sithBot_BuildFloorRoute(startNode, steps, first);

    for (i = 0; i < sithCog_numSurfaceLinks; i++)
    {
        sithCogSurfaceLink *link = &sithCog_aSurfaceLinks[i];
        sithSurface *surface = link->surface;
        rdVector3 center;
        flex_t score;
        int approach;

        if (!surface || surface->adjoin ||
            stdMath_Fabs(surface->surfaceInfo.face.normal.z) > 0.65 ||
            !sithBot_CogControlsThing(link->cog, lift) ||
            !sithBot_CogHandlesMessage(link->cog, SITH_MESSAGE_ACTIVATE) ||
            !sithBot_GetSurfaceCenter(surface, &center))
        {
            continue;
        }
        approach = sithBot_FindControlApproach(&center, surface->parent_sector, liftNode, steps, &score);
        if (approach >= 0 && score < bestScore)
        {
            bestScore = score;
            bestApproach = approach;
            bestSurface = surface;
            bestThing = 0;
            rdVector_Copy3(&bestPos, &center);
        }
    }

    for (i = 0; i < sithCog_numThingLinks; i++)
    {
        sithCogThingLink *link = &sithCog_aThingLinks[i];
        sithThing *control = link->thing;
        flex_t score;
        int approach;

        if (!control || control == lift || link->signature != control->signature ||
            !sithBot_CogControlsThing(link->cog, lift) ||
            !sithBot_CogHandlesMessage(link->cog, SITH_MESSAGE_ACTIVATE))
        {
            continue;
        }
        approach = sithBot_FindControlApproach(&control->position, control->sector, liftNode, steps, &score);
        if (approach >= 0 && score < bestScore)
        {
            bestScore = score;
            bestApproach = approach;
            bestSurface = 0;
            bestThing = control;
            rdVector_Copy3(&bestPos, &control->position);
        }
    }

    if (bestApproach < 0)
        return 0;
    bestNext = first[bestApproach];
    if (bestNext < 0)
        bestNext = bestApproach;

    dist = rdVector_Dist3(&bot->position, &bestPos);
    if (dist <= 1.15 &&
        sithBot_CanSeePosition(bot->sector, &bot->position,
                               bestSurface ? bestSurface->parent_sector : bestThing->sector, &bestPos))
    {
        if (state->nextUseMs <= sithTime_curMs)
        {
            if (bestSurface)
                sithCog_SendMessageFromSurface(bestSurface, bot, SITH_MESSAGE_ACTIVATE);
            else
                sithCog_SendMessageFromThing(bestThing, bot, SITH_MESSAGE_ACTIVATE);
            state->lastInteractionSurfaceIdx = bestSurface ? (int)bestSurface->index : -1;
            state->lastInteractionThingIdx = bestThing ? bestThing->thingIdx : -1;
            state->nextUseMs = sithTime_curMs + 1200;
            state->interactionWaitUntilMs = sithTime_curMs + 500;
            state->interactionRepeatUntilMs = sithTime_curMs + 1800;
            if (sithBot_debugUsesLogged < 40)
            {
                sithBot_Logf("BotMatch: lift-call slot=%d lift=%d kind=%s index=%d dist=%.2f approach=%d\n",
                             state->playerIdx,
                             lift->thingIdx,
                             bestSurface ? "surface" : "thing",
                             bestSurface ? (int)bestSurface->index : bestThing->thingIdx,
                             dist,
                             bestApproach);
                sithBot_debugUsesLogged++;
            }
        }
        rdVector_Zero3(&bot->physicsParams.acceleration);
        sithBot_DampHorizontalVelocity(state, bot, 10.0);
        sithBot_FaceToward(state, bot, &bestPos, 0);
        sithBot_SyncPositionIfNeeded(state, bot);
        return 1;
    }

    if (bestNext == startNode || bestApproach == startNode)
    {
        rdVector3 controlFloorPos;
        rdVector_Copy3(&controlFloorPos, &bestPos);
        controlFloorPos.z = bot->position.z;
        sithBot_FaceToward(state, bot, &bestPos, 0);
        sithBot_MoveToward(state, bot, &controlFloorPos, 0);
    }
    else
    {
        sithBot_FaceToward(state, bot, &sithBot_nodes[bestNext].pos, 0);
        sithBot_MoveToward(state, bot, &sithBot_nodes[bestNext].pos, 0);
    }
    if (state->nextLiftLogMs <= sithTime_curMs && sithBot_debugLiftsLogged < 48)
    {
        sithBot_Logf("BotMatch: lift-call-route slot=%d lift=%d approach=%d next=%d dist=%.2f\n",
                     state->playerIdx, lift->thingIdx, bestApproach, bestNext, dist);
        sithBot_debugLiftsLogged++;
        state->nextLiftLogMs = sithTime_curMs + 750;
    }
    sithBot_SyncPositionIfNeeded(state, bot);
    return 1;
}

static int sithBot_TryActivateLiftControl(SithBotState *state, sithThing *bot, sithThing *lift, int attached)
{
    sithSurface *bestSurface = 0;
    sithThing *bestThing = 0;
    flex_t bestDistSq = attached ? 2.25 : 1.44;
    flex_t bestDist;
    int i;

    if (!state || !bot || !lift || !sithWorld_pCurrentWorld)
        return 0;

    for (i = 0; i < sithCog_numSurfaceLinks; i++)
    {
        sithCogSurfaceLink *link = &sithCog_aSurfaceLinks[i];
        sithSurface *surface = link->surface;
        rdFace *face;
        rdVector3 center;
        flex_t distSq;
        int j;

        if (!surface || (!attached && surface->adjoin) ||
            !sithBot_CogControlsThing(link->cog, lift) ||
            !sithBot_CogHandlesMessage(link->cog, SITH_MESSAGE_ACTIVATE))
            continue;
        face = &surface->surfaceInfo.face;
        if (face->numVertices <= 0 || !face->vertexPosIdx)
            continue;
        rdVector_Zero3(&center);
        for (j = 0; j < (int)face->numVertices; j++)
        {
            int vertexIdx = face->vertexPosIdx[j];
            if (vertexIdx < 0 || vertexIdx >= sithWorld_pCurrentWorld->numVertices)
                break;
            rdVector_Add3Acc(&center, &sithWorld_pCurrentWorld->vertices[vertexIdx]);
        }
        if (j != (int)face->numVertices)
            continue;
        rdVector_InvScale3Acc(&center, (flex_t)face->numVertices);
        distSq = sithBot_DistSq(&bot->position, &center);
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            bestSurface = surface;
            bestThing = 0;
        }
    }

    for (i = 0; i < sithCog_numThingLinks; i++)
    {
        sithCogThingLink *controlLink = &sithCog_aThingLinks[i];
        sithThing *control = controlLink->thing;
        flex_t distSq;

        if (!control || control == lift || controlLink->signature != control->signature ||
            !sithBot_CogControlsThing(controlLink->cog, lift) ||
            !sithBot_CogHandlesMessage(controlLink->cog, SITH_MESSAGE_ACTIVATE))
        {
            continue;
        }
        distSq = sithBot_DistSq(&bot->position, &control->position);
        if (distSq < bestDistSq)
        {
            bestDistSq = distSq;
            bestThing = control;
            bestSurface = 0;
        }
    }

    if (!bestSurface && !bestThing)
        return 0;

    bestDist = stdMath_Sqrt(bestDistSq);
    if (bestSurface)
    {
        state->lastInteractionSurfaceIdx = (int)bestSurface->index;
        state->lastInteractionThingIdx = -1;
        sithCog_SendMessageFromSurface(bestSurface, bot, SITH_MESSAGE_ACTIVATE);
        if (sithBot_debugUsesLogged < 40)
        {
            sithBot_Logf("BotMatch: lift-control slot=%d lift=%d kind=surface index=%u dist=%.2f\n",
                         state->playerIdx, lift->thingIdx, (unsigned int)bestSurface->index, bestDist);
            sithBot_debugUsesLogged++;
        }
    }
    else
    {
        state->lastInteractionSurfaceIdx = -1;
        state->lastInteractionThingIdx = bestThing->thingIdx;
        sithCog_SendMessageFromThing(bestThing, bot, SITH_MESSAGE_ACTIVATE);
        if (sithBot_debugUsesLogged < 40)
        {
            sithBot_Logf("BotMatch: lift-control slot=%d lift=%d kind=thing index=%d dist=%.2f\n",
                         state->playerIdx, lift->thingIdx, bestThing->thingIdx, bestDist);
            sithBot_debugUsesLogged++;
        }
    }

    state->interactionWaitUntilMs = sithTime_curMs + 500;
    state->interactionRepeatUntilMs = sithTime_curMs + 1200;
    return 1;
}

static sithThing *sithBot_GetPathLiftForNode(int nodeIdx)
{
    SithBotNode *node;
    sithThing *lift;

    if (nodeIdx < 0 || nodeIdx >= sithBot_numNodes)
        return 0;
    node = &sithBot_nodes[nodeIdx];
    if (node->kind != SITHBOT_NODE_LIFT || node->thingIdx < 0 || node->pathFrame < 0)
        return 0;

    lift = sithThing_GetThingByIdx(node->thingIdx);
    if (!sithBot_IsPathLiftThing(lift) || node->pathFrame >= lift->trackParams.loadedFrames)
        return 0;
    return lift;
}

static int sithBot_FindNearbyPathLiftStop(sithThing *thing, sithThing *lift, int requestedNode)
{
    int bestNode = -1;
    flex_t bestScore = 3.4e38f;
    int i;

    if (!thing || !lift)
        return requestedNode;
    for (i = 0; i < sithBot_numNodes; i++)
    {
        SithBotNode *candidate = &sithBot_nodes[i];
        flex_t dx;
        flex_t dy;
        flex_t dz;
        flex_t score;

        if (candidate->kind != SITHBOT_NODE_LIFT || candidate->thingIdx != lift->thingIdx)
            continue;
        dx = thing->position.x - candidate->pos.x;
        dy = thing->position.y - candidate->pos.y;
        dz = sithBot_AbsFlex(thing->position.z - candidate->pos.z);
        if (dx * dx + dy * dy > 1.25 * 1.25 || dz > 0.50)
            continue;
        score = dx * dx + dy * dy + dz * dz * 5.0;
        if (score < bestScore)
        {
            bestScore = score;
            bestNode = i;
        }
    }
    return bestNode >= 0 ? bestNode : requestedNode;
}

static int sithBot_TryAttachToPathLift(sithThing *thing, sithThing *lift)
{
    sithCollisionSearchEntry *entry;
    rdVector3 down;
    rdVector3 probeStart;
    rdVector3 worldNormal;
    int attached = 0;

    if (!thing || !thing->sector || !sithBot_IsPathLiftThing(lift))
        return 0;
    if ((thing->attach_flags & (SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)) &&
        thing->attachedThing == lift)
    {
        return 1;
    }

    rdVector_Copy3(&probeStart, &thing->position);
    probeStart.z += 0.18;
    rdVector_Neg3(&down, &rdroid_zVector3);
    sithCollision_SearchRadiusForThings(thing->sector,
                                        0,
                                        &probeStart,
                                        &down,
                                        1.10,
                                        0.02,
                                        RAYCAST_10 | RAYCAST_2000 | RAYCAST_800 | RAYCAST_2);
    while ((entry = sithCollision_NextSearchResult()) != 0)
    {
        if (!(entry->hitType & SITHCOLLISION_THING) || entry->receiver != lift ||
            !entry->face || !entry->sender)
        {
            continue;
        }

        rdMatrix_TransformVector34(&worldNormal, &entry->face->normal, &lift->lookOrientation);
        if (rdVector_Dot3(&worldNormal, &rdroid_zVector3) < 0.60)
            continue;

        sithThing_LandThing(thing, lift, entry->face, entry->sender->vertices, 1);
        attached = (thing->attach_flags & (SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)) &&
            thing->attachedThing == lift;
        if (attached)
        {
            /* A path thing normally carries a standing player through collision
               updates. Bots deliberately stop while riding, which makes the
               floor reacquisition pass detach them on short MotS lifts. Latch
               the passenger to the car until the route reaches its exit stop. */
            thing->attach_flags |= SITH_ATTACH_NO_MOVE;
            rdVector_Zero3(&thing->physicsParams.acceleration);
            rdVector_Zero3(&thing->physicsParams.vel);
            if (sithComm_multiplayerFlags)
                sithDSSThing_SendSyncThingAttachment(thing, -1, 255, 1);
        }
        break;
    }
    sithCollision_SearchClose();
    return attached;
}

static int sithBot_HandlePathLiftRoute(SithBotState *state, sithThing *thing, int nextNode)
{
    int requestedNode = nextNode;
    SithBotNode *node;
    sithThing *lift;
    sithThingFrame *frame;
    rdVector3 boardDir;
    flex_t dx;
    flex_t dy;
    flex_t boardDist;
    flex_t boardSpeed;
    int attached;
    int atStop;
    int moving;

    if (!state || !thing)
        return 0;
    lift = sithBot_GetPathLiftForNode(nextNode);
    if (!lift)
        return 0;

    node = &sithBot_nodes[nextNode];
    frame = &lift->trackParams.aFrames[node->pathFrame];
    dx = lift->position.x - frame->pos.x;
    dy = lift->position.y - frame->pos.y;
    atStop = dx * dx + dy * dy +
        (lift->position.z - frame->pos.z) * (lift->position.z - frame->pos.z) < 0.24 * 0.24;
    moving = (lift->trackParams.flags & 3) != 0;
    attached = (thing->attach_flags & (SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)) &&
        thing->attachedThing == lift;

    if (!attached && state->ridingLiftThingIdx == lift->thingIdx)
    {
        flex_t relX = thing->position.x - lift->position.x;
        flex_t relY = thing->position.y - lift->position.y;
        flex_t relZ = sithBot_AbsFlex(thing->position.z - lift->position.z);

        if (relX * relX + relY * relY <= 0.95 * 0.95 && relZ <= 1.25)
        {
            /* Some MotS path things briefly lose a surface attachment while
               changing sectors. Preserve the passenger relationship until the
               intended stop instead of treating the moving car as absent. */
            sithThing_AttachThing(thing, lift);
            thing->attach_flags |= SITH_ATTACH_NO_MOVE;
            rdVector_Zero3(&thing->physicsParams.acceleration);
            rdVector_Zero3(&thing->physicsParams.vel);
            attached = 1;
            if (sithComm_multiplayerFlags)
                sithDSSThing_SendSyncThingAttachment(thing, -1, 255, 1);
        }
        else
        {
            state->ridingLiftThingIdx = -1;
            state->ridingLiftTargetNode = -1;
        }
    }

    if (!attached)
    {
        nextNode = sithBot_FindNearbyPathLiftStop(thing, lift, requestedNode);
        node = &sithBot_nodes[nextNode];
        frame = &lift->trackParams.aFrames[node->pathFrame];
        dx = lift->position.x - frame->pos.x;
        dy = lift->position.y - frame->pos.y;
        atStop = dx * dx + dy * dy +
            (lift->position.z - frame->pos.z) * (lift->position.z - frame->pos.z) < 0.24 * 0.24;
    }

    if (attached)
    {
        if (atStop && lift->curframe == node->pathFrame && state->goalNode >= 0 &&
            state->goalNode < sithBot_numNodes)
        {
            int rideNode = sithBot_FindPathNext(nextNode, state->goalNode);
            sithThing *rideLift = sithBot_GetPathLiftForNode(rideNode);
            if (rideLift == lift && sithBot_nodes[rideNode].pathFrame != lift->curframe)
            {
                state->nextNode = rideNode;
                state->ridingLiftTargetNode = rideNode;
                node = &sithBot_nodes[rideNode];
                frame = &lift->trackParams.aFrames[node->pathFrame];
                dx = lift->position.x - frame->pos.x;
                dy = lift->position.y - frame->pos.y;
                atStop = 0;
            }
            else
            {
                if (thing->attach_flags & SITH_ATTACH_NO_MOVE)
                {
                    sithThing_DetachThing(thing);
                    sithPhysics_ThingStop(thing);
                    sithPhysics_FindFloor(thing, 1);
                    if (sithComm_multiplayerFlags)
                        sithDSSThing_SendSyncThingAttachment(thing, -1, 255, 1);
                }
                /* Continue from the graph edge on this floor. Recomputing the
                   nearest node while the car still fills the shaft can select
                   the opposite landing and send the bot straight back in. */
                state->nextNode = rideNode;
                state->ridingLiftThingIdx = -1;
                state->ridingLiftTargetNode = -1;
                state->routeGoalNode = state->goalNode;
                state->routeBestDist = rideNode >= 0 && rideNode < sithBot_numNodes
                    ? rdVector_Dist3(&thing->position, &sithBot_nodes[rideNode].pos)
                    : 3.4e38f;
                state->routeCommitUntilMs = sithTime_curMs + SITHBOT_ROUTE_COMMIT_MS;
                if (state->nextLiftLogMs <= sithTime_curMs && sithBot_debugLiftsLogged < 48)
                {
                    sithBot_Logf("BotMatch: lift-exit slot=%d thing=%d frame=%d next=%d dist=%.2f\n",
                                 state->playerIdx,
                                 lift->thingIdx,
                                 node->pathFrame,
                                 rideNode,
                                 state->routeBestDist);
                    sithBot_debugLiftsLogged++;
                    state->nextLiftLogMs = sithTime_curMs + 500;
                }
                return 0;
            }
        }

        if (!moving && !atStop && lift->goalframe != node->pathFrame &&
            state->nextUseMs <= sithTime_curMs)
        {
            if (!sithBot_TryActivateLiftControl(state, thing, lift, 1))
                sithPlayerActions_Activate(thing);
            state->nextUseMs = sithTime_curMs + 900;
        }

        rdVector_Zero3(&thing->physicsParams.acceleration);
        sithBot_DampHorizontalVelocity(state, thing, 12.0);
        if (state->nextLiftLogMs <= sithTime_curMs && sithBot_debugLiftsLogged < 48)
        {
            sithBot_Logf("BotMatch: lift-ride slot=%d thing=%d targetFrame=%d curFrame=%d goalFrame=%d moving=%d\n",
                         state->playerIdx,
                         lift->thingIdx,
                         node->pathFrame,
                         lift->curframe,
                         lift->goalframe,
                         moving);
            sithBot_debugLiftsLogged++;
            state->nextLiftLogMs = sithTime_curMs + 750;
        }
        sithBot_SyncPositionIfNeeded(state, thing);
        return 1;
    }

    if (atStop)
    {
        /* Board an available car before considering its controls. */
        if (state->nextUseMs < sithTime_curMs + 800)
            state->nextUseMs = sithTime_curMs + 800;

        boardDir.x = node->pos.x - thing->position.x;
        boardDir.y = node->pos.y - thing->position.y;
        boardDir.z = 0.0;
        boardDist = rdVector_Normalize3Acc(&boardDir);
        if (boardDist < 0.55)
        {
            if (sithBot_TryAttachToPathLift(thing, lift))
            {
                state->ridingLiftThingIdx = lift->thingIdx;
                state->ridingLiftTargetNode = nextNode;
                if (state->nextLiftLogMs <= sithTime_curMs && sithBot_debugLiftsLogged < 48)
                {
                    sithBot_Logf("BotMatch: lift-attach slot=%d thing=%d frame=%d pos=(%.2f,%.2f,%.2f)\n",
                                 state->playerIdx,
                                 lift->thingIdx,
                                 node->pathFrame,
                                 thing->position.x,
                                 thing->position.y,
                                 thing->position.z);
                    sithBot_debugLiftsLogged++;
                    state->nextLiftLogMs = sithTime_curMs + 500;
                }
                sithBot_SyncPositionIfNeeded(state, thing);
                return 1;
            }

            if (boardDist < 0.38 && sithBot_AbsFlex(thing->position.z - node->pos.z) < 0.45)
            {
                /* The car is under the bot, but a narrow cross-sector ray can
                   miss its face. A locked relative attachment is equivalent
                   for a passenger who intentionally stands still during the
                   ride and is released at the destination frame. */
                sithThing_AttachThing(thing, lift);
                thing->attach_flags |= SITH_ATTACH_NO_MOVE;
                rdVector_Zero3(&thing->physicsParams.acceleration);
                rdVector_Zero3(&thing->physicsParams.vel);
                state->ridingLiftThingIdx = lift->thingIdx;
                state->ridingLiftTargetNode = nextNode;
                if (sithComm_multiplayerFlags)
                    sithDSSThing_SendSyncThingAttachment(thing, -1, 255, 1);
                sithBot_SyncPositionIfNeeded(state, thing);
                return 1;
            }

            sithBot_FaceToward(state, thing, &node->pos, 0);
            rdVector_Zero3(&thing->physicsParams.acceleration);
            if (boardDist > 0.035)
            {
                boardSpeed = boardDist * 4.0;
                if (boardSpeed < 0.25)
                    boardSpeed = 0.25;
                if (boardSpeed > 0.75)
                    boardSpeed = 0.75;
                thing->physicsParams.vel.x = boardDir.x * boardSpeed;
                thing->physicsParams.vel.y = boardDir.y * boardSpeed;
            }
            else
            {
                sithBot_DampHorizontalVelocity(state, thing, 12.0);
            }
            if (state->nextLiftLogMs <= sithTime_curMs && sithBot_debugLiftsLogged < 48)
            {
                sithBot_Logf("BotMatch: lift-board slot=%d thing=%d frame=%d dist=%.2f pos=(%.2f,%.2f,%.2f)\n",
                             state->playerIdx,
                             lift->thingIdx,
                             node->pathFrame,
                             boardDist,
                             thing->position.x,
                             thing->position.y,
                             thing->position.z);
                sithBot_debugLiftsLogged++;
                state->nextLiftLogMs = sithTime_curMs + 500;
            }
            sithBot_SyncPositionIfNeeded(state, thing);
            return 1;
        }
        return 0;
    }

    if (!moving && sithBot_MoveToLiftControl(state, thing, lift, node))
        return 1;

    dx = thing->position.x - node->pos.x;
    dy = thing->position.y - node->pos.y;
    if (!atStop && dx * dx + dy * dy < 0.90 * 0.90)
    {
        if (!moving && state->nextUseMs <= sithTime_curMs)
        {
            if (!sithBot_TryActivateLiftControl(state, thing, lift, 0))
                sithPlayerActions_Activate(thing);
            state->nextUseMs = sithTime_curMs + 900;
        }

        rdVector_Zero3(&thing->physicsParams.acceleration);
        sithBot_DampHorizontalVelocity(state, thing, 12.0);
        if (state->nextLiftLogMs <= sithTime_curMs && sithBot_debugLiftsLogged < 48)
        {
            sithBot_Logf("BotMatch: lift-wait slot=%d thing=%d frame=%d curFrame=%d goalFrame=%d moving=%d dist=%.2f\n",
                         state->playerIdx,
                         lift->thingIdx,
                         node->pathFrame,
                         lift->curframe,
                         lift->goalframe,
                         moving,
                         stdMath_Sqrt(dx * dx + dy * dy));
            sithBot_debugLiftsLogged++;
            state->nextLiftLogMs = sithTime_curMs + 750;
        }
        sithBot_SyncPositionIfNeeded(state, thing);
        return 1;
    }

    return 0;
}

static void sithBot_MoveToward(SithBotState *state, sithThing *thing, const rdVector3 *target, int combat)
{
    rdVector3 flat;
    rdVector3 localForward;
    rdVector3 localRight;
    flex_t dist;
    flex_t thrust;

    rdVector_Sub3(&flat, target, &thing->position);
    flat.z = 0.0;
    dist = rdVector_Normalize3Acc(&flat);
    if (dist <= 0.001)
    {
        rdVector_Zero3(&thing->physicsParams.acceleration);
        sithBot_DampHorizontalVelocity(state, thing, 8.0);
        return;
    }
    sithBot_AdjustMoveDirForPlayers(state, thing, &flat);

    if (state->interactionWaitUntilMs > sithTime_curMs)
    {
        rdVector_Zero3(&thing->physicsParams.acceleration);
        sithBot_DampHorizontalVelocity(state, thing, 10.0);
        return;
    }
    thrust = thing->actorParams.maxThrust + thing->actorParams.extraSpeed;
    if (thrust <= 0.0)
        thrust = 1.0;

    rdVector_Copy3(&localForward, &thing->lookOrientation.lvec);
    localForward.z = 0.0;
    if (rdVector_Normalize3Acc(&localForward) <= 0.001)
        rdVector_Copy3(&localForward, &flat);
    rdVector_Copy3(&localRight, &thing->lookOrientation.rvec);
    localRight.z = 0.0;
    if (rdVector_Normalize3Acc(&localRight) <= 0.001)
    {
        localRight.x = flat.y;
        localRight.y = -flat.x;
        localRight.z = 0.0;
    }

    thing->physicsParams.acceleration.x = rdVector_Dot3(&localRight, &flat) * thrust * 0.80;
    thing->physicsParams.acceleration.y = rdVector_Dot3(&localForward, &flat) * thrust * (dist < 0.8 ? 0.45 : 0.90);
    thing->physicsParams.acceleration.z = 0.0;

    /* Treat route-aligned controls like UT movers: press them on approach,
       then wait briefly for the connected door or platform to respond. */
    if (!combat && state->nextUseMs <= sithTime_curMs &&
        state->interactionRepeatUntilMs <= sithTime_curMs)
    {
        if (sithBot_TryActivateNearbyInteraction(state, thing, target, 1))
        {
            state->nextUseMs = sithTime_curMs + 650;
            rdVector_Zero3(&thing->physicsParams.acceleration);
            sithBot_DampHorizontalVelocity(state, thing, 10.0);
            return;
        }
        state->nextUseMs = sithTime_curMs + 250;
    }
    sithBot_DriveGroundVelocity(state, thing, &flat, dist, target->z - thing->position.z, combat);

    if (target->z - thing->position.z > 0.30 &&
        target->z - thing->position.z < 1.20 &&
        dist < 0.75 &&
        (thing->attach_flags &
         (SITH_ATTACH_WORLDSURFACE | SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)) &&
        stdMath_Fabs(thing->physicsParams.vel.z) < 0.15 &&
        state->nextUseMs <= sithTime_curMs)
    {
        sithPlayerActions_JumpWithVel(thing, 1.0);
        if (sithBot_debugJumpsLogged < 48)
        {
            sithBot_Logf("BotMatch: jump slot=%d reason=route-step dz=%.2f dist=%.2f\n",
                         state->playerIdx, target->z - thing->position.z, dist);
            sithBot_debugJumpsLogged++;
        }
        state->nextUseMs = sithTime_curMs + 700;
    }
}

static void sithBot_AdjustMoveDirForPlayers(SithBotState *state, sithThing *thing, rdVector3 *flatDir)
{
    rdVector3 adjusted;
    int i;

    if (!state || !thing || !flatDir)
        return;

    rdVector_Copy3(&adjusted, flatDir);
    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        sithThing *other = jkPlayer_playerInfos[i].playerThing;
        rdVector3 away;
        flex_t dist;
        flex_t separation;
        flex_t weight;

        if (i == state->playerIdx || !sithBot_IsThingAlivePlayer(other))
            continue;
        rdVector_Sub3(&away, &thing->position, &other->position);
        away.z = 0.0;
        dist = rdVector_Normalize3Acc(&away);
        separation = thing->moveSize + other->moveSize + 0.20;
        if (separation < 0.42)
            separation = 0.42;
        if (separation > 0.70)
            separation = 0.70;
        if (dist >= separation)
            continue;
        if (dist <= 0.001)
        {
            away.x = (state->playerIdx & 1) ? 1.0 : -1.0;
            away.y = 0.0;
        }
        weight = (separation - dist) / separation;
        if (weight > 0.65)
            weight = 0.65;
        adjusted.x += away.x * weight;
        adjusted.y += away.y * weight;
    }
    adjusted.z = 0.0;
    if (rdVector_Normalize3Acc(&adjusted) > 0.001)
        rdVector_Copy3(flatDir, &adjusted);
}

static void sithBot_SteerJumpPadFlight(SithBotState *state, sithThing *thing, const rdVector3 *target)
{
    rdVector3 flat;
    rdVector3 side;
    flex_t dist;
    flex_t desiredSpeed;
    flex_t response;
    flex_t horizontalSpeedSq;

    rdVector_Sub3(&flat, target, &thing->position);
    flat.z = 0.0;
    dist = rdVector_Normalize3Acc(&flat);
    horizontalSpeedSq = thing->physicsParams.vel.x * thing->physicsParams.vel.x +
                        thing->physicsParams.vel.y * thing->physicsParams.vel.y;
    if (dist > 0.35 && horizontalSpeedSq < 0.04 &&
        thing->position.z > state->jumpPadLaunchZ + 0.45 &&
        state->jumpPadDodgeUntilMs <= sithTime_curMs)
    {
        state->jumpPadDodgeSign = state->jumpPadDodgeSign ? -state->jumpPadDodgeSign : 1;
        state->jumpPadDodgeUntilMs = sithTime_curMs + 120;
        if (sithBot_debugJumpPadsLogged < 128)
        {
            sithBot_Logf("BotMatch: jump-pad-dodge slot=%d node=%d landing=%d sign=%d pos=(%.2f,%.2f,%.2f)\n",
                         state->playerIdx,
                         state->jumpPadLaunchNode,
                         state->jumpPadTargetNode,
                         state->jumpPadDodgeSign,
                         thing->position.x,
                         thing->position.y,
                         thing->position.z);
            sithBot_debugJumpPadsLogged++;
        }
    }
    if (state->jumpPadDodgeUntilMs > sithTime_curMs)
    {
        side.x = -flat.y * (flex_t)state->jumpPadDodgeSign;
        side.y = flat.x * (flex_t)state->jumpPadDodgeSign;
        side.z = 0.0;
        flat.x += side.x * 0.35;
        flat.y += side.y * 0.35;
        rdVector_Normalize3Acc(&flat);
    }
    desiredSpeed = dist < 0.03 ? 0.0 : dist * 4.0;
    if (desiredSpeed > 1.80)
        desiredSpeed = 1.80;
    response = state->frameDeltaSeconds * 18.0;
    if (response < 0.10)
        response = 0.10;
    if (response > 0.50)
        response = 0.50;

    thing->physicsParams.vel.x += (flat.x * desiredSpeed - thing->physicsParams.vel.x) * response;
    thing->physicsParams.vel.y += (flat.y * desiredSpeed - thing->physicsParams.vel.y) * response;
    rdVector_Zero3(&thing->physicsParams.acceleration);
}

static void sithBot_GetJumpPadLandingPos(int launchNode, int landingNode, rdVector3 *out)
{
    rdVector3 through;

    if (!out || launchNode < 0 || launchNode >= sithBot_numNodes ||
        landingNode < 0 || landingNode >= sithBot_numNodes)
    {
        return;
    }

    rdVector_Copy3(out, &sithBot_nodes[landingNode].pos);
    rdVector_Sub3(&through, &sithBot_nodes[landingNode].pos, &sithBot_nodes[launchNode].pos);
    through.z = 0.0;
    if (rdVector_Normalize3Acc(&through) > 0.001)
    {
        out->x += through.x * 0.10;
        out->y += through.y * 0.10;
    }
}

static int sithBot_DetectJumpPadLaunch(SithBotState *state, sithThing *thing)
{
    int bestPad = -1;
    int bestLanding = -1;
    flex_t bestPadDistSq = 3.4e38f;
    flex_t bestLandingScore = 3.4e38f;
    int i;

    if (!state || !thing || state->jumpPadAirUntilMs > sithTime_curMs ||
        thing->physicsParams.vel.z <= 2.40)
    {
        return 0;
    }

    for (i = 0; i < sithBot_numNodes; i++)
    {
        SithBotNode *pad = &sithBot_nodes[i];
        flex_t dx;
        flex_t dy;
        flex_t dz;
        flex_t horizontalSq;

        if (pad->kind != SITHBOT_NODE_JUMPPAD)
            continue;
        dx = thing->position.x - pad->pos.x;
        dy = thing->position.y - pad->pos.y;
        dz = thing->position.z - pad->pos.z;
        horizontalSq = dx * dx + dy * dy;
        if (horizontalSq > 0.64 || dz < -0.10 || dz > 0.90 || horizontalSq >= bestPadDistSq)
            continue;
        bestPad = i;
        bestPadDistSq = horizontalSq;
    }

    if (bestPad < 0)
        return 0;

    for (i = 0; i < sithBot_nodes[bestPad].edgeCount; i++)
    {
        int nodeIdx = sithBot_nodes[bestPad].edges[i];
        SithBotNode *candidate;
        flex_t score;

        if (nodeIdx < 0 || nodeIdx >= sithBot_numNodes)
            continue;
        candidate = &sithBot_nodes[nodeIdx];
        if (candidate->pos.z - sithBot_nodes[bestPad].pos.z < 0.35)
            continue;

        if (state->goalNode >= 0 && state->goalNode < sithBot_numNodes)
            score = sithBot_DistSq(&candidate->pos, &sithBot_nodes[state->goalNode].pos);
        else
            score = sithBot_DistSq(&candidate->pos, &thing->position);
        if (score < bestLandingScore)
        {
            bestLandingScore = score;
            bestLanding = nodeIdx;
        }
    }

    if (bestLanding < 0)
        return 0;

    state->jumpPadLaunchNode = bestPad;
    state->jumpPadTargetNode = bestLanding;
    state->jumpPadLaunchZ = sithBot_nodes[bestPad].pos.z;
    sithBot_GetJumpPadLandingPos(bestPad, bestLanding, &state->jumpPadLandingPos);
    state->jumpPadAirUntilMs = sithTime_curMs + 2400;
    state->jumpPadDodgeUntilMs = 0;
    state->jumpPadDodgeSign = 0;
    sithBot_qualityJumpDetected++;
    if (sithBot_debugJumpPadsLogged < 128)
    {
        sithBot_Logf("BotMatch: jump-pad-detected slot=%d node=%d landing=%d vz=%.2f dz=%.2f\n",
                     state->playerIdx,
                     bestPad,
                     bestLanding,
                     thing->physicsParams.vel.z,
                     sithBot_nodes[bestLanding].pos.z - sithBot_nodes[bestPad].pos.z);
        sithBot_debugJumpPadsLogged++;
    }
    return 1;
}

static int sithBot_HandleJumpPadRoute(SithBotState *state, sithThing *thing, int startNode, int nextNode)
{
    SithBotNode *launch;
    SithBotNode *landing;
    int grounded;

    if (!state || !thing)
        return 0;

    if (state->jumpPadAirUntilMs > sithTime_curMs)
    {
        if (state->jumpPadTargetNode < 0 || state->jumpPadTargetNode >= sithBot_numNodes)
        {
            state->jumpPadAirUntilMs = 0;
            return 0;
        }

        landing = &sithBot_nodes[state->jumpPadTargetNode];
        grounded = (thing->attach_flags &
                    (SITH_ATTACH_WORLDSURFACE | SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)) != 0;
        if (grounded && thing->position.z > state->jumpPadLaunchZ + 0.30)
        {
            sithBot_qualityJumpLanded++;
            if (sithBot_debugJumpPadsLogged < 128)
            {
                sithBot_Logf("BotMatch: jump-pad-landed slot=%d node=%d landing=%d pos=(%.2f,%.2f,%.2f)\n",
                             state->playerIdx,
                             state->jumpPadLaunchNode,
                             state->jumpPadTargetNode,
                             thing->position.x,
                             thing->position.y,
                             thing->position.z);
                sithBot_debugJumpPadsLogged++;
            }
            state->jumpPadAirUntilMs = 0;
            state->jumpPadLaunchNode = -1;
            state->jumpPadTargetNode = -1;
            state->routeWatchGoal = -1;
            state->routeWatchStartMs = 0;
            return 0;
        }
        if (grounded && sithTime_curMs + 2000 < state->jumpPadAirUntilMs)
        {
            return 1;
        }
        if (grounded &&
            thing->position.z < state->jumpPadLaunchZ + 0.25 &&
            stdMath_Fabs(thing->physicsParams.vel.z) < 0.25)
        {
            sithBot_qualityJumpRetry++;
            if (sithBot_debugJumpPadsLogged < 128)
            {
                sithBot_Logf("BotMatch: jump-pad-retry slot=%d node=%d landing=%d pos=(%.2f,%.2f,%.2f)\n",
                             state->playerIdx,
                             state->jumpPadLaunchNode,
                             state->jumpPadTargetNode,
                             thing->position.x,
                             thing->position.y,
                             thing->position.z);
                sithBot_debugJumpPadsLogged++;
            }
            state->jumpPadAirUntilMs = 0;
            state->jumpPadLaunchNode = -1;
            state->jumpPadTargetNode = -1;
            return 0;
        }
        if (grounded)
        {
            sithBot_qualityJumpFailed++;
            if (sithBot_debugJumpPadsLogged < 128)
            {
                sithBot_Logf("BotMatch: jump-pad-failed slot=%d node=%d landing=%d pos=(%.2f,%.2f,%.2f)\n",
                             state->playerIdx,
                             state->jumpPadLaunchNode,
                             state->jumpPadTargetNode,
                             thing->position.x,
                             thing->position.y,
                             thing->position.z);
                sithBot_debugJumpPadsLogged++;
            }
            state->jumpPadAirUntilMs = 0;
            state->jumpPadLaunchNode = -1;
            state->jumpPadTargetNode = -1;
            return 0;
        }

        sithBot_FaceToward(state, thing, &state->jumpPadLandingPos, 0);
        sithBot_SteerJumpPadFlight(state, thing, &state->jumpPadLandingPos);
        sithBot_SyncPositionIfNeeded(state, thing);
        return 1;
    }
    if (state->jumpPadAirUntilMs)
    {
        int remainedAtLaunch = state->jumpPadLaunchNode >= 0 &&
            state->jumpPadLaunchNode < sithBot_numNodes &&
            sithBot_DistSq(&thing->position, &sithBot_nodes[state->jumpPadLaunchNode].pos) < 0.64 &&
            thing->position.z < state->jumpPadLaunchZ + 0.25;

        if (remainedAtLaunch)
            sithBot_qualityJumpRetry++;
        else
            sithBot_qualityJumpTimeout++;
        if (sithBot_debugJumpPadsLogged < 128)
        {
            sithBot_Logf("BotMatch: jump-pad-%s slot=%d node=%d landing=%d pos=(%.2f,%.2f,%.2f)\n",
                         remainedAtLaunch ? "retry-expired" : "timeout",
                         state->playerIdx,
                         state->jumpPadLaunchNode,
                         state->jumpPadTargetNode,
                         thing->position.x,
                         thing->position.y,
                         thing->position.z);
            sithBot_debugJumpPadsLogged++;
        }
        state->jumpPadAirUntilMs = 0;
        state->jumpPadLaunchNode = -1;
        state->jumpPadTargetNode = -1;
        if (remainedAtLaunch)
        {
            state->routeWatchGoal = -1;
            state->routeWatchStartMs = 0;
        }
    }

    if (startNode < 0 || startNode >= sithBot_numNodes ||
        nextNode < 0 || nextNode >= sithBot_numNodes)
    {
        return 0;
    }

    launch = &sithBot_nodes[startNode];
    landing = &sithBot_nodes[nextNode];
    if (launch->kind != SITHBOT_NODE_JUMPPAD ||
        landing->pos.z - launch->pos.z < 0.35 ||
        sithBot_DistSq(&thing->position, &launch->pos) > 0.36)
    {
        return 0;
    }

    if (thing->physicsParams.vel.z > 0.20 ||
        !(thing->attach_flags &
          (SITH_ATTACH_WORLDSURFACE | SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)))
    {
        state->jumpPadLaunchNode = startNode;
        state->jumpPadTargetNode = nextNode;
        state->jumpPadLaunchZ = launch->pos.z;
        sithBot_GetJumpPadLandingPos(startNode, nextNode, &state->jumpPadLandingPos);
        state->jumpPadAirUntilMs = sithTime_curMs + 2400;
        state->jumpPadDodgeUntilMs = 0;
        state->jumpPadDodgeSign = 0;
        sithBot_qualityJumpDetected++;
        if (sithBot_debugJumpPadsLogged < 128)
        {
            sithBot_Logf("BotMatch: jump-pad-launch slot=%d node=%d landing=%d dz=%.2f\n",
                         state->playerIdx,
                         startNode,
                         nextNode,
                         landing->pos.z - launch->pos.z);
            sithBot_debugJumpPadsLogged++;
        }
        sithBot_FaceToward(state, thing, &state->jumpPadLandingPos, 0);
        sithBot_SteerJumpPadFlight(state, thing, &state->jumpPadLandingPos);
        sithBot_SyncPositionIfNeeded(state, thing);
        return 1;
    }

    sithBot_FaceToward(state, thing, &launch->pos, 0);
    sithBot_MoveToward(state, thing, &launch->pos, 0);
    sithBot_SyncPositionIfNeeded(state, thing);
    return 1;
}

static void sithBot_CheckStuck(SithBotState *state, sithThing *thing, const rdVector3 *target)
{
    if (!state->lastMoveCheckMs)
    {
        state->lastMoveCheckMs = sithTime_curMs;
        rdVector_Copy3(&state->lastMovePos, &thing->position);
        return;
    }

    if (sithTime_curMs - state->lastMoveCheckMs < SITHBOT_STUCK_MS)
        return;

    if (sithBot_DistSq(&state->lastMovePos, &thing->position) < 0.06 && sithBot_DistSq(&thing->position, target) > 0.0225)
    {
        rdVector3 escapeDir;
        flex_t safeSpeed = 2.4;
        flex_t targetDz = target->z - thing->position.z;

        state->stuckTicks++;
        if (targetDz > 0.30 && targetDz < 1.20 && state->nextUseMs <= sithTime_curMs)
        {
            sithBot_FaceToward(state, thing, target, 0);
            sithPlayerActions_JumpWithVel(thing, 1.0);
            state->nextUseMs = sithTime_curMs + 700;
            if (sithBot_debugJumpsLogged < 48)
            {
                sithBot_Logf("BotMatch: jump slot=%d reason=stalled-step dz=%.2f dist=%.2f\n",
                             state->playerIdx,
                             targetDz,
                             rdVector_Dist3(&thing->position, target));
                sithBot_debugJumpsLogged++;
            }
        }
        if (state->nextUseMs <= sithTime_curMs && state->interactionRepeatUntilMs <= sithTime_curMs)
        {
            sithBot_FaceToward(state, thing, target, 0);
            if (!sithBot_TryActivateNearbyInteraction(state, thing, target, 0))
                sithPlayerActions_Activate(thing);
            state->nextUseMs = sithTime_curMs + 900;
        }
        rdVector_Sub3(&escapeDir, target, &thing->position);
        escapeDir.z = 0.0;
        if (state->interactionRepeatUntilMs <= sithTime_curMs &&
            rdVector_Normalize3Acc(&escapeDir) > 0.001 &&
            sithBot_FindSafeMoveDir(thing, &escapeDir, safeSpeed,
                                    target->z - thing->position.z, &safeSpeed))
        {
            state->nextNode = -1;
            state->routeGoalNode = -1;
            state->routeCommitUntilMs = 0;
            rdVector_Copy3(&state->steeringDir, &escapeDir);
            state->steeringUntilMs = sithTime_curMs + 900;
            state->steeringTargetNode = -1;
            state->steeringCombat = 0;
        }
        else if (state->stuckTicks >= 3 && state->interactionRepeatUntilMs <= sithTime_curMs)
        {
            state->goalNode = -1;
            state->nextNode = -1;
            state->nextGoalMs = 0;
            state->routeGoalNode = -1;
            state->routeCommitUntilMs = 0;
            state->stuckTicks = 0;
        }
    }
    else
    {
        state->stuckTicks = 0;
    }

    state->lastMoveCheckMs = sithTime_curMs;
    rdVector_Copy3(&state->lastMovePos, &thing->position);
}

static void sithBot_SyncPositionIfNeeded(SithBotState *state, sithThing *thing)
{
    if (state->nextSyncMs <= sithTime_curMs)
    {
        if (sithComm_multiplayerFlags)
            sithDSSThing_SendPos(thing, -1, 0);
        state->nextSyncMs = sithTime_curMs + 500;
    }
}

static int sithBot_IsDirectDestinationSafe(sithThing *thing, const rdVector3 *destination)
{
    SithBotNode from;
    SithBotNode to;
    rdVector3 end;
    sithSector *destinationSector;

    if (!thing || !thing->sector || !destination)
        return 0;

    rdVector_Copy3(&end, destination);
    destinationSector = sithCollision_GetSectorLookAt(thing->sector, &thing->position, &end, 0.03);
    if (!sithBot_IsSectorSafeForBot(destinationSector))
        return 0;

    rdVector_Copy3(&from.pos, &thing->position);
    from.sector = thing->sector;
    from.kind = SITHBOT_NODE_FLOOR;
    from.thingIdx = -1;
    from.edgeCount = 0;

    rdVector_Copy3(&to.pos, destination);
    to.sector = destinationSector;
    to.kind = SITHBOT_NODE_FLOOR;
    to.thingIdx = -1;
    to.edgeCount = 0;
    return sithBot_IsWalkableSegment(&from, &to);
}

static int sithBot_TryCombatMoveCandidate(sithThing *thing, const rdVector3 *direction, flex_t stride, rdVector3 *destination)
{
    rdVector3 dir;
    int pass;

    if (!thing || !direction || !destination)
        return 0;

    rdVector_Copy3(&dir, direction);
    dir.z = 0.0;
    if (rdVector_Normalize3Acc(&dir) <= 0.001)
        return 0;

    for (pass = 0; pass < 3; pass++)
    {
        flex_t passStride = stride * (pass == 0 ? 1.0 : (pass == 1 ? 0.68 : 0.42));
        rdVector_Copy3(destination, &thing->position);
        destination->x += dir.x * passStride;
        destination->y += dir.y * passStride;
        if (sithBot_IsDirectDestinationSafe(thing, destination))
            return 1;
    }

    return 0;
}

static int sithBot_TryCombatSlideCandidate(sithThing *thing, const rdVector3 *direction, rdVector3 *destination)
{
    rdVector3 safeDir;
    rdVector3 end;
    sithSector *sector;
    flex_t safeSpeed = 2.8;
    int pass;

    if (!thing || !direction || !destination)
        return 0;

    rdVector_Copy3(&safeDir, direction);
    safeDir.z = 0.0;
    if (rdVector_Normalize3Acc(&safeDir) <= 0.001 ||
        !sithBot_FindSafeMoveDir(thing, &safeDir, safeSpeed, 0.0, &safeSpeed))
    {
        return 0;
    }

    /* UT's wall adjustment commits to a point along the obstruction normal.
       Use the validated alternate direction for several feet instead of
       choosing another left/right answer on the next frame. */
    for (pass = 0; pass < 2; pass++)
    {
        flex_t stride = pass == 0 ? 1.25 : 0.80;
        rdVector_Copy3(destination, &thing->position);
        destination->x += safeDir.x * stride;
        destination->y += safeDir.y * stride;
        rdVector_Copy3(&end, destination);
        sector = sithCollision_GetSectorLookAt(thing->sector, &thing->position, &end, 0.03);
        if (sithBot_IsSectorSafeForBot(sector) &&
            sithBot_PositionHasWalkableFootprint(thing, sector, destination, &safeDir, 0.35))
        {
            return 1;
        }
    }

    return 0;
}

static int sithBot_ChooseCombatDestination(SithBotState *state, sithThing *thing, sithThing *enemy,
                                           const SithBotWeaponSpec *spec, flex_t dist)
{
    rdVector3 forward;
    rdVector3 side;
    rdVector3 candidateDirs[7];
    flex_t holdMin = spec ? sithBot_GetCombatHoldMin(spec) : 0.8;
    flex_t holdMax = spec ? spec->idealDist + 1.4 : 1.25;
    flex_t radial = 0.0;
    flex_t lateral = 1.0;
    flex_t stride = 2.0;
    int mode = SITHBOT_COMBAT_TACTICAL;
    int lowHealth;
    int i;

    if (spec && holdMax > spec->maxDist)
        holdMax = spec->maxDist;
    if (holdMax < holdMin + 0.8)
        holdMax = holdMin + 0.8;

    rdVector_Sub3(&forward, &enemy->position, &thing->position);
    forward.z = 0.0;
    if (rdVector_Normalize3Acc(&forward) <= 0.001)
        return 0;

    if (!state->combatStrafeSign)
        state->combatStrafeSign = (state->playerIdx & 1) ? 1 : -1;
    side.x = -forward.y * (flex_t)state->combatStrafeSign;
    side.y = forward.x * (flex_t)state->combatStrafeSign;
    side.z = 0.0;

    if (holdMin < SITHBOT_COMBAT_SEPARATION_MIN)
        holdMin = SITHBOT_COMBAT_SEPARATION_MIN;

    /* JA's bot layer treats about 40% health as the point to stop pressing.
       Keep facing and firing, but create room so ordinary health-item routing
       can take over after LOS breaks. */
    lowHealth = thing->actorParams.maxHealth > 0.0 &&
        thing->actorParams.health <= thing->actorParams.maxHealth * 0.40;
    if ((state->explosiveBackoffUntilMs > sithTime_curMs && dist < 9.0) ||
        lowHealth ||
        dist < holdMin)
    {
        radial = -1.0;
        lateral = 0.45;
        stride = 2.5;
        mode = SITHBOT_COMBAT_RETREAT;
    }
    else if (dist > holdMax)
    {
        radial = 1.0;
        lateral = 0.32;
        stride = dist - holdMax + 1.0;
        if (stride < 1.6)
            stride = 1.6;
        if (stride > 3.0)
            stride = 3.0;
        mode = SITHBOT_COMBAT_CHARGE;
    }
    else
    {
        lateral = SITHBOT_COMBAT_HOLD_STRAFE;
        stride = 2.10;
    }

    candidateDirs[0].x = forward.x * radial + side.x * lateral;
    candidateDirs[0].y = forward.y * radial + side.y * lateral;
    candidateDirs[0].z = 0.0;
    candidateDirs[1].x = forward.x * radial;
    candidateDirs[1].y = forward.y * radial;
    candidateDirs[1].z = 0.0;
    rdVector_Copy3(&candidateDirs[2], &side);
    candidateDirs[3].x = forward.x * radial - side.x * lateral;
    candidateDirs[3].y = forward.y * radial - side.y * lateral;
    candidateDirs[3].z = 0.0;
    rdVector_Neg3(&candidateDirs[4], &side);
    rdVector_Copy3(&candidateDirs[5], &forward);
    rdVector_Neg3(&candidateDirs[6], &forward);

    /* Keep moving on the selected side whenever possible. Reversing before
       trying radial and same-side options produces visible pillar dancing. */
    candidateDirs[2].x += forward.x * radial * 0.20;
    candidateDirs[2].y += forward.y * radial * 0.20;
    candidateDirs[2].z = 0.0;

    for (i = 0; i < 7; i++)
    {
        if (sithBot_TryCombatMoveCandidate(thing, &candidateDirs[i], stride, &state->combatMoveTarget))
        {
            if (i == 3 || i == 4)
                state->combatStrafeSign = -state->combatStrafeSign;
            state->combatMode = mode;
            state->combatHasMoveTarget = 1;
            state->steeringUntilMs = 0;
            state->combatMoveUntilMs = sithTime_curMs + SITHBOT_TACTICAL_MOVE_MIN_MS +
                (uint32_t)(_frand() * (SITHBOT_TACTICAL_MOVE_MAX_MS - SITHBOT_TACTICAL_MOVE_MIN_MS));
            return 1;
        }
    }

    for (i = 0; i < 7; i++)
    {
        if (sithBot_TryCombatSlideCandidate(thing, &candidateDirs[i], &state->combatMoveTarget))
        {
            if (i == 3 || i == 4)
                state->combatStrafeSign = -state->combatStrafeSign;
            state->combatMode = mode;
            state->combatHasMoveTarget = 1;
            state->steeringUntilMs = 0;
            state->combatMoveUntilMs = sithTime_curMs + SITHBOT_TACTICAL_MOVE_MIN_MS +
                (uint32_t)(_frand() * 350.0);
            return 1;
        }
    }

    for (i = 2; i < 5; i++)
    {
        rdVector3 fallbackDir;
        rdVector_Copy3(&fallbackDir, &candidateDirs[i]);
        fallbackDir.x -= forward.x * 0.45;
        fallbackDir.y -= forward.y * 0.45;
        if (sithBot_TryCombatSlideCandidate(thing, &fallbackDir, &state->combatMoveTarget))
        {
            if (i >= 3)
                state->combatStrafeSign = -state->combatStrafeSign;
            state->combatMode = SITHBOT_COMBAT_HOLD;
            state->combatHasMoveTarget = 1;
            state->steeringUntilMs = 0;
            state->combatMoveUntilMs = sithTime_curMs + 1000 + (uint32_t)(_frand() * 400.0);
            return 1;
        }
    }

    state->combatMode = SITHBOT_COMBAT_HOLD;
    state->combatHasMoveTarget = 1;
    state->steeringUntilMs = 0;
    rdVector_Copy3(&state->combatMoveTarget, &thing->position);
    state->combatMoveTarget.x += side.x * 0.85 - forward.x * 0.65;
    state->combatMoveTarget.y += side.y * 0.85 - forward.y * 0.65;
    state->combatMoveUntilMs = sithTime_curMs + 900;
    return 1;
}

static void sithBot_MoveForCombat(SithBotState *state, sithThing *thing, sithThing *enemy)
{
    flex_t dist;
    int enemyIdx;
    int weaponBin;
    int movementWeaponBin;
    const SithBotWeaponSpec *spec;
    const SithBotWeaponSpec *movementSpec;

    if (!state || !thing || !enemy)
        return;

    dist = rdVector_Dist3(&thing->position, &enemy->position);
    enemyIdx = sithBot_GetPlayerSlotForThing(enemy);
    weaponBin = sithBot_ChooseFireWeapon(thing, dist);
    spec = sithBot_GetWeaponSpec(weaponBin);
    movementSpec = sithBot_GetBestOwnedRangedSpec(thing, &movementWeaponBin);
    if (movementSpec &&
        ((!spec && dist < sithBot_GetCombatHoldMin(movementSpec)) ||
         (spec && sithBot_IsSpecSafeForFireAtDist(thing, movementSpec, dist) && movementSpec->score >= spec->score + 80.0) ||
         (movementSpec->score >= 800.0 && dist < sithBot_GetCombatHoldMin(movementSpec))))
    {
        spec = movementSpec;
        weaponBin = movementWeaponBin;
    }

    if (state->combatTargetIdx != enemyIdx || state->combatMoveUntilMs <= sithTime_curMs ||
        !state->combatHasMoveTarget ||
        sithBot_DistSq(&thing->position, &state->combatMoveTarget) < 0.16 ||
        (state->blockedSinceMs && sithTime_curMs - state->blockedSinceMs > 700))
    {
        state->combatTargetIdx = enemyIdx;
        sithBot_ChooseCombatDestination(state, thing, enemy, spec, dist);
    }

    if (state->combatHasMoveTarget)
        sithBot_MoveToward(state, thing, &state->combatMoveTarget, 1);

    if (state->nextCombatLogMs <= sithTime_curMs && sithBot_debugCombatMovesLogged < 72)
    {
        sithBot_Logf("BotMatch: combat slot=%d target=%d weapon=%d mode=%d dist=%.2f health=%.2f committed=%d\n",
                     state->playerIdx,
                     enemyIdx,
                     weaponBin,
                     state->combatMode,
                     dist,
                     thing->actorParams.health,
                     state->combatHasMoveTarget);
        sithBot_debugCombatMovesLogged++;
        state->nextCombatLogMs = sithTime_curMs + 1200;
    }
}

static void sithBot_PlayForceAnimation(sithThing *thing)
{
    int track;

    if (!thing || !thing->rdthing.puppet)
        return;

    track = sithPuppet_PlayMode(thing, SITH_ANIM_MAGIC, 0);
    if (track >= 0 && sithComm_multiplayerFlags)
        sithDSSThing_SendPlayKeyMode(thing, SITH_ANIM_MAGIC, thing->rdthing.puppet->tracks[track].field_130, -1, 255);
}

static void sithBot_SpawnHealEffect(sithThing *thing)
{
    sithThing *effectTemplate;
    sithThing *effect;

    if (!thing || !thing->sector)
        return;

    effectTemplate = sithTemplate_GetEntryByName("+force_heal");
    if (!effectTemplate)
        return;

    effect = sithThing_SpawnTemplate(effectTemplate, thing);
    if (!effect)
        return;

    effect->lifeLeftMs = 1000;
    if (sithComm_multiplayerFlags)
        sithDSSThing_SendCreateThing(effectTemplate, effect, thing, 0, 0, 0, 255, 1);
    sithThing_AttachThing(effect, thing);
    effect->attach_flags |= 0x8;
    if (sithComm_multiplayerFlags)
        sithDSSThing_SendSyncThingAttachment(effect, -1, 255, 1);
}

static void sithBot_RegenerateForce(SithBotState *state, sithThing *thing)
{
    flex_t mana;

    if (!state || !thing)
        return;

    if (!state->nextForceRegenMs)
    {
        state->nextForceRegenMs = sithTime_curMs + 1000;
        return;
    }
    if (sithTime_curMs < state->nextForceRegenMs)
        return;

    state->nextForceRegenMs = sithTime_curMs + 1000;
    mana = sithInventory_GetBinAmount(thing, SITHBIN_FORCEMANA);
    if (mana < SITHBOT_FORCE_MANA_MAX)
        sithInventory_SetBinAmount(thing, SITHBIN_FORCEMANA, mana + 4.0);
}

static int sithBot_IsForcePushSafe(sithThing *enemy, const rdVector3 *forceDir)
{
    rdVector3 flatDir;

    if (!enemy || !enemy->sector || !forceDir)
        return 0;
    if (!sithBot_IsSectorSafeForBot(enemy->sector) ||
        sithBot_IsUpwardThrustSector(enemy->sector) ||
        sithBot_IsDynamicHazardSector(enemy->sector))
    {
        return 0;
    }

    rdVector_Copy3(&flatDir, forceDir);
    flatDir.z = 0.0;
    if (rdVector_Normalize3Acc(&flatDir) <= 0.001)
        return 0;

    /* Force Push should be usable on ordinary floors and platforms. A full
       player-footprint test at three future points rejected nearly every MotS
       encounter, especially beside walls. Two center-line floor samples still
       keep it away from immediate drops and damaging sectors. */
    return sithBot_IsMoveCenterSafeWithRise(enemy, &flatDir, 0.45, 0.55) &&
        sithBot_IsMoveCenterSafeWithRise(enemy, &flatDir, 0.95, 0.55);
}

static int sithBot_TryUseForce(SithBotState *state, sithThing *thing, sithThing *enemy)
{
    flex_t mana;
    flex_t dist;
    flex_t rank;

    if (!state || !thing || sithTime_curMs < state->nextForceMs)
        return 0;

    mana = sithInventory_GetBinAmount(thing, SITHBIN_FORCEMANA);
    rank = sithInventory_GetBinAmount(thing, SITHBIN_F_HEALING);
    if (rank > 0.0 && sithInventory_GetAvailable(thing, SITHBIN_F_HEALING) &&
        thing->actorParams.health > 0.0 &&
        thing->actorParams.health <= thing->actorParams.maxHealth * 0.42 &&
        mana >= SITHBOT_FORCE_HEAL_COST)
    {
        flex_t healthBefore = thing->actorParams.health;
        flex_t healAmount = 20.0 * rank;

        thing->actorParams.health += healAmount;
        if (thing->actorParams.health > thing->actorParams.maxHealth)
            thing->actorParams.health = thing->actorParams.maxHealth;
        sithInventory_ChangeInv(thing, SITHBIN_FORCEMANA, -SITHBOT_FORCE_HEAL_COST);
        sithBot_PlayForceAnimation(thing);
        sithBot_SpawnHealEffect(thing);
        state->nextForceMs = sithTime_curMs + 9000;
        state->nextFireMs = sithTime_curMs + 700;
        if (sithBot_debugForceLogged < 80)
        {
            sithBot_Logf("BotMatch: force-heal slot=%d rank=%.0f healthBefore=%.2f healthAfter=%.2f manaBefore=%.2f manaAfter=%.2f\n",
                         state->playerIdx,
                         rank,
                         healthBefore,
                         thing->actorParams.health,
                         mana,
                         sithInventory_GetBinAmount(thing, SITHBIN_FORCEMANA));
            sithBot_debugForceLogged++;
        }
        return 1;
    }

    if (!enemy || !sithBot_HasCombatLos(thing, enemy))
        return 0;

    dist = rdVector_Dist3(&thing->position, &enemy->position);
    if (Main_bMotsCompat)
    {
        rdVector3 forceDir;
        rdVector3 force;

        rank = sithInventory_GetBinAmount(thing, SITHBIN_F_PUSH);
        if (rank <= 0.0 || !sithInventory_GetAvailable(thing, SITHBIN_F_PUSH) ||
            mana < SITHBOT_FORCE_PUSH_COST || dist < 0.45 || dist > 1.55)
            return 0;

        rdVector_Sub3(&forceDir, &enemy->position, &thing->position);
        forceDir.z += 0.18;
        if (rdVector_Normalize3Acc(&forceDir) <= 0.001)
            return 0;
        if (!sithBot_IsForcePushSafe(enemy, &forceDir))
        {
            state->nextForceMs = sithTime_curMs + 1000;
            if (sithBot_debugForceLogged < 80)
            {
                sithBot_Logf("BotMatch: force-push-unsafe slot=%d target=%d dist=%.2f targetSectorFlags=0x%x\n",
                             state->playerIdx,
                             enemy->actorParams.playerinfo ? (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos) : -1,
                             dist,
                             enemy->sector ? enemy->sector->flags : 0);
                sithBot_debugForceLogged++;
            }
            return 0;
        }
        rdVector_Scale3(&force, &forceDir, 120.0 * rank);
        sithBot_FaceToward(state, thing, &enemy->position, 1);
        if (!sithBot_IsAimAligned(thing, &enemy->position, 0.86))
            return 0;
        sithBot_PlayForceAnimation(thing);
        sithThing_DetachThing(enemy);
        sithPhysics_ThingApplyForce(enemy, &force);
        sithThing_SetSyncFlags(enemy, THING_SYNC_POS);
        sithInventory_ChangeInv(thing, SITHBIN_FORCEMANA, -SITHBOT_FORCE_PUSH_COST);
        state->nextForceMs = sithTime_curMs + (uint32_t)(5200.0 + _frand() * 1800.0);
        state->nextFireMs = sithTime_curMs + 700;
        if (enemy->actorParams.playerinfo)
        {
            int enemySlot = (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos);
            int enemyStateIdx = sithBot_BotStateForPlayer(enemySlot);
            if (enemyStateIdx >= 0 && &sithBot_bots[enemyStateIdx] != state &&
                sithBot_bots[enemyStateIdx].nextForceMs < sithTime_curMs + 1200)
            {
                sithBot_bots[enemyStateIdx].nextForceMs = sithTime_curMs + 1200;
            }
        }
        if (sithBot_debugForceLogged < 80)
        {
            sithBot_Logf("BotMatch: force-push slot=%d target=%d rank=%.0f dist=%.2f manaBefore=%.2f manaAfter=%.2f\n",
                         state->playerIdx,
                         enemy->actorParams.playerinfo ? (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos) : -1,
                         rank,
                         dist,
                         mana,
                         sithInventory_GetBinAmount(thing, SITHBIN_FORCEMANA));
            sithBot_debugForceLogged++;
        }
        return 1;
    }
    else
    {
        sithThing *projectileTemplate;
        sithThing *spawned;
        rdVector3 fireOffset;
        rdVector3 aimError;

        rank = sithInventory_GetBinAmount(thing, SITHBIN_F_LIGHTNING);
        if (rank <= 0.0 || !sithInventory_GetAvailable(thing, SITHBIN_F_LIGHTNING) ||
            mana < SITHBOT_FORCE_LIGHTNING_COST || dist < 0.65 || dist > 1.55)
            return 0;

        projectileTemplate = sithTemplate_GetEntryByName("+force_lightning");
        if (!projectileTemplate)
        {
            state->nextForceMs = sithTime_curMs + 1000;
            if (sithBot_debugForceLogged < 80)
            {
                sithBot_Logf("BotMatch: force-no-template slot=%d power=lightning dist=%.2f\n", state->playerIdx, dist);
                sithBot_debugForceLogged++;
            }
            return 0;
        }

        sithBot_FaceToward(state, thing, &enemy->position, 1);
        if (!sithBot_IsAimAligned(thing, &enemy->position, 0.86))
            return 0;
        fireOffset.x = -0.025;
        fireOffset.y = 0.01;
        fireOffset.z = 0.0;
        rdVector_Zero3(&aimError);
        spawned = sithWeapon_FireProjectile(thing,
                                            projectileTemplate,
                                            0,
                                            SITH_ANIM_MAGIC,
                                            &fireOffset,
                                            &aimError,
                                            1.0,
                                            0,
                                            45.0,
                                            45.0,
                                            0);
        if (!spawned)
        {
            state->nextForceMs = sithTime_curMs + 500;
            return 0;
        }

        sithInventory_ChangeInv(thing, SITHBIN_FORCEMANA, -SITHBOT_FORCE_LIGHTNING_COST);
        sithCog_SendMessageFromThing(thing, spawned, SITH_MESSAGE_FIRE);
        state->nextForceMs = sithTime_curMs + (uint32_t)(1900.0 + _frand() * 700.0);
        state->nextFireMs = sithTime_curMs + 700;
        if (sithBot_debugForceLogged < 80)
        {
            sithBot_Logf("BotMatch: force-lightning slot=%d target=%d rank=%.0f projectileThing=%d dist=%.2f manaBefore=%.2f manaAfter=%.2f\n",
                         state->playerIdx,
                         enemy->actorParams.playerinfo ? (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos) : -1,
                         rank,
                         spawned->thingIdx,
                         dist,
                         mana,
                         sithInventory_GetBinAmount(thing, SITHBIN_FORCEMANA));
            sithBot_debugForceLogged++;
        }
        return 1;
    }
}

static void sithBot_FireAt(SithBotState *state, sithThing *thing, sithThing *enemy)
{
    int weaponBin;
    const SithBotWeaponSpec *spec;
    sithThing *projectile;
    sithThing *spawned;
    rdVector3 aim;
    rdVector3 dir;
    rdVector3 fireOffset;
    rdVector3 aimError;
    flex_t dist;

    if (sithTime_curMs < state->nextFireMs)
        return;

    rdVector_Copy3(&aim, &enemy->position);
    aim.z += 0.08;
    rdVector_Sub3(&dir, &aim, &thing->position);
    dist = rdVector_Normalize3Acc(&dir);
    if (dist <= 0.001)
        return;
    if (!sithBot_HasCombatLos(thing, enemy))
    {
        sithBot_qualityNoLosFireAttempts++;
        if (sithBot_debugNoLosFireLogged < 32)
        {
            sithBot_Logf("BotMatch: fire-no-los slot=%d target=%d dist=%.2f fromSectorFlags=%X toSectorFlags=%X\n",
                         state->playerIdx,
                         enemy->actorParams.playerinfo ? (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos) : -1,
                         dist,
                         thing->sector ? (unsigned int)thing->sector->flags : 0,
                         enemy->sector ? (unsigned int)enemy->sector->flags : 0);
            sithBot_debugNoLosFireLogged++;
        }
        state->nextFireMs = sithTime_curMs + 120;
        return;
    }
    weaponBin = sithBot_ChooseFireWeapon(thing, dist);
    if (weaponBin < 0)
        return;
    spec = sithBot_GetWeaponSpec(weaponBin);
    if (state->explosiveBackoffUntilMs > sithTime_curMs && sithBot_IsBlastWeaponSpec(spec))
    {
        weaponBin = sithBot_ChooseNonBlastWeapon(thing, dist);
        if (weaponBin < 0)
        {
            state->nextFireMs = sithTime_curMs + 120;
            return;
        }
        spec = sithBot_GetWeaponSpec(weaponBin);
    }
    if (weaponBin != sithInventory_GetCurWeapon(thing))
    {
        sithInventory_SetCurWeapon(thing, weaponBin);
        sithBot_ApplyWeaponPresentation(thing, weaponBin);
    }

    sithBot_FaceToward(state, thing, &aim, 1);

    if (sithBot_IsSaberBin(weaponBin) && dist < 1.8)
    {
        flex_t beforeHealth = enemy->actorParams.health;

        if (!sithBot_IsAimAligned(thing, &aim, 0.70))
            return;
        sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_FIRE1);
        sithThing_Damage(enemy, thing, 35.0, SITH_DAMAGE_SABER);
        if (weaponBin > 0 && weaponBin < SITHBIN_NUMBINS)
            sithBot_qualityWeaponShots[weaponBin]++;
        if (sithBot_debugHitsLogged < 80)
        {
            sithBot_Logf("BotMatch: hit slot=%d target=%d weapon=%d damage=35.00 healthBefore=%.2f healthAfter=%.2f dist=%.2f\n",
                         state->playerIdx,
                         enemy->actorParams.playerinfo ? (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos) : -1,
                         weaponBin,
                         beforeHealth,
                         enemy->actorParams.health,
                         dist);
            sithBot_debugHitsLogged++;
        }
        state->nextFireMs = sithTime_curMs + (uint32_t)(520.0 + _frand() * 360.0);
        return;
    }

    if (spec)
    {
        rdVector3 lead;
        flex_t projectileSpeed;
        flex_t leadSeconds = 0.0;
        flex_t ammoBefore = spec->ammoBin > 0 ? sithInventory_GetBinAmount(thing, spec->ammoBin) : 0.0;
        flex_t ammoAfter = ammoBefore;

        if (spec->ammoBin > 0 && ammoBefore < spec->ammoCost)
        {
            state->nextFireMs = sithTime_curMs + 250;
            return;
        }

        projectile = sithTemplate_GetEntryByName(spec->projectileName);
        if (!projectile)
        {
            if (sithBot_debugFireFailuresLogged < 16)
            {
                sithBot_Logf("BotMatch: fire-no-template slot=%d weapon=%d projectile='%s' dist=%.2f\n",
                             state->playerIdx,
                             weaponBin,
                             spec->projectileName,
                             dist);
                sithBot_debugFireFailuresLogged++;
            }
            state->nextFireMs = sithTime_curMs + 500;
            return;
        }

        projectileSpeed = rdVector_Len3(&projectile->physicsParams.vel);
        if (projectileSpeed > 0.1)
        {
            leadSeconds = dist / projectileSpeed;
            if (leadSeconds > 0.24)
                leadSeconds = 0.24;
            if (leadSeconds < 0.0)
                leadSeconds = 0.0;
            leadSeconds *= 0.55 + _frand() * 0.20;
        }

        rdVector_Copy3(&aim, &enemy->position);
        aim.z += 0.08;
        rdVector_Scale3(&lead, &enemy->physicsParams.vel, leadSeconds);
        rdVector_Add3Acc(&aim, &lead);
        sithBot_FaceToward(state, thing, &aim, 1);
        rdVector_Sub3(&dir, &aim, &thing->position);
        if (rdVector_Normalize3Acc(&dir) <= 0.001 ||
            !sithBot_IsAimAligned(thing, &aim, 0.92))
        {
            return;
        }

        rdVector_Copy3(&fireOffset, &spec->fireOffset);
        aimError.x = (_frand() - 0.5) * spec->spreadDeg;
        aimError.y = (_frand() - 0.5) * spec->spreadDeg;
        aimError.z = 0.0;

        sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_FIRE1);
        thing->actorParams.templateWeapon = projectile;
        spawned = sithWeapon_FireProjectile(thing,
                                            projectile,
                                            0,
                                            spec->mode,
                                            &fireOffset,
                                            &aimError,
                                            1.0,
                                            spec->scaleFlags,
                                            spec->autoAimFov,
                                            spec->autoAimMaxDist,
                                            0);
        if (spawned)
        {
            if (weaponBin > 0 && weaponBin < SITHBIN_NUMBINS)
                sithBot_qualityWeaponShots[weaponBin]++;
            if (spec->ammoBin > 0 && spec->ammoCost > 0.0)
                ammoAfter = sithInventory_ChangeInv(thing, spec->ammoBin, -spec->ammoCost);
            if (sithBot_IsBlastWeaponSpec(spec))
            {
                uint32_t backoffMs = strstr(spec->projectileName, "rail") ? 3300 : 1600;
                state->explosiveBackoffUntilMs = sithTime_curMs + backoffMs;
            }
            sithCog_SendMessageFromThing(thing, spawned, SITH_MESSAGE_FIRE);
            if (sithBot_debugShotsLogged < 80)
            {
                sithBot_Logf("BotMatch: fired slot=%d target=%d weapon=%d projectile='%s' projectileThing=%d dist=%.2f lead=%.2f ammoBin=%d ammoBefore=%.2f ammoAfter=%.2f\n",
                             state->playerIdx,
                             enemy->actorParams.playerinfo ? (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos) : -1,
                             weaponBin,
                             spec->projectileName,
                             spawned->thingIdx,
                             dist,
                             leadSeconds,
                             spec->ammoBin,
                             ammoBefore,
                             ammoAfter);
                sithBot_debugShotsLogged++;
            }
        }
        else if (sithBot_debugFireFailuresLogged < 16)
        {
            sithBot_Logf("BotMatch: fire-missed slot=%d weapon=%d projectileTemplate='%s' dist=%.2f\n",
                         state->playerIdx,
                         weaponBin,
                         spec->projectileName,
                         dist);
            sithBot_debugFireFailuresLogged++;
        }

        state->nextFireMs = sithTime_curMs + (uint32_t)(spec->fireWaitMs + _frand() * spec->fireWaitMs * 0.45);
        return;
    }

    if (dist < 1.25)
    {
        flex_t beforeHealth = enemy->actorParams.health;
        if (!sithBot_IsAimAligned(thing, &aim, 0.70))
            return;
        sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_FIRE1);
        sithThing_Damage(enemy, thing, 12.0, SITH_DAMAGE_IMPACT);
        if (sithBot_debugHitsLogged < 80)
        {
            sithBot_Logf("BotMatch: hit slot=%d target=%d weapon=%d damage=12.00 healthBefore=%.2f healthAfter=%.2f dist=%.2f\n",
                         state->playerIdx,
                         enemy->actorParams.playerinfo ? (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos) : -1,
                         weaponBin,
                         beforeHealth,
                         enemy->actorParams.health,
                         dist);
            sithBot_debugHitsLogged++;
        }
    }
    else if (sithBot_debugFireFailuresLogged < 16)
    {
        sithBot_Logf("BotMatch: fire-no-spec slot=%d weapon=%d dist=%.2f\n",
                     state->playerIdx,
                     weaponBin,
                     dist);
        sithBot_debugFireFailuresLogged++;
    }
    state->nextFireMs = sithTime_curMs + (uint32_t)(420.0 + _frand() * 360.0);
}

static int sithBot_RunHazardFlee(SithBotState *state, sithThing *thing)
{
    rdVector3 moveTarget;
    int startNode;
    int nextNode;

    if (!state || !thing || !state->hazardSector)
        return 0;

    if (state->hazardFleeUntilMs <= sithTime_curMs)
    {
        state->hazardFleeUntilMs = 0;
        state->hazardSector = 0;
        return 0;
    }

    if (sithBot_numNodes <= 0)
        return 0;

    if (state->goalNode < 0 || state->goalNode >= sithBot_numNodes ||
        sithBot_nodes[state->goalNode].sector == state->hazardSector ||
        sithBot_DistSq(&thing->position, &sithBot_nodes[state->goalNode].pos) < 0.8)
    {
        state->goalNode = sithBot_ChooseEscapeNode(thing, state->hazardSector, &state->hazardPos);
        state->nextGoalMs = sithTime_curMs + 3600;
        state->nextNode = -1;
        if (state->goalNode < 0)
        {
            rdVector3 away;
            rdVector_Sub3(&away, &thing->position, &state->hazardPos);
            away.z = 0.0;
            if (rdVector_Normalize3Acc(&away) <= 0.001)
            {
                away.x = -1.0;
                away.y = 0.0;
                away.z = 0.0;
            }
            moveTarget.x = thing->position.x + away.x * 3.0;
            moveTarget.y = thing->position.y + away.y * 3.0;
            moveTarget.z = thing->position.z;
            sithBot_FaceToward(state, thing, &moveTarget, 0);
            sithBot_MoveToward(state, thing, &moveTarget, 0);
            sithBot_SyncPositionIfNeeded(state, thing);
            return 1;
        }
    }

    startNode = sithBot_FindNearestNode(thing);
    nextNode = sithBot_FindRouteMoveNodeAvoidSector(startNode, state->goalNode, state->hazardSector, thing);
    if (nextNode < 0)
        nextNode = state->goalNode;
    if (nextNode < 0 || nextNode >= sithBot_numNodes)
        return 0;

    state->nextNode = nextNode;
    rdVector_Copy3(&moveTarget, &sithBot_nodes[nextNode].pos);
    if (state->nextCombatLogMs <= sithTime_curMs && sithBot_debugHazardMovesLogged < 48)
    {
        sithBot_Logf("BotMatch: hazard-route slot=%d goal=%d next=%d curFlags=%X hazardFlags=%X goalFlags=%X dist=%.2f\n",
                     state->playerIdx,
                     state->goalNode,
                     nextNode,
                     thing->sector ? (unsigned int)thing->sector->flags : 0,
                     state->hazardSector ? (unsigned int)state->hazardSector->flags : 0,
                     sithBot_nodes[state->goalNode].sector ? (unsigned int)sithBot_nodes[state->goalNode].sector->flags : 0,
                     rdVector_Dist3(&thing->position, &moveTarget));
        sithBot_debugHazardMovesLogged++;
        state->nextCombatLogMs = sithTime_curMs + 800;
    }
    if (sithBot_HandlePathLiftRoute(state, thing, nextNode))
        return 1;
    sithBot_FaceToward(state, thing, &moveTarget, 0);
    sithBot_MoveToward(state, thing, &moveTarget, 0);
    sithBot_CheckStuck(state, thing, &moveTarget);
    sithBot_SyncPositionIfNeeded(state, thing);
    return 1;
}

static int sithBot_RunArmGoal(SithBotState *state, sithThing *thing, sithThing *enemyThing)
{
    rdVector3 moveTarget;
    int startNode;
    int nextNode;

    if (!state || !thing || sithBot_numNodes <= 0)
        return 0;
    if (state->goalMode == SITHBOT_GOAL_TACTICAL_ITEM)
        return 0;
    if (!sithBot_ShouldSeekWeaponPickup(thing, enemyThing))
    {
        if (state->goalMode == SITHBOT_GOAL_ARM)
        {
            state->goalNode = -1;
            state->nextNode = -1;
            state->nextGoalMs = 0;
            state->routeGoalNode = -1;
            state->routeCommitUntilMs = 0;
            state->goalMode = SITHBOT_GOAL_ROAM;
        }
        return 0;
    }

    if (state->goalMode == SITHBOT_GOAL_ARM &&
        (state->goalNode < 0 || state->goalNode >= sithBot_numNodes ||
         !sithBot_IsItemNodeAvailable(state->goalNode)))
    {
        state->goalNode = -1;
        state->nextNode = -1;
        state->nextGoalMs = 0;
        state->goalMode = SITHBOT_GOAL_ROAM;
    }

    if (state->goalMode == SITHBOT_GOAL_ARM &&
        state->goalNode >= 0 && state->goalNode < sithBot_numNodes)
    {
        flex_t armDist = rdVector_Dist3(&thing->position, &sithBot_nodes[state->goalNode].pos);
        sithThing *item = sithThing_GetThingByIdx(sithBot_nodes[state->goalNode].thingIdx);
        if (armDist + 0.30 < state->armBestDist)
        {
            state->armBestDist = armDist;
            state->nextGoalMs = sithTime_curMs + 3500;
        }
        if (sithBot_IsItemAvailable(item) && sithBot_IsWithinPickupRange(thing, item))
        {
            state->nextPickupMs = 0;
            sithBot_TryPickupItem(state, thing, item, 1);
            if (!sithBot_IsItemAvailable(item))
            {
                state->goalNode = -1;
                state->nextNode = -1;
                state->nextGoalMs = 0;
                state->goalMode = SITHBOT_GOAL_ROAM;
                return 0;
            }
        }
    }

    if (state->goalMode != SITHBOT_GOAL_ARM || state->goalNode < 0 || state->goalNode >= sithBot_numNodes ||
        sithTime_curMs >= state->nextGoalMs)
    {
        if (state->goalMode == SITHBOT_GOAL_ARM && state->goalNode >= 0 && state->goalNode < sithBot_numNodes)
        {
            flex_t armDist = rdVector_Dist3(&thing->position, &sithBot_nodes[state->goalNode].pos);
            sithThing *item = sithThing_GetThingByIdx(sithBot_nodes[state->goalNode].thingIdx);
            if (sithBot_IsItemAvailable(item) && sithBot_IsWithinPickupRange(thing, item))
            {
                state->nextPickupMs = 0;
                sithBot_TryPickupItem(state, thing, item, 1);
                if (!sithBot_IsItemAvailable(item))
                {
                    state->goalNode = -1;
                    state->nextNode = -1;
                    state->nextGoalMs = 0;
                    state->goalMode = SITHBOT_GOAL_ROAM;
                    return 0;
                }

                sithBot_RejectArmThing(state, item->thingIdx);
                if (sithBot_debugArmRejectsLogged < 32)
                {
                    sithBot_Logf("BotMatch: arm-reject slot=%d itemThing=%d item='%s' dist=%.2f\n",
                                 state->playerIdx,
                                 item->thingIdx,
                                 sithBot_GetItemTemplateName(item),
                                 armDist);
                    sithBot_debugArmRejectsLogged++;
                }
            }
            else if (item && sithTime_curMs >= state->nextGoalMs)
            {
                sithBot_RejectArmThing(state, item->thingIdx);
                if (sithBot_debugArmRejectsLogged < 32)
                {
                    sithBot_Logf("BotMatch: arm-stalled slot=%d itemThing=%d item='%s' dist=%.2f bestDist=%.2f\n",
                                 state->playerIdx,
                                 item->thingIdx,
                                 sithBot_GetItemTemplateName(item),
                                 armDist,
                                 state->armBestDist);
                    sithBot_debugArmRejectsLogged++;
                }
            }
            state->goalNode = -1;
            state->nextNode = -1;
            state->nextGoalMs = 0;
        }

        {
            int armNode = sithBot_ChooseArmGoalNode(state, thing);
            if (armNode < 0)
                return 0;

            state->goalNode = armNode;
            state->goalMode = SITHBOT_GOAL_ARM;
            state->nextGoalMs = sithTime_curMs + (uint32_t)(3000.0 + _frand() * 1700.0);
            state->armBestDist = rdVector_Dist3(&thing->position, &sithBot_nodes[armNode].pos);
            state->nextNode = -1;
            if (sithBot_debugArmGoalsLogged < 36)
            {
                sithThing *item = sithThing_GetThingByIdx(sithBot_nodes[armNode].thingIdx);
                const char *name = sithBot_GetItemTemplateName(item);
                sithBot_Logf("BotMatch: arm slot=%d goalNode=%d item='%s' bestWeaponScore=%.0f dist=%.2f\n",
                             state->playerIdx,
                             armNode,
                             name,
                             sithBot_GetBestOwnedRangedScore(thing),
                             rdVector_Dist3(&thing->position, &sithBot_nodes[armNode].pos));
                sithBot_debugArmGoalsLogged++;
            }
        }
    }

    startNode = sithBot_FindNearestNode(thing);
    nextNode = sithBot_FindCommittedRouteMoveNode(state, startNode, state->goalNode, thing);
    if (nextNode < 0 || nextNode >= sithBot_numNodes)
        return 0;

    state->nextNode = nextNode;
    if (sithBot_HandleJumpPadRoute(state, thing, startNode, nextNode))
    {
        if (enemyThing)
            sithBot_FireAt(state, thing, enemyThing);
        return 1;
    }
    rdVector_Copy3(&moveTarget, &sithBot_nodes[nextNode].pos);
    if (state->goalNode >= 0 && state->goalNode < sithBot_numNodes &&
        nextNode == state->goalNode && sithBot_nodes[state->goalNode].kind == SITHBOT_NODE_ITEM)
    {
        sithThing *item = sithThing_GetThingByIdx(sithBot_nodes[state->goalNode].thingIdx);
        if (sithBot_IsItemAvailable(item))
            rdVector_Copy3(&moveTarget, &item->position);
    }
    if (sithBot_HandlePathLiftRoute(state, thing, nextNode))
    {
        if (enemyThing)
            sithBot_FireAt(state, thing, enemyThing);
        return 1;
    }
    sithBot_FaceToward(state, thing, enemyThing ? &enemyThing->position : &moveTarget, enemyThing != 0);
    sithBot_MoveToward(state, thing, &moveTarget, 0);
    sithBot_CheckStuck(state, thing, &moveTarget);
    sithBot_SyncPositionIfNeeded(state, thing);

    if (enemyThing && rdVector_Dist3(&thing->position, &enemyThing->position) > 2.35)
        sithBot_FireAt(state, thing, enemyThing);

    return 1;
}

static int sithBot_RunTacticalPickupGoal(SithBotState *state, sithThing *thing, sithThing *visibleEnemy)
{
    sithThing *item;
    rdVector3 moveTarget;
    int startNode;
    int nextNode;

    if (!state || !thing || sithBot_numNodes <= 0)
        return 0;
    if (state->goalMode == SITHBOT_GOAL_ARM)
        return 0;

    if (state->goalMode == SITHBOT_GOAL_TACTICAL_ITEM &&
        (state->goalNode < 0 || state->goalNode >= sithBot_numNodes ||
         !sithBot_IsItemNodeAvailable(state->goalNode) ||
         sithTime_curMs >= state->nextGoalMs))
    {
        state->goalNode = -1;
        state->nextNode = -1;
        state->nextGoalMs = 0;
        state->routeGoalNode = -1;
        state->routeCommitUntilMs = 0;
        state->goalMode = SITHBOT_GOAL_ROAM;
    }

    if (state->goalMode != SITHBOT_GOAL_TACTICAL_ITEM)
    {
        int pickupNode;

        if (!visibleEnemy || state->nextTacticalPickupMs > sithTime_curMs)
            return 0;
        state->nextTacticalPickupMs = sithTime_curMs + 800;
        pickupNode = sithBot_ChooseTacticalPickupNode(thing);
        if (pickupNode < 0)
            return 0;

        state->goalNode = pickupNode;
        state->nextNode = -1;
        state->nextGoalMs = sithTime_curMs + 2800;
        state->routeGoalNode = -1;
        state->routeCommitUntilMs = 0;
        state->goalMode = SITHBOT_GOAL_TACTICAL_ITEM;
        if (sithBot_debugTacticalPickupsLogged < 40)
        {
            item = sithThing_GetThingByIdx(sithBot_nodes[pickupNode].thingIdx);
            sithBot_Logf("BotMatch: tactical-pickup slot=%d goalNode=%d item='%s' desire=%.1f dist=%.2f health=%.1f\n",
                         state->playerIdx,
                         pickupNode,
                         sithBot_GetItemTemplateName(item),
                         sithBot_GetItemDesire(thing, item),
                         rdVector_Dist3(&thing->position, &sithBot_nodes[pickupNode].pos),
                         thing->actorParams.health);
            sithBot_debugTacticalPickupsLogged++;
        }
    }

    item = sithThing_GetThingByIdx(sithBot_nodes[state->goalNode].thingIdx);
    if (!sithBot_IsItemAvailable(item))
        return 0;
    if (sithBot_IsWithinPickupRange(thing, item))
    {
        state->nextPickupMs = 0;
        sithBot_TryPickupItem(state, thing, item, 1);
        if (!sithBot_IsItemAvailable(item))
        {
            state->goalNode = -1;
            state->nextNode = -1;
            state->nextGoalMs = 0;
            state->routeGoalNode = -1;
            state->routeCommitUntilMs = 0;
            state->goalMode = SITHBOT_GOAL_ROAM;
            return 0;
        }
    }

    if (sithBot_CheckRouteGoalProgress(state, thing))
        return 1;
    startNode = sithBot_FindNearestNode(thing);
    nextNode = sithBot_FindCommittedRouteMoveNode(state, startNode, state->goalNode, thing);
    if (nextNode < 0 || nextNode >= sithBot_numNodes)
    {
        state->goalNode = -1;
        state->nextNode = -1;
        state->nextGoalMs = 0;
        state->routeGoalNode = -1;
        state->routeCommitUntilMs = 0;
        state->goalMode = SITHBOT_GOAL_ROAM;
        return 0;
    }

    state->nextNode = nextNode;
    if (sithBot_HandleJumpPadRoute(state, thing, startNode, nextNode))
    {
        if (visibleEnemy)
            sithBot_FireAt(state, thing, visibleEnemy);
        return 1;
    }
    rdVector_Copy3(&moveTarget, &sithBot_nodes[nextNode].pos);
    if (nextNode == state->goalNode)
        rdVector_Copy3(&moveTarget, &item->position);
    if (sithBot_HandlePathLiftRoute(state, thing, nextNode))
    {
        if (visibleEnemy)
            sithBot_FireAt(state, thing, visibleEnemy);
        return 1;
    }

    sithBot_FaceToward(state, thing, visibleEnemy ? &visibleEnemy->position : &moveTarget, visibleEnemy != 0);
    sithBot_MoveToward(state, thing, &moveTarget, 0);
    sithBot_CheckStuck(state, thing, &moveTarget);
    sithBot_SyncPositionIfNeeded(state, thing);
    if (visibleEnemy)
        sithBot_FireAt(state, thing, visibleEnemy);
    return 1;
}

static void sithBot_TickState(SithBotState *state, flex_t deltaSeconds, int deltaMs)
{
    sithPlayerInfo *info;
    sithThing *thing;
    rdVector3 moveTarget;
    sithThing *enemyThing = 0;
    sithThing *huntThing = 0;
    const rdVector3 *huntPos = 0;
    sithSector *huntSector = 0;
    int startNode;
    int nextNode;
    int huntIdx;
    int usedForce;
    int enemyVisible = 0;

    (void)deltaMs;

    if (!state->active || state->playerIdx < 0 || state->playerIdx >= jkPlayer_maxPlayers)
        return;

    state->frameDeltaSeconds = deltaSeconds;
    if (state->frameDeltaSeconds <= 0.0 || state->frameDeltaSeconds > 0.10)
        state->frameDeltaSeconds = 1.0 / 60.0;

    info = &jkPlayer_playerInfos[state->playerIdx];
    thing = info->playerThing;
    if (!thing || !sithBot_IsBotNetId(info->net_id))
    {
        state->active = 0;
        return;
    }

    info->lastUpdateMs = sithTime_curMs;

    if (sithBot_TryRecoverFromFall(state, thing))
        return;
    sithBot_RecordSafeAnchor(state, thing);

    if ((thing->thingflags & SITH_TF_DEAD) || thing->actorParams.health <= 0.0)
    {
        rdVector_Zero3(&thing->physicsParams.acceleration);
        if (!state->respawnAtMs)
            state->respawnAtMs = sithTime_curMs + SITHBOT_RESPAWN_MS;
        if (sithTime_curMs >= state->respawnAtMs)
        {
            int playerIdx = state->playerIdx;
            sithBot_Respawn(state->playerIdx);
            sithBot_ResetState(state, playerIdx);
            rdVector_Copy3(&state->lastMovePos, &thing->position);
            state->lastMoveCheckMs = sithTime_curMs;
        }
        return;
    }

    if (state->nextProgressLogMs <= sithTime_curMs && sithBot_debugProgressLogged < 160)
    {
        rdVector3 horizontalVel;
        horizontalVel.x = thing->physicsParams.vel.x;
        horizontalVel.y = thing->physicsParams.vel.y;
        horizontalVel.z = 0.0;
        sithBot_Logf("BotMatch: progress slot=%d sector=%d pos=(%.2f,%.2f,%.2f) speed=%.2f goal=%d next=%d mode=%d anim=%d flags=0x%X\n",
                     state->playerIdx,
                     sithBot_GetSectorIndex(thing->sector),
                     thing->position.x,
                     thing->position.y,
                     thing->position.z,
                     rdVector_Len3(&horizontalVel),
                     state->goalNode,
                     state->nextNode,
                     state->goalMode,
                     thing->puppet ? thing->puppet->currentAnimation : -1,
                     (unsigned int)thing->thingflags);
        state->nextProgressLogMs = sithTime_curMs + 1800;
        sithBot_debugProgressLogged++;
    }

    sithBot_RegenerateForce(state, thing);

    sithBot_TryPickupNearby(state, thing);

    if (sithBot_RunHazardFlee(state, thing))
        return;

    if (sithBot_GetPathLiftForNode(state->nextNode))
    {
        SithBotNode *liftNode = &sithBot_nodes[state->nextNode];
        sithThing *lift = sithBot_GetPathLiftForNode(state->nextNode);
        flex_t dx = thing->position.x - liftNode->pos.x;
        flex_t dy = thing->position.y - liftNode->pos.y;
        int attached = (thing->attach_flags & (SITH_ATTACH_THING | SITH_ATTACH_THINGSURFACE)) &&
            thing->attachedThing == lift;

        if (attached || dx * dx + dy * dy < 1.25 * 1.25)
        {
            rdVector_Copy3(&moveTarget, &liftNode->pos);
            if (!sithBot_HandlePathLiftRoute(state, thing, state->nextNode))
            {
                sithBot_FaceToward(state, thing, &moveTarget, 0);
                sithBot_MoveToward(state, thing, &moveTarget, 0);
                sithBot_CheckStuck(state, thing, &moveTarget);
                sithBot_SyncPositionIfNeeded(state, thing);
            }
            return;
        }
    }

    state->enemyIdx = sithBot_FindEnemy(state, thing);
    if (state->enemyIdx >= 0)
    {
        enemyThing = jkPlayer_playerInfos[state->enemyIdx].playerThing;
        enemyVisible = sithBot_HasCombatLos(thing, enemyThing);
        if (enemyVisible)
        {
            state->lastSeenEnemyIdx = state->enemyIdx;
            state->lastEnemySeenMs = sithTime_curMs;
            rdVector_Copy3(&state->lastEnemySeenPos, &enemyThing->position);
            state->lastEnemySeenSector = enemyThing->sector;
        }
    }

    sithBot_DetectJumpPadLaunch(state, thing);
    if (sithBot_HandleJumpPadRoute(state, thing, -1, -1))
    {
        if (enemyThing && enemyVisible)
            sithBot_FireAt(state, thing, enemyThing);
        return;
    }

    if (sithBot_RunArmGoal(state, thing, enemyVisible ? enemyThing : 0))
        return;

    if (sithBot_RunTacticalPickupGoal(state, thing, enemyVisible ? enemyThing : 0))
        return;

    if (enemyThing && enemyVisible)
    {
        int targetChanged = state->combatTargetIdx != state->enemyIdx;

        if (targetChanged)
        {
            state->combatTargetIdx = state->enemyIdx;
            state->combatHasMoveTarget = 0;
            state->steeringUntilMs = 0;
        }
        rdVector_Copy3(&moveTarget, &enemyThing->position);
        sithBot_FaceToward(state, thing, &moveTarget, 1);
        usedForce = sithBot_TryUseForce(state, thing, enemyThing);
        sithBot_MoveForCombat(state, thing, enemyThing);
        if (!usedForce)
            sithBot_FireAt(state, thing, enemyThing);
        sithBot_ResetRouteProgressWatch(state);
        return;
    }
    else if (enemyThing)
    {
        if (state->lastSeenEnemyIdx == state->enemyIdx &&
            sithTime_curMs - state->lastEnemySeenMs <= SITHBOT_TARGET_COMMIT_MS)
        {
            sithBot_FaceToward(state, thing, &state->lastEnemySeenPos, 1);
            if (state->combatHasMoveTarget &&
                state->combatMoveUntilMs > sithTime_curMs &&
                sithBot_IsDirectDestinationSafe(thing, &state->combatMoveTarget))
            {
                sithBot_MoveToward(state, thing, &state->combatMoveTarget, 1);
            }
            else if (sithBot_IsDirectDestinationSafe(thing, &state->lastEnemySeenPos))
            {
                sithBot_MoveToward(state, thing, &state->lastEnemySeenPos, 1);
            }
            else
            {
                rdVector_Zero3(&thing->physicsParams.acceleration);
                sithBot_DampHorizontalVelocity(state, thing, 7.0);
            }
            sithBot_SyncPositionIfNeeded(state, thing);
            sithBot_ResetRouteProgressWatch(state);
            return;
        }
        if (thing->actorParams.maxHealth > 0.0 &&
            thing->actorParams.health <= thing->actorParams.maxHealth * 0.40)
        {
            state->enemyIdx = -1;
            state->lastSeenEnemyIdx = -1;
            state->lastEnemySeenMs = 0;
            state->lastEnemySeenSector = 0;
            state->combatTargetIdx = -1;
            state->combatMode = SITHBOT_COMBAT_NONE;
            state->combatHasMoveTarget = 0;
            state->goalNode = -1;
            state->nextNode = -1;
            state->nextGoalMs = 0;
            state->goalMode = SITHBOT_GOAL_ROAM;
        }
        else
        {
            state->combatMode = SITHBOT_COMBAT_HUNT;
            state->combatHasMoveTarget = 0;
            huntIdx = state->enemyIdx;
            huntThing = enemyThing;
            huntPos = &state->lastEnemySeenPos;
            huntSector = state->lastEnemySeenSector;
        }
        enemyThing = 0;
    }
    else
    {
        sithBot_TryUseForce(state, thing, 0);
    }

    if (sithBot_numNodes <= 0)
        return;

    if (!huntThing && state->lastSeenEnemyIdx >= 0 && state->lastSeenEnemyIdx < jkPlayer_maxPlayers &&
        sithTime_curMs - state->lastEnemySeenMs <= SITHBOT_LAST_SEEN_MS &&
        sithBot_IsThingAlivePlayer(jkPlayer_playerInfos[state->lastSeenEnemyIdx].playerThing))
    {
        huntIdx = state->lastSeenEnemyIdx;
        huntThing = jkPlayer_playerInfos[huntIdx].playerThing;
        huntPos = &state->lastEnemySeenPos;
        huntSector = state->lastEnemySeenSector;
    }
    if (state->goalMode == SITHBOT_GOAL_HUNT &&
        state->goalNode >= 0 && state->goalNode < sithBot_numNodes &&
        sithBot_DistSq(&thing->position, &sithBot_nodes[state->goalNode].pos) < 0.8)
    {
        state->nextGoalMs = 0;
    }
    if (huntThing && huntPos && huntSector &&
        (state->goalMode != SITHBOT_GOAL_HUNT || state->goalNode < 0 || sithTime_curMs >= state->nextGoalMs))
    {
        int huntNode = sithBot_ChooseHuntGoalNode(thing, huntPos, huntSector);
        if (huntNode >= 0)
        {
            state->goalNode = huntNode;
            state->goalMode = SITHBOT_GOAL_HUNT;
            state->nextGoalMs = sithTime_curMs + (enemyThing
                ? SITHBOT_HUNT_REPLAN_MS
                : (uint32_t)(3500.0 + _frand() * 1800.0));
            state->nextNode = -1;
            state->routeGoalNode = -1;
            state->routeCommitUntilMs = 0;
            if (sithBot_debugHuntsLogged < 24)
            {
                sithBot_Logf("BotMatch: hunt slot=%d target=%d goalNode=%d\n", state->playerIdx, huntIdx, huntNode);
                sithBot_debugHuntsLogged++;
            }
        }
    }

    if (state->goalNode >= 0 && state->goalNode < sithBot_numNodes && !sithBot_IsItemNodeAvailable(state->goalNode))
    {
        state->goalNode = -1;
        state->nextNode = -1;
        state->nextGoalMs = 0;
        state->goalMode = SITHBOT_GOAL_ROAM;
    }

    if (state->goalNode < 0 || state->goalNode >= sithBot_numNodes || sithTime_curMs >= state->nextGoalMs ||
        (state->goalMode != SITHBOT_GOAL_HUNT &&
         sithBot_DistSq(&thing->position, &sithBot_nodes[state->goalNode].pos) < 0.8))
    {
        state->goalNode = sithBot_ChooseGoalNode(state, thing);
        state->goalMode = SITHBOT_GOAL_ROAM;
        state->nextGoalMs = sithTime_curMs + (uint32_t)(10000.0 + _frand() * 6000.0);
        state->nextNode = -1;
        state->routeGoalNode = -1;
        state->routeCommitUntilMs = 0;
    }
    if (sithBot_CheckRouteGoalProgress(state, thing))
        return;

    startNode = sithBot_FindNearestNode(thing);
    nextNode = sithBot_FindCommittedRouteMoveNode(state, startNode, state->goalNode, thing);
    if (nextNode < 0 || nextNode >= sithBot_numNodes)
    {
        if (state->goalMode == SITHBOT_GOAL_HUNT)
        {
            state->enemyIdx = -1;
            state->lastSeenEnemyIdx = -1;
            state->lastEnemySeenMs = 0;
            state->lastEnemySeenSector = 0;
            state->combatTargetIdx = -1;
        }
        state->goalNode = -1;
        state->nextNode = -1;
        state->nextGoalMs = 0;
        state->routeGoalNode = -1;
        state->routeCommitUntilMs = 0;
        state->routeWatchGoal = -1;
        state->routeWatchStartMs = 0;
        return;
    }

    if (sithBot_HandleJumpPadRoute(state, thing, startNode, nextNode))
    {
        if (enemyThing)
            sithBot_FireAt(state, thing, enemyThing);
        return;
    }

    if (nextNode >= 0 && nextNode < sithBot_numNodes)
        rdVector_Copy3(&moveTarget, &sithBot_nodes[nextNode].pos);
    else
        rdVector_Copy3(&moveTarget, &thing->position);

    if (sithBot_HandlePathLiftRoute(state, thing, nextNode))
    {
        if (enemyThing)
            sithBot_FireAt(state, thing, enemyThing);
        return;
    }
    sithBot_FaceToward(state, thing, enemyThing ? &enemyThing->position : &moveTarget, enemyThing != 0);
    sithBot_MoveToward(state, thing, &moveTarget, 0);
    sithBot_CheckStuck(state, thing, &moveTarget);
    sithBot_SyncPositionIfNeeded(state, thing);
    if (enemyThing)
    {
        usedForce = sithBot_TryUseForce(state, thing, enemyThing);
        if (!usedForce)
            sithBot_FireAt(state, thing, enemyThing);
    }
}

static void sithBot_ResetForWorldChange(void)
{
    memset(sithBot_bots, 0, sizeof(sithBot_bots));
    sithBot_numNodes = 0;
    sithBot_navBuilt = 0;
    sithBot_navWorld = 0;
    sithBot_spawnedForWorld = 0;
    sithBot_matchStartMs = 0;
    sithBot_scoreLogged = 0;
    sithBot_debugShotsLogged = 0;
    sithBot_debugHitsLogged = 0;
    sithBot_debugHuntsLogged = 0;
    sithBot_debugFireFailuresLogged = 0;
    sithBot_debugPickupsLogged = 0;
    sithBot_debugMovesLogged = 0;
    sithBot_debugProgressLogged = 0;
    sithBot_debugJumpsLogged = 0;
    sithBot_debugRouteNudgesLogged = 0;
    sithBot_debugCombatMovesLogged = 0;
    sithBot_debugDamageLogged = 0;
    sithBot_debugDeathsLogged = 0;
    sithBot_debugHazardsLogged = 0;
    sithBot_debugHazardMovesLogged = 0;
    sithBot_debugLedgeAvoidLogged = 0;
    sithBot_debugDynamicHazardsLogged = 0;
    sithBot_debugArmGoalsLogged = 0;
    sithBot_debugArmRejectsLogged = 0;
    sithBot_debugTacticalPickupsLogged = 0;
    sithBot_debugForceLogged = 0;
    sithBot_debugFallRecoveriesLogged = 0;
    sithBot_debugUsesLogged = 0;
    sithBot_debugLiftsLogged = 0;
    sithBot_debugJumpPadsLogged = 0;
    sithBot_debugTickSkipsLogged = 0;
    sithBot_qualityJumpDetected = 0;
    sithBot_qualityJumpLanded = 0;
    sithBot_qualityJumpRetry = 0;
    sithBot_qualityJumpFailed = 0;
    sithBot_qualityJumpTimeout = 0;
    sithBot_qualityRouteNudges = 0;
    sithBot_qualityRouteStalls = 0;
    sithBot_qualityNoLosFireAttempts = 0;
    memset(sithBot_qualityWeaponShots, 0, sizeof(sithBot_qualityWeaponShots));
    sithBot_cameraPlayer = 0;
    memset(sithBot_dynamicHazards, 0, sizeof(sithBot_dynamicHazards));
    memset(sithBot_blockedEdges, 0, sizeof(sithBot_blockedEdges));
    sithBot_numInferredLiftSectors = 0;
    memset(sithBot_inferredLiftSectors, 0, sizeof(sithBot_inferredLiftSectors));
}

void sithBot_TickAll(flex_t deltaSeconds, int deltaMs)
{
    int i;

    if (!sithWorld_pCurrentWorld || !sithNet_isMulti || !sithNet_isServer || Main_numBots <= 0)
    {
        if (sithBot_debugTickSkipsLogged < 8)
        {
            sithBot_Logf("BotMatch: tick-skip world=%p multi=%d server=%d bots=%d\n",
                         sithWorld_pCurrentWorld,
                         sithNet_isMulti,
                         sithNet_isServer,
                         Main_numBots);
            sithBot_debugTickSkipsLogged++;
        }
        if (sithBot_navWorld && sithBot_navWorld != sithWorld_pCurrentWorld)
            sithBot_ResetForWorldChange();
        return;
    }

    if (sithBot_navWorld != sithWorld_pCurrentWorld)
        sithBot_ResetForWorldChange();

    if (!sithBot_navBuilt)
        sithBot_PrepareNavigation();

    sithBot_EnsureBots();
    sithBot_UpdateCamera();
    for (i = 0; i < SITHBOT_MAX_BOTS; i++)
        sithBot_TickState(&sithBot_bots[i], deltaSeconds, deltaMs);

    if (Main_botMatchSeconds > 0 && sithBot_matchStartMs && !sithBot_scoreLogged &&
        sithTime_curMs - sithBot_matchStartMs >= (uint32_t)Main_botMatchSeconds * 1000u)
    {
        sithBot_scoreLogged = 1;
        sithBot_LogScoreboard("timed-final");
    }
}

void sithBot_PrepareNavigation(void)
{
    if (!sithWorld_pCurrentWorld || !sithNet_isMulti || !sithNet_isServer || Main_numBots <= 0)
        return;
    if (sithBot_navWorld != sithWorld_pCurrentWorld)
        sithBot_ResetForWorldChange();
    if (sithBot_navBuilt)
        return;

    sithWorld_UpdateLoadPercent(92.0);
    sithBot_BuildNav();
    sithWorld_UpdateLoadPercent(100.0);
}
