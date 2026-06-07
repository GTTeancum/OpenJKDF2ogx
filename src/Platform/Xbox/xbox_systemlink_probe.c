#include "Platform/Xbox/xbox_systemlink_probe.h"

#include <string.h>

#ifdef TARGET_XBOX
#include "Platform/Xbox/xbox_debug.h"
#include <stdio.h>
#include <WinSockX.h>

#define XSL_BASE_PORT 9777
#define XSL_PORT_COUNT 4
#define XSL_PACKET_MAGIC "JKXSL1"

typedef struct XboxSystemLinkProbeState
{
    int initialized;
    int started;
    int socketsReady;
    SOCKET socketHandle;
    int localPort;
    unsigned long localId;
    unsigned long sendCounter;
    unsigned long lastSendMs;
    unsigned long lastLogMs;
    int lastError;
    int peerCount;
    XboxSystemLinkProbePeer peers[XBOX_SYSTEMLINK_PROBE_MAX_PEERS];
} XboxSystemLinkProbeState;

static XboxSystemLinkProbeState g_xslProbe;

static void xboxSystemLinkProbe_EnsureState(void)
{
    if (g_xslProbe.initialized)
        return;

    memset(&g_xslProbe, 0, sizeof(g_xslProbe));
    g_xslProbe.socketHandle = INVALID_SOCKET;
    g_xslProbe.initialized = 1;
}

void xboxSystemLinkProbe_FormatAddress(unsigned long address, char *out, int outCount)
{
    unsigned char *b;

    if (!out || outCount <= 0)
        return;

    b = (unsigned char *)&address;
    _snprintf(out, outCount, "%u.%u.%u.%u", b[0], b[1], b[2], b[3]);
    out[outCount - 1] = 0;
}

static int xboxSystemLinkProbe_InitSockets(void)
{
    XNetStartupParams params;
    WSADATA wsaData;
    int xnetResult;
    int wsaResult;

    xboxSystemLinkProbe_EnsureState();
    if (g_xslProbe.socketsReady)
        return 1;

    memset(&params, 0, sizeof(params));
    params.cfgSizeOfStruct = sizeof(params);
    params.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
    params.cfgPrivatePoolSizeInPages = 12;
    params.cfgSockMaxSockets = 16;
    params.cfgSockDefaultRecvBufsizeInK = 16;
    params.cfgSockDefaultSendBufsizeInK = 16;

    xnetResult = XNetStartup(&params);
    memset(&wsaData, 0, sizeof(wsaData));
    wsaResult = WSAStartup(MAKEWORD(2, 2), &wsaData);

    g_xslProbe.socketsReady = (xnetResult == 0 && wsaResult == 0);
    g_xslProbe.lastError = g_xslProbe.socketsReady ? 0 : (wsaResult ? wsaResult : xnetResult);
    xbox_debug_Printf("XSL probe net init xnet=%d wsa=%d ready=%d\n",
                      xnetResult,
                      wsaResult,
                      g_xslProbe.socketsReady);
    return g_xslProbe.socketsReady;
}

void xboxSystemLinkProbe_Stop(void)
{
    xboxSystemLinkProbe_EnsureState();

    if (g_xslProbe.socketHandle != INVALID_SOCKET)
    {
        closesocket(g_xslProbe.socketHandle);
        g_xslProbe.socketHandle = INVALID_SOCKET;
    }

    g_xslProbe.started = 0;
    g_xslProbe.localPort = 0;
    xbox_debug_Print("XSL probe stopped\n");
}

int xboxSystemLinkProbe_Start(void)
{
    BOOL yes;
    u_long noBlock;
    int i;
    int bound;

    xboxSystemLinkProbe_EnsureState();
    if (g_xslProbe.started)
        return 1;

    if (!xboxSystemLinkProbe_InitSockets())
        return 0;

    g_xslProbe.socketHandle = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (g_xslProbe.socketHandle == INVALID_SOCKET)
    {
        g_xslProbe.lastError = WSAGetLastError();
        xbox_debug_Printf("XSL probe socket failed err=%d\n", g_xslProbe.lastError);
        return 0;
    }

    yes = TRUE;
    setsockopt(g_xslProbe.socketHandle, SOL_SOCKET, SO_BROADCAST, (char *)&yes, sizeof(yes));
    setsockopt(g_xslProbe.socketHandle, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));

    noBlock = 1;
    ioctlsocket(g_xslProbe.socketHandle, FIONBIO, &noBlock);

    bound = 0;
    for (i = 0; i < XSL_PORT_COUNT; i++)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((u_short)(XSL_BASE_PORT + i));

        if (bind(g_xslProbe.socketHandle, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            g_xslProbe.localPort = XSL_BASE_PORT + i;
            bound = 1;
            break;
        }
    }

    if (!bound)
    {
        g_xslProbe.lastError = WSAGetLastError();
        xbox_debug_Printf("XSL probe bind failed err=%d\n", g_xslProbe.lastError);
        xboxSystemLinkProbe_Stop();
        return 0;
    }

    if (!g_xslProbe.localId)
        g_xslProbe.localId = GetTickCount() ^ (unsigned long)&g_xslProbe;

    g_xslProbe.started = 1;
    g_xslProbe.lastSendMs = 0;
    g_xslProbe.lastLogMs = 0;
    g_xslProbe.peerCount = 0;
    g_xslProbe.sendCounter = 0;
    xbox_debug_Printf("XSL probe started id=0x%08X port=%d\n",
                      g_xslProbe.localId,
                      g_xslProbe.localPort);
    return 1;
}

static void xboxSystemLinkProbe_Send(void)
{
    char packet[96];
    int packetLen;
    int i;

    if (!g_xslProbe.started || g_xslProbe.socketHandle == INVALID_SOCKET)
        return;

    _snprintf(packet, sizeof(packet), XSL_PACKET_MAGIC "|%08X|%d|%lu",
              g_xslProbe.localId,
              g_xslProbe.localPort,
              g_xslProbe.sendCounter++);
    packet[sizeof(packet) - 1] = 0;
    packetLen = (int)strlen(packet);

    for (i = 0; i < XSL_PORT_COUNT; i++)
    {
        struct sockaddr_in to;
        int sent;

        memset(&to, 0, sizeof(to));
        to.sin_family = AF_INET;
        to.sin_addr.s_addr = INADDR_BROADCAST;
        to.sin_port = htons((u_short)(XSL_BASE_PORT + i));

        sent = sendto(g_xslProbe.socketHandle, packet, packetLen, 0, (struct sockaddr *)&to, sizeof(to));
        if (sent == SOCKET_ERROR)
            g_xslProbe.lastError = WSAGetLastError();
    }
}

static void xboxSystemLinkProbe_RecordPeer(unsigned long id, unsigned long address, int port, unsigned long nowMs)
{
    int i;
    XboxSystemLinkProbePeer *peer;
    char addressText[32];

    if (id == g_xslProbe.localId)
        return;

    for (i = 0; i < g_xslProbe.peerCount; i++)
    {
        peer = &g_xslProbe.peers[i];
        if (peer->id == id)
        {
            peer->address = address;
            peer->port = port;
            peer->lastSeenMs = nowMs;
            peer->packets++;
            return;
        }
    }

    if (g_xslProbe.peerCount >= XBOX_SYSTEMLINK_PROBE_MAX_PEERS)
    {
        memmove(&g_xslProbe.peers[0],
                &g_xslProbe.peers[1],
                sizeof(g_xslProbe.peers[0]) * (XBOX_SYSTEMLINK_PROBE_MAX_PEERS - 1));
        g_xslProbe.peerCount = XBOX_SYSTEMLINK_PROBE_MAX_PEERS - 1;
    }

    peer = &g_xslProbe.peers[g_xslProbe.peerCount++];
    peer->id = id;
    peer->address = address;
    peer->port = port;
    peer->lastSeenMs = nowMs;
    peer->packets = 1;

    xboxSystemLinkProbe_FormatAddress(address, addressText, sizeof(addressText));
    xbox_debug_Printf("XSL peer discovered id=0x%08X addr=%s port=%d\n",
                      id,
                      addressText,
                      port);
}

void xboxSystemLinkProbe_Tick(void)
{
    unsigned long nowMs;

    xboxSystemLinkProbe_EnsureState();

    if (!g_xslProbe.started)
        xboxSystemLinkProbe_Start();
    if (!g_xslProbe.started)
        return;

    nowMs = GetTickCount();
    if (nowMs - g_xslProbe.lastSendMs >= 1000)
    {
        xboxSystemLinkProbe_Send();
        g_xslProbe.lastSendMs = nowMs;
    }

    for (;;)
    {
        char buffer[128];
        struct sockaddr_in from;
        int fromSize;
        int count;
        unsigned long id;
        int port;
        unsigned long counter;

        fromSize = sizeof(from);
        count = recvfrom(g_xslProbe.socketHandle, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&from, &fromSize);
        if (count == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)
                g_xslProbe.lastError = err;
            break;
        }

        buffer[count] = 0;
        id = 0;
        port = 0;
        counter = 0;
        if (sscanf(buffer, XSL_PACKET_MAGIC "|%08X|%d|%lu", &id, &port, &counter) == 3)
            xboxSystemLinkProbe_RecordPeer(id, from.sin_addr.s_addr, port, nowMs);
    }

    if (nowMs - g_xslProbe.lastLogMs >= 5000)
    {
        g_xslProbe.lastLogMs = nowMs;
        xbox_debug_Printf("XSL probe status id=0x%08X port=%d peers=%d sent=%lu lastErr=%d\n",
                          g_xslProbe.localId,
                          g_xslProbe.localPort,
                          g_xslProbe.peerCount,
                          g_xslProbe.sendCounter,
                          g_xslProbe.lastError);
    }
}

void xboxSystemLinkProbe_GetStatus(XboxSystemLinkProbeStatus *outStatus)
{
    if (!outStatus)
        return;

    xboxSystemLinkProbe_EnsureState();
    memset(outStatus, 0, sizeof(*outStatus));
    outStatus->started = g_xslProbe.started;
    outStatus->socketsReady = g_xslProbe.socketsReady;
    outStatus->localPort = g_xslProbe.localPort;
    outStatus->localId = g_xslProbe.localId;
    outStatus->sent = g_xslProbe.sendCounter;
    outStatus->lastError = g_xslProbe.lastError;
    outStatus->peerCount = g_xslProbe.peerCount;
    memcpy(outStatus->peers, g_xslProbe.peers, sizeof(outStatus->peers));
}
#else
int xboxSystemLinkProbe_Start(void) { return 0; }
void xboxSystemLinkProbe_Stop(void) {}
void xboxSystemLinkProbe_Tick(void) {}
void xboxSystemLinkProbe_GetStatus(XboxSystemLinkProbeStatus *outStatus)
{
    if (outStatus)
        memset(outStatus, 0, sizeof(*outStatus));
}
void xboxSystemLinkProbe_FormatAddress(unsigned long address, char *out, int outCount)
{
    if (out && outCount > 0)
        out[0] = 0;
    (void)address;
}
#endif
