#ifndef XBOX_SYSTEMLINK_PROBE_H
#define XBOX_SYSTEMLINK_PROBE_H

#ifdef __cplusplus
extern "C" {
#endif

#define XBOX_SYSTEMLINK_PROBE_MAX_PEERS 8

typedef struct XboxSystemLinkProbePeer
{
    unsigned long id;
    unsigned long address;
    int port;
    unsigned long lastSeenMs;
    unsigned long packets;
} XboxSystemLinkProbePeer;

typedef struct XboxSystemLinkProbeStatus
{
    int started;
    int socketsReady;
    int localPort;
    unsigned long localId;
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

#ifdef __cplusplus
}
#endif

#endif
