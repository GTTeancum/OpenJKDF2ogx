#include "AI/sithBot.h"

#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "Cog/sithCog.h"
#include "Dss/sithDSSThing.h"
#include "Dss/sithMulti.h"
#include "Engine/sithCollision.h"
#include "Engine/sithPhysics.h"
#include "Gameplay/sithInventory.h"
#include "Gameplay/sithPlayer.h"
#include "Gameplay/sithPlayerActions.h"
#include "Gameplay/sithTime.h"
#include "Main/Main.h"
#include "Primitives/rdMatrix.h"
#include "Primitives/rdVector.h"
#include "World/jkPlayer.h"
#include "World/sithActor.h"
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
#endif

#define SITHBOT_MAX_BOTS 31
#define SITHBOT_MAX_NODES 512
#define SITHBOT_MAX_EDGES 8
#define SITHBOT_NETID_BASE 0x42000000
#define SITHBOT_RESPAWN_MS 3000
#define SITHBOT_STUCK_MS 1400

typedef enum SithBotNodeKind
{
    SITHBOT_NODE_SPAWN,
    SITHBOT_NODE_ITEM,
    SITHBOT_NODE_FLOOR
} SithBotNodeKind;

typedef struct SithBotNode
{
    rdVector3 pos;
    sithSector *sector;
    int thingIdx;
    int kind;
    int edgeCount;
    int edges[SITHBOT_MAX_EDGES];
} SithBotNode;

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
    uint32_t respawnAtMs;
    uint32_t lastMoveCheckMs;
    rdVector3 lastMovePos;
    int stuckTicks;
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

static int sithBot_IsBotNetId(int netId)
{
    return netId >= SITHBOT_NETID_BASE && netId < SITHBOT_NETID_BASE + 64;
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
    xbox_debug_PerfPrintf("%s", buf);
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
}

static flex_t sithBot_DistSq(const rdVector3 *a, const rdVector3 *b)
{
    flex_t dx = a->x - b->x;
    flex_t dy = a->y - b->y;
    flex_t dz = a->z - b->z;
    return dx * dx + dy * dy + dz * dz;
}

static int sithBot_AddNode(const rdVector3 *pos, sithSector *sector, int kind, int thingIdx, flex_t minDist)
{
    int i;

    if (!pos || !sector || sithBot_numNodes >= SITHBOT_MAX_NODES)
        return -1;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        if (sithBot_nodes[i].sector == sector && sithBot_DistSq(&sithBot_nodes[i].pos, pos) < minDist * minDist)
            return i;
    }

    rdVector_Copy3(&sithBot_nodes[sithBot_numNodes].pos, pos);
    sithBot_nodes[sithBot_numNodes].sector = sector;
    sithBot_nodes[sithBot_numNodes].kind = kind;
    sithBot_nodes[sithBot_numNodes].thingIdx = thingIdx;
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

static void sithBot_AddEdge(int a, int b)
{
    SithBotNode *node;
    int i;

    if (a < 0 || b < 0 || a == b || a >= sithBot_numNodes || b >= sithBot_numNodes)
        return;

    node = &sithBot_nodes[a];
    for (i = 0; i < node->edgeCount; i++)
    {
        if (node->edges[i] == b)
            return;
    }

    if (node->edgeCount < SITHBOT_MAX_EDGES)
        node->edges[node->edgeCount++] = b;
}

static void sithBot_LinkNodes(void)
{
    int i;
    int j;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        for (j = i + 1; j < sithBot_numNodes; j++)
        {
            flex_t distSq = sithBot_DistSq(&sithBot_nodes[i].pos, &sithBot_nodes[j].pos);
            if (distSq > 100.0)
                continue;
            if (!sithBot_CanSeePosition(sithBot_nodes[i].sector, &sithBot_nodes[i].pos, sithBot_nodes[j].sector, &sithBot_nodes[j].pos))
                continue;
            sithBot_AddEdge(i, j);
            sithBot_AddEdge(j, i);
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
    if (!(surface->surfaceFlags & (SITH_SURFACE_FLOOR | SITH_SURFACE_AI_CAN_WALK_ON_FLOOR)))
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

static void sithBot_BuildNav(void)
{
    int i;
    int j;
    int edgeCount = 0;

    sithBot_numNodes = 0;
    if (!sithWorld_pCurrentWorld)
        return;

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        if (jkPlayer_playerInfos[i].pSpawnSector)
            sithBot_AddNode(&jkPlayer_playerInfos[i].spawnPosOrient.scale, jkPlayer_playerInfos[i].pSpawnSector, SITHBOT_NODE_SPAWN, i, 1.0);
    }

    for (i = 0; i < sithWorld_pCurrentWorld->numThingsLoaded; i++)
    {
        sithThing *thing = &sithWorld_pCurrentWorld->things[i];
        if (thing->type == SITH_THING_ITEM && thing->sector)
            sithBot_AddNode(&thing->position, thing->sector, SITHBOT_NODE_ITEM, thing->thingIdx, 1.25);
    }

    for (i = 0; i < sithWorld_pCurrentWorld->numSectors; i++)
    {
        sithSector *sector = &sithWorld_pCurrentWorld->sectors[i];
        for (j = 0; j < (int)sector->numSurfaces; j++)
            sithBot_AddSurfaceNode(&sector->surfaces[j]);
    }

    sithBot_LinkNodes();
    for (i = 0; i < sithBot_numNodes; i++)
        edgeCount += sithBot_nodes[i].edgeCount;

    sithBot_navWorld = sithWorld_pCurrentWorld;
    sithBot_navBuilt = 1;
    sithBot_Logf("BotNav: generated nodes=%d directedEdges=%d map='%s' episode='%s'\n",
                 sithBot_numNodes,
                 edgeCount,
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
        if (!sithBot_CanSeePosition(sector, pos, sithBot_nodes[i].sector, &sithBot_nodes[i].pos))
            continue;
        distSq = sithBot_DistSq(pos, &sithBot_nodes[i].pos);
        if (distSq < bestDist)
        {
            bestDist = distSq;
            best = i;
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

static int sithBot_FindPathNext(int startNode, int goalNode)
{
    int queue[SITHBOT_MAX_NODES];
    int prev[SITHBOT_MAX_NODES];
    int head = 0;
    int tail = 0;
    int i;
    int node;

    if (startNode < 0 || goalNode < 0 || startNode >= sithBot_numNodes || goalNode >= sithBot_numNodes)
        return -1;
    if (startNode == goalNode)
        return goalNode;

    for (i = 0; i < sithBot_numNodes; i++)
        prev[i] = -2;

    queue[tail++] = startNode;
    prev[startNode] = -1;
    while (head < tail)
    {
        node = queue[head++];
        if (node == goalNode)
            break;
        for (i = 0; i < sithBot_nodes[node].edgeCount; i++)
        {
            int next = sithBot_nodes[node].edges[i];
            if (prev[next] != -2)
                continue;
            prev[next] = node;
            queue[tail++] = next;
            if (next == goalNode)
                break;
        }
    }

    if (prev[goalNode] == -2)
        return -1;

    node = goalNode;
    while (prev[node] != -1 && prev[node] != startNode)
        node = prev[node];
    return node;
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

static int sithBot_FindEnemy(int botIdx, sithThing *botThing)
{
    int i;
    int best = -1;
    flex_t bestDist = 3.4e38f;

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        sithThing *candidate;
        flex_t distSq;

        if (i == botIdx)
            continue;
        candidate = jkPlayer_playerInfos[i].playerThing;
        if (!sithBot_IsThingAlivePlayer(candidate))
            continue;
        if ((sithNet_MultiModeFlags & MULTIMODEFLAG_TEAMS) && sithPlayer_sub_4C9060(botThing, candidate))
            continue;

        distSq = sithBot_DistSq(&botThing->position, &candidate->position);
        if (!sithCollision_HasLos(botThing, candidate, 0) &&
            (distSq > 36.0 || !sithBot_CanSeePosition(botThing->sector, &botThing->position, candidate->sector, &candidate->position)))
            continue;
        if (distSq < bestDist)
        {
            bestDist = distSq;
            best = i;
        }
    }
    return best;
}

static int sithBot_FindHuntEnemy(int botIdx, sithThing *botThing)
{
    int i;
    int best = -1;
    flex_t bestDist = 3.4e38f;

    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        sithThing *candidate;
        flex_t distSq;

        if (i == botIdx)
            continue;
        candidate = jkPlayer_playerInfos[i].playerThing;
        if (!sithBot_IsThingAlivePlayer(candidate))
            continue;
        if ((sithNet_MultiModeFlags & MULTIMODEFLAG_TEAMS) && sithPlayer_sub_4C9060(botThing, candidate))
            continue;

        distSq = sithBot_DistSq(&botThing->position, &candidate->position);
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

    if (!thing || bin <= 0 || bin >= SITHBIN_NUMBINS)
        return 0;
    desc = sithInventory_GetBinByIdx(bin);
    return desc
        && (desc->flags & ITEMINFO_WEAPON)
        && sithInventory_GetAvailable(thing, bin)
        && sithInventory_GetBinAmount(thing, bin) > 0.0;
}

static int sithBot_ChooseWeapon(sithThing *thing, flex_t enemyDist)
{
    int jkPriority[] = {
        SITHBIN_CONCUSSION_RIFLE,
        SITHBIN_RAIL_DETONATOR,
        SITHBIN_REPEATER,
        SITHBIN_STORMTROOPER_RIFLE,
        SITHBIN_BRYARPISTOL,
        SITHBIN_LIGHTSABER,
        SITHBIN_FISTS
    };
    int motsPriority[] = {
        SITHBIN_MOTS_CONCUSSION_RIFLE,
        SITHBIN_MOTS_RAIL_DETONATOR,
        SITHBIN_MOTS_REPEATER,
        SITHBIN_MOTS_STORMTROOPER_RIFLE,
        SITHBIN_MOTS_BRYARPISTOL,
        SITHBIN_MOTS_LIGHTSABER,
        SITHBIN_MOTS_FISTS
    };
    int *priority = Main_bMotsCompat ? motsPriority : jkPriority;
    int count = Main_bMotsCompat ? (int)(sizeof(motsPriority) / sizeof(motsPriority[0])) : (int)(sizeof(jkPriority) / sizeof(jkPriority[0]));
    int i;

    if (enemyDist < 1.8)
    {
        int saber = Main_bMotsCompat ? SITHBIN_MOTS_LIGHTSABER : SITHBIN_LIGHTSABER;
        if (sithBot_WeaponAvailable(thing, saber))
            return saber;
    }

    for (i = 0; i < count; i++)
    {
        if (sithBot_WeaponAvailable(thing, priority[i]))
            return priority[i];
    }
    return Main_bMotsCompat ? SITHBIN_MOTS_FISTS : SITHBIN_FISTS;
}

static int sithBot_IsSaberBin(int weaponBin)
{
    if (Main_bMotsCompat)
        return weaponBin == SITHBIN_MOTS_LIGHTSABER;
    return weaponBin == SITHBIN_LIGHTSABER;
}

static void sithBot_GiveBin(sithThing *thing, int bin, flex_t amount)
{
    sithInventory_SetCarries(thing, bin, 1);
    sithInventory_SetAvailable(thing, bin, 1);
    sithInventory_SetBinAmount(thing, bin, amount);
}

static void sithBot_GiveLoadout(sithThing *thing)
{
    sithInventory_ClearInventory(thing);

    if (Main_bMotsCompat)
    {
        sithBot_GiveBin(thing, SITHBIN_MOTS_FISTS, 1.0);
        sithBot_GiveBin(thing, SITHBIN_MOTS_BRYARPISTOL, 1.0);
        sithBot_GiveBin(thing, SITHBIN_MOTS_LIGHTSABER, 1.0);
        sithBot_GiveBin(thing, SITHBIN_ENERGY, 200.0);
    }
    else
    {
        sithBot_GiveBin(thing, SITHBIN_FISTS, 1.0);
        sithBot_GiveBin(thing, SITHBIN_BRYARPISTOL, 1.0);
        sithBot_GiveBin(thing, SITHBIN_LIGHTSABER, 1.0);
        sithBot_GiveBin(thing, SITHBIN_ENERGY, 200.0);
    }

    sithBot_GiveBin(thing, SITHBIN_SHIELDS, 100.0);
    sithBot_GiveBin(thing, SITHBIN_FORCEMANA, 100.0);
    if (!thing->actorParams.templateWeapon)
        thing->actorParams.templateWeapon = sithTemplate_GetEntryByName("+bryarbolt");
    sithInventory_SetCurWeapon(thing, Main_bMotsCompat ? SITHBIN_MOTS_BRYARPISTOL : SITHBIN_BRYARPISTOL);
    sithInventory_SetCurItem(thing, 0);
    sithInventory_SetCurPower(thing, 0);
}

static void sithBot_Respawn(int botIdx)
{
    int spawnIdx;
    sithThing *thing;
    sithThing *tmpl;

    if (botIdx < 0 || botIdx >= jkPlayer_maxPlayers)
        return;

    thing = jkPlayer_playerInfos[botIdx].playerThing;
    if (!thing || !jkPlayer_playerInfos[botIdx].pSpawnSector)
        return;

    tmpl = thing->templateBase ? thing->templateBase : thing;
    thing->actorParams.msUnderwater = 0;
    thing->actorParams.health = tmpl->actorParams.health;
    if (thing->actorParams.health <= 0.0)
        thing->actorParams.health = 100.0;

    thing->thingflags &= ~(SITH_TF_DISABLED | SITH_TF_INVULN | SITH_TF_DEAD | SITH_TF_WILLBEREMOVED);
    thing->actorParams.typeflags &= ~(SITH_AF_FALLING_TO_DEATH | SITH_AF_INVULNERABLE | SITH_AF_DISABLED);
    thing->lifeLeftMs = 0;
    sithActor_MoveJointsForEyePYR(thing, &rdroid_zeroVector3);

    spawnIdx = sithMulti_GetSpawnIdx(thing);
    if (spawnIdx < 0 || spawnIdx >= jkPlayer_maxPlayers || !jkPlayer_playerInfos[spawnIdx].pSpawnSector)
        spawnIdx = botIdx;

    sithThing_LeaveSector(thing);
    sithThing_SetPosAndRot(thing, &jkPlayer_playerInfos[spawnIdx].spawnPosOrient.scale, &jkPlayer_playerInfos[spawnIdx].spawnPosOrient);
    sithThing_EnterSector(thing, jkPlayer_playerInfos[spawnIdx].pSpawnSector, 1, 0);
    sithPhysics_ThingStop(thing);
    sithPhysics_FindFloor(thing, 1);
    sithWeapon_SyncPuppet(thing);
    sithBot_GiveLoadout(thing);
    sithCog_SendSimpleMessageToAll(SITH_MESSAGE_NEWPLAYER, SENDERTYPE_THING, thing->thingIdx, SENDERTYPE_THING, thing->thingIdx);
    if (sithComm_multiplayerFlags)
        sithDSSThing_SendSyncThing(thing, -1, 255);
}

static void sithBot_ResetState(SithBotState *bot, int playerIdx)
{
    memset(bot, 0, sizeof(*bot));
    bot->active = 1;
    bot->playerIdx = playerIdx;
    bot->goalNode = -1;
    bot->nextNode = -1;
    bot->enemyIdx = -1;
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
        L"JA-UT Bot 1",
        L"JA-UT Bot 2",
        L"JA-UT Bot 3",
        L"JA-UT Bot 4",
        L"JA-UT Bot 5",
        L"JA-UT Bot 6",
        L"JA-UT Bot 7",
        L"JA-UT Bot 8"
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
    thing->playerInfo = &playerThings[playerIdx];
    playerThings[playerIdx].actorThing = thing;
    thing->thingflags |= SITH_TF_RENDERWEAPON;
    thing->thingflags &= ~(SITH_TF_DISABLED | SITH_TF_INVULN | SITH_TF_DEAD | SITH_TF_WILLBEREMOVED);
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
    int i;

    desired = Main_numBots;
    if (desired < 0)
        desired = 0;
    if (desired > SITHBOT_MAX_BOTS)
        desired = SITHBOT_MAX_BOTS;
    if (desired > jkPlayer_maxPlayers - 1)
        desired = jkPlayer_maxPlayers - 1;

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

static int sithBot_ChooseGoalNode(sithThing *thing)
{
    int i;
    int best = -1;
    flex_t bestScore = -3.4e38f;

    for (i = 0; i < sithBot_numNodes; i++)
    {
        flex_t distSq = sithBot_DistSq(&thing->position, &sithBot_nodes[i].pos);
        flex_t score = _frand() * 4.0;
        if (sithBot_nodes[i].kind == SITHBOT_NODE_ITEM)
        {
            sithThing *item = sithThing_GetThingByIdx(sithBot_nodes[i].thingIdx);
            if (item && !(item->thingflags & SITH_TF_DISABLED) && item->itemParams.respawnTime <= sithTime_curMs)
                score += 18.0;
            else
                score -= 8.0;
        }
        else if (sithBot_nodes[i].kind == SITHBOT_NODE_SPAWN)
        {
            score += 4.0;
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

static void sithBot_FaceToward(sithThing *thing, const rdVector3 *target)
{
    rdVector3 dir;

    rdVector_Sub3(&dir, target, &thing->position);
    if (rdVector_Normalize3Acc(&dir) <= 0.001)
        return;

    rdMatrix_BuildFromLook34(&thing->lookOrientation, &dir);
    rdVector_Zero3(&thing->lookOrientation.scale);
}

static void sithBot_MoveToward(SithBotState *state, sithThing *thing, const rdVector3 *target, int combat)
{
    rdVector3 flat;
    flex_t dist;
    flex_t thrust;

    rdVector_Sub3(&flat, target, &thing->position);
    flat.z = 0.0;
    dist = rdVector_Normalize3Acc(&flat);
    if (dist <= 0.001)
    {
        rdVector_Zero3(&thing->physicsParams.acceleration);
        return;
    }

    thrust = thing->actorParams.maxThrust + thing->actorParams.extraSpeed;
    if (thrust <= 0.0)
        thrust = 1.0;

    thing->physicsParams.acceleration.x = combat ? (_frand() - 0.5) * thrust * 0.55 : 0.0;
    thing->physicsParams.acceleration.y = thrust * (dist < 0.8 ? 0.35 : 0.9);
    thing->physicsParams.acceleration.z = 0.0;

    if (target->z - thing->position.z > 0.25 && state->nextUseMs <= sithTime_curMs)
    {
        sithPlayerActions_JumpWithVel(thing, 1.0);
        state->nextUseMs = sithTime_curMs + 700;
    }
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

    if (sithBot_DistSq(&state->lastMovePos, &thing->position) < 0.06 && sithBot_DistSq(&thing->position, target) > 1.0)
    {
        state->stuckTicks++;
        if (state->nextUseMs <= sithTime_curMs)
        {
            sithBot_FaceToward(thing, target);
            sithPlayerActions_Activate(thing);
            sithPlayerActions_JumpWithVel(thing, 1.0);
            state->nextUseMs = sithTime_curMs + 900;
        }
        if (state->stuckTicks >= 3)
        {
            state->goalNode = -1;
            state->nextNode = -1;
            state->nextGoalMs = 0;
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

static void sithBot_FireAt(SithBotState *state, sithThing *thing, sithThing *enemy)
{
    int weaponBin;
    sithItemDescriptor *desc;
    sithThing *projectile;
    sithThing *spawned;
    rdVector3 aim;
    rdVector3 dir;
    flex_t dist;

    if (sithTime_curMs < state->nextFireMs)
        return;

    rdVector_Copy3(&aim, &enemy->position);
    aim.z += 0.08;
    rdVector_Sub3(&dir, &aim, &thing->position);
    dist = rdVector_Normalize3Acc(&dir);
    if (dist <= 0.001)
        return;

    weaponBin = sithBot_ChooseWeapon(thing, dist);
    if (weaponBin != sithInventory_GetCurWeapon(thing))
        sithInventory_SetCurWeapon(thing, weaponBin);

    sithBot_FaceToward(thing, &aim);

    if (sithBot_IsSaberBin(weaponBin) && dist < 1.8)
    {
        flex_t beforeHealth = enemy->actorParams.health;

        sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_FIRE1);
        sithThing_Damage(enemy, thing, 35.0, SITH_DAMAGE_SABER);
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

    projectile = thing->actorParams.templateWeapon;
    if (!projectile)
        projectile = sithTemplate_GetEntryByName("+bryarbolt");
    if (projectile)
    {
        sithSoundClass_ThingPlaySoundclass4(thing, SITH_SC_FIRE1);
        spawned = sithWeapon_Fire(thing, projectile, &dir, &aim, 0, SITH_ANIM_FIRE, 1.0, 0, 0.0);
        if (spawned)
        {
            sithCog_SendMessageFromThing(thing, spawned, SITH_MESSAGE_FIRE);
            if (sithBot_debugShotsLogged < 40)
            {
                sithBot_Logf("BotMatch: fired slot=%d target=%d weapon=%d projectileThing=%d dist=%.2f\n",
                             state->playerIdx,
                             enemy->actorParams.playerinfo ? (int)(enemy->actorParams.playerinfo - jkPlayer_playerInfos) : -1,
                             weaponBin,
                             spawned->thingIdx,
                             dist);
                sithBot_debugShotsLogged++;
            }
        }
        else if (sithBot_debugFireFailuresLogged < 16)
        {
            sithBot_Logf("BotMatch: fire-missed slot=%d weapon=%d projectileTemplate='%s' dist=%.2f\n",
                         state->playerIdx,
                         weaponBin,
                         projectile->template_name,
                         dist);
            sithBot_debugFireFailuresLogged++;
        }
    }
    else
    {
        desc = sithInventory_GetBinByIdx(weaponBin);
        if (desc && desc->cog)
        {
            sithCog_SendMessageEx(desc->cog, SITH_MESSAGE_FIRE, SENDERTYPE_SYSTEM, weaponBin, SENDERTYPE_THING, thing->thingIdx, 0, 0.0, 0.0, 0.0, 0.0);
            if (sithBot_debugFireFailuresLogged < 16)
            {
                sithBot_Logf("BotMatch: fire-cog-fallback slot=%d weapon=%d dist=%.2f\n", state->playerIdx, weaponBin, dist);
                sithBot_debugFireFailuresLogged++;
            }
        }
        else if (sithBot_debugFireFailuresLogged < 16)
        {
            sithBot_Logf("BotMatch: fire-no-projectile slot=%d weapon=%d desc=%p cog=%p dist=%.2f\n",
                         state->playerIdx,
                         weaponBin,
                         (void *)desc,
                         desc ? (void *)desc->cog : 0,
                         dist);
            sithBot_debugFireFailuresLogged++;
        }
    }
    state->nextFireMs = sithTime_curMs + (uint32_t)(420.0 + _frand() * 360.0);
}

static void sithBot_TickState(SithBotState *state, flex_t deltaSeconds, int deltaMs)
{
    sithPlayerInfo *info;
    sithThing *thing;
    rdVector3 moveTarget;
    sithThing *enemyThing = 0;
    sithThing *huntThing = 0;
    int startNode;
    int nextNode;
    int huntIdx;

    (void)deltaSeconds;
    (void)deltaMs;

    if (!state->active || state->playerIdx < 0 || state->playerIdx >= jkPlayer_maxPlayers)
        return;

    info = &jkPlayer_playerInfos[state->playerIdx];
    thing = info->playerThing;
    if (!thing || !sithBot_IsBotNetId(info->net_id))
    {
        state->active = 0;
        return;
    }

    info->lastUpdateMs = sithTime_curMs;

    if ((thing->thingflags & SITH_TF_DEAD) || thing->actorParams.health <= 0.0)
    {
        rdVector_Zero3(&thing->physicsParams.acceleration);
        if (!state->respawnAtMs)
            state->respawnAtMs = sithTime_curMs + SITHBOT_RESPAWN_MS;
        if (sithTime_curMs >= state->respawnAtMs)
        {
            sithBot_Respawn(state->playerIdx);
            state->respawnAtMs = 0;
            state->goalNode = -1;
            state->nextNode = -1;
            state->nextGoalMs = 0;
        }
        return;
    }

    state->enemyIdx = sithBot_FindEnemy(state->playerIdx, thing);
    if (state->enemyIdx >= 0)
        enemyThing = jkPlayer_playerInfos[state->enemyIdx].playerThing;

    if (enemyThing)
    {
        rdVector_Copy3(&moveTarget, &enemyThing->position);
        sithBot_FaceToward(thing, &moveTarget);
        if (sithBot_DistSq(&thing->position, &moveTarget) > 1.9)
            sithBot_MoveToward(state, thing, &moveTarget, 1);
        else
        {
            rdVector_Zero3(&thing->physicsParams.acceleration);
            thing->physicsParams.acceleration.x = (_frand() - 0.5) * (thing->actorParams.maxThrust + thing->actorParams.extraSpeed) * 0.5;
        }
        sithBot_FireAt(state, thing, enemyThing);
        return;
    }

    if (sithBot_numNodes <= 0)
        return;

    huntIdx = sithBot_FindHuntEnemy(state->playerIdx, thing);
    if (huntIdx >= 0)
        huntThing = jkPlayer_playerInfos[huntIdx].playerThing;
    if (huntThing && (state->goalNode < 0 || sithTime_curMs >= state->nextGoalMs))
    {
        int huntNode = sithBot_FindNearestNode(huntThing);
        if (huntNode >= 0)
        {
            state->goalNode = huntNode;
            state->nextGoalMs = sithTime_curMs + (uint32_t)(1800.0 + _frand() * 1200.0);
            state->nextNode = -1;
            if (sithBot_debugHuntsLogged < 24)
            {
                sithBot_Logf("BotMatch: hunt slot=%d target=%d goalNode=%d\n", state->playerIdx, huntIdx, huntNode);
                sithBot_debugHuntsLogged++;
            }
        }
    }

    if (state->goalNode < 0 || state->goalNode >= sithBot_numNodes || sithTime_curMs >= state->nextGoalMs ||
        sithBot_DistSq(&thing->position, &sithBot_nodes[state->goalNode].pos) < 0.8)
    {
        state->goalNode = sithBot_ChooseGoalNode(thing);
        state->nextGoalMs = sithTime_curMs + (uint32_t)(5000.0 + _frand() * 5000.0);
        state->nextNode = -1;
    }

    startNode = sithBot_FindNearestNode(thing);
    nextNode = sithBot_FindPathNext(startNode, state->goalNode);
    if (nextNode < 0)
        nextNode = state->goalNode;
    state->nextNode = nextNode;

    if (nextNode >= 0 && nextNode < sithBot_numNodes)
        rdVector_Copy3(&moveTarget, &sithBot_nodes[nextNode].pos);
    else
        rdVector_Copy3(&moveTarget, &thing->position);

    sithBot_FaceToward(thing, &moveTarget);
    sithBot_MoveToward(state, thing, &moveTarget, 0);
    sithBot_CheckStuck(state, thing, &moveTarget);

    if (state->nextSyncMs <= sithTime_curMs)
    {
        if (sithComm_multiplayerFlags)
            sithDSSThing_SendPos(thing, -1, 0);
        state->nextSyncMs = sithTime_curMs + 500;
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
}

void sithBot_TickAll(flex_t deltaSeconds, int deltaMs)
{
    int i;

    if (!sithWorld_pCurrentWorld || !sithNet_isMulti || !sithNet_isServer || Main_numBots <= 0)
    {
        if (sithBot_navWorld && sithBot_navWorld != sithWorld_pCurrentWorld)
            sithBot_ResetForWorldChange();
        return;
    }

    if (sithBot_navWorld != sithWorld_pCurrentWorld)
        sithBot_ResetForWorldChange();

    if (!sithBot_navBuilt)
        sithBot_BuildNav();

    sithBot_EnsureBots();
    for (i = 0; i < SITHBOT_MAX_BOTS; i++)
        sithBot_TickState(&sithBot_bots[i], deltaSeconds, deltaMs);

    if (Main_botMatchSeconds > 0 && sithBot_matchStartMs && !sithBot_scoreLogged &&
        sithTime_curMs - sithBot_matchStartMs >= (uint32_t)Main_botMatchSeconds * 1000u)
    {
        sithBot_scoreLogged = 1;
        sithBot_LogScoreboard("timed-final");
    }
}
