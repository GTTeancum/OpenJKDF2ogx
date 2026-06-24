#ifndef XBOX_SYSTEMLINK_PROBE_H
#define XBOX_SYSTEMLINK_PROBE_H

#include "types.h"

#ifdef __cplusplus
extern "C" {
#endif

#define XBOX_SYSTEMLINK_PROBE_MAX_PEERS 8
#define XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS 4
#define XBOX_SYSTEMLINK_PLAYER_STRIDE 4
#define XBOX_SYSTEMLINK_MAX_MACHINES 8

enum
{
    XBOX_SYSTEMLINK_ROLE_SEEKING = 0,
    XBOX_SYSTEMLINK_ROLE_HOST = 1,
    XBOX_SYSTEMLINK_ROLE_CLIENT = 2
};

enum
{
    XBOX_SYSTEMLINK_PHASE_DISCOVERY = 0,
    XBOX_SYSTEMLINK_PHASE_READY = 1,
    XBOX_SYSTEMLINK_PHASE_MAP_SELECT = 2,
    XBOX_SYSTEMLINK_PHASE_LAUNCHING = 3,
    XBOX_SYSTEMLINK_PHASE_IN_GAME = 4
};

typedef struct XboxSystemLinkProbePeer
{
    unsigned long id;
    unsigned long address;
    int port;
    int gamePort;
    int role;
    unsigned long hostId;
    int phase;
    int localPlayerCount;
    int readyMask;
    int confirmed;
    int machineIndex;
    int firstPlayerIndex;
    int hasSecureInfo;
    unsigned long launchId;
    unsigned long launchAckId;
    unsigned long lastSeenMs;
    unsigned long packets;
} XboxSystemLinkProbePeer;

typedef struct XboxSystemLinkProbeStatus
{
    int started;
    int socketsReady;
    int localPort;
    int localGamePort;
    unsigned long localId;
    int role;
    unsigned long hostId;
    int phase;
    int localPlayerCount;
    int readyMask;
    int confirmed;
    int groupMachineCount;
    int allConfirmed;
    int localFirstPlayerIndex;
    int rosterMaxPlayers;
    unsigned long launchId;
    unsigned long launchAckId;
    unsigned long sent;
    int lastError;
    int peerCount;
    XboxSystemLinkProbePeer peers[XBOX_SYSTEMLINK_PROBE_MAX_PEERS];
} XboxSystemLinkProbeStatus;

int xboxSystemLinkProbe_Start(void);
void xboxSystemLinkProbe_Stop(void);
void xboxSystemLinkProbe_Tick(void);
void xboxSystemLinkProbe_GetStatus(XboxSystemLinkProbeStatus *outStatus);
void xboxSystemLinkProbe_FormatAddress(unsigned long address, char *out, int outCount);

void xboxSystemLinkProbe_StopForTravel(void);
void xboxSystemLinkProbe_SetLocalReady(int localPlayerCount);
void xboxSystemLinkProbe_SetLocalConfirmed(int confirmed);
int xboxSystemLinkProbe_IsHost(void);
int xboxSystemLinkProbe_IsClient(void);
int xboxSystemLinkProbe_GroupMachineCount(void);
int xboxSystemLinkProbe_AllMachinesConfirmed(void);
int xboxSystemLinkProbe_SmokeHarnessBegin(const jkMultiEntry3 *entry, int isHost, int remoteLocalPlayerCount);
int xboxSystemLinkProbe_ScheduleHostLaunch(const jkMultiEntry3 *entry);
int xboxSystemLinkProbe_PollLaunch(jkMultiEntry3 *outEntry, int *outIsHost);
int xboxSystemLinkProbe_BeginGameplay(const jkMultiEntry3 *entry, int isHost);
int xboxSystemLinkProbe_IsGameplayActive(void);
int xboxSystemLinkProbe_IsGameplayHost(void);
int xboxSystemLinkProbe_IsSmokeHarness(void);
int xboxSystemLinkProbe_GetLocalFirstPlayerIndex(void);
int xboxSystemLinkProbe_GetLocalPlayerCount(void);
int xboxSystemLinkProbe_GetRosterMaxPlayers(void);
int xboxSystemLinkProbe_IsLocalPlayerIndex(int playerIndex);
int xboxSystemLinkProbe_NetIdForPlayerIndex(int playerIndex);
int xboxSystemLinkProbe_PlayerIndexForNetId(int netId);
void xboxSystemLinkProbe_ApplyRosterAfterStartup(void);

int xboxSystemLinkProbe_GameReceive(int *pIdOut, int *pMsgIdOut, int *pLenOut);
BOOL xboxSystemLinkProbe_GameSend(DPID idFrom, DPID idTo, void *lpData, DWORD dwDataSize);
DPID xboxSystemLinkProbe_GameCreatePlayer(wchar_t *pwName, int flags);
int xboxSystemLinkProbe_GameOpenHost(jkMultiEntry *entry);
int xboxSystemLinkProbe_GameOpenClient(int idx);
void xboxSystemLinkProbe_GameClose(void);
void xboxSystemLinkProbe_GameEnumPlayers(void);

#ifdef __cplusplus
}
#endif

#endif
