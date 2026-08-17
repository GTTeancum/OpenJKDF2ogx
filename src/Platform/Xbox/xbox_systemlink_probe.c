#include "Platform/Xbox/xbox_systemlink_probe.h"

#include <string.h>

#ifdef TARGET_XBOX
#include "Devices/sithConsole.h"
#include "Dss/sithMulti.h"
#include "Gameplay/sithPlayer.h"
#include "General/stdString.h"
#include "Platform/Xbox/xbox_debug.h"
#include "Platform/Xbox/xbox_splitscreen.h"
#include "Win95/stdComm.h"
#include "World/jkPlayer.h"
#include "World/sithThing.h"
#include "World/sithWorld.h"
#include "jk.h"

#include <stdio.h>
#include <stdlib.h>
#include <WinSockX.h>

#define XSL_BASE_PORT 9777
#define XSL_PORT_COUNT 4
#define XSL_GAME_BASE_PORT 7777
#define XSL_GAME_PORT_COUNT 4
#define XSL_PACKET_MAGIC "JKXSL4"
#define XSL_GAME_MAGIC 0x47534B4Au
#define XSL_GAME_VERSION 1
#define XSL_HOST_CLAIM_MS 1500u
#define XSL_PEER_TIMEOUT_MS 8000u
#define XSL_SEND_INTERVAL_MS 500u
#define XSL_LAUNCH_DELAY_MS 800u
#define XSL_LAUNCH_DEADLINE_MS 5000u
#define XSL_MAX_PACKET 2304
#define XSL_GAME_QUEUE 32
#define XSL_GAME_RECV_SCAN_LIMIT 64
#define XSL_HEALTH_WARN_INTERVAL_MS 1000u
#define XSL_HEALTH_PACKET_STALE_MS 15000u

enum
{
    XSL_HEALTH_OK = 0,
    XSL_HEALTH_NO_PEER = 1,
    XSL_HEALTH_NO_SESSION = 2,
    XSL_HEALTH_NO_HOST_ADDRESS = 3,
    XSL_HEALTH_STALE_PACKETS = 4
};

typedef struct XboxSystemLinkGamePacketHeader
{
    unsigned long magic;
    unsigned short version;
    unsigned short headerSize;
    unsigned long idFrom;
    unsigned long idTo;
    unsigned long payloadSize;
    unsigned long sequence;
} XboxSystemLinkGamePacketHeader;

typedef struct XboxSystemLinkGameQueueEntry
{
    int used;
    int idFrom;
    int len;
    unsigned char data[2052];
} XboxSystemLinkGameQueueEntry;

typedef struct XboxSystemLinkPeerState
{
    XboxSystemLinkProbePeer publicPeer;
    unsigned long firstSeenMs;
    int hasXnAddr;
    XNADDR xnAddr;
    int hasSessionKey;
    XNKID sessionKeyId;
    XNKEY sessionKey;
    int hasSecureAddress;
    unsigned long secureAddress;
} XboxSystemLinkPeerState;

typedef struct XboxSystemLinkProbeState
{
    int initialized;
    int started;
    int socketsReady;
    int xnetStartupOwned;
    int wsaStartupOwned;
    SOCKET lobbySocket;
    SOCKET gameSocket;
    int localPort;
    int localGamePort;
    unsigned long localId;
    unsigned long sendCounter;
    unsigned long gameSendCounter;
    unsigned long lastSendMs;
    unsigned long lastLogMs;
    unsigned long enterMs;
    int lastError;
    int role;
    int lastLoggedRole;
    unsigned long hostId;
    unsigned long lastLoggedHostId;
    int phase;
    int localPlayerCount;
    int readyMask;
    int confirmed;
    int localMachineIndex;
    int localFirstPlayerIndex;
    int rosterMaxPlayers;
    int peerCount;
    XboxSystemLinkPeerState peers[XBOX_SYSTEMLINK_PROBE_MAX_PEERS];
    int sessionRegistered;
    int sessionIsHost;
    int sessionKeyOwned;
    unsigned long sessionHostId;
    XNADDR localXnAddr;
    unsigned long localXnAddrStatus;
    int hasLocalXnAddr;
    XNKID sessionKeyId;
    XNKEY sessionKey;
    int hasSecureHostAddress;
    unsigned long secureHostAddress;
    jkMultiEntry3 selectedEntry;
    unsigned long launchId;
    unsigned long launchAckId;
    unsigned long lastSeenLaunchId;
    int pendingTravel;
    int pendingTravelIsHost;
    unsigned long pendingTravelMs;
    unsigned long launchDeadlineMs;
    int gameplayActive;
    int gameplayIsHost;
    int smokeHarness;
    unsigned long lastGameSendMs;
    unsigned long lastGameReceiveMs;
    unsigned long lastHealthWarnMs;
    int lastHealthWarnCode;
    XboxSystemLinkGameQueueEntry loopback[XSL_GAME_QUEUE];
} XboxSystemLinkProbeState;

static XboxSystemLinkProbeState g_xsl;
static int g_xslSendTraceBudget = 0;
static int g_xslRecvTraceBudget = 0;

static unsigned long xboxSystemLinkProbe_NowMs(void)
{
    return GetTickCount();
}

static void xboxSystemLinkProbe_EnsureState(void)
{
    if (g_xsl.initialized)
        return;

    memset(&g_xsl, 0, sizeof(g_xsl));
    g_xsl.lobbySocket = INVALID_SOCKET;
    g_xsl.gameSocket = INVALID_SOCKET;
    g_xsl.role = XBOX_SYSTEMLINK_ROLE_SEEKING;
    g_xsl.lastLoggedRole = -1;
    g_xsl.phase = XBOX_SYSTEMLINK_PHASE_DISCOVERY;
    g_xsl.localPlayerCount = 1;
    g_xsl.readyMask = 1;
    g_xsl.localFirstPlayerIndex = 0;
    g_xsl.rosterMaxPlayers = XBOX_SYSTEMLINK_PLAYER_STRIDE;
    g_xsl.initialized = 1;
}

static char xboxSystemLinkProbe_HexDigit(int value)
{
    value &= 0xF;
    return (char)(value < 10 ? ('0' + value) : ('A' + value - 10));
}

static int xboxSystemLinkProbe_HexValue(char ch)
{
    if (ch >= '0' && ch <= '9')
        return ch - '0';
    if (ch >= 'a' && ch <= 'f')
        return ch - 'a' + 10;
    if (ch >= 'A' && ch <= 'F')
        return ch - 'A' + 10;
    return -1;
}

static void xboxSystemLinkProbe_HexEncode(const unsigned char *data, int count, char *out, int outCount)
{
    int i;
    int pos;

    if (!out || outCount <= 0)
        return;
    pos = 0;
    for (i = 0; i < count && pos + 2 < outCount; i++)
    {
        out[pos++] = xboxSystemLinkProbe_HexDigit(data[i] >> 4);
        out[pos++] = xboxSystemLinkProbe_HexDigit(data[i]);
    }
    out[pos] = 0;
}

static void xboxSystemLinkProbe_HexZeroes(int count, char *out, int outCount)
{
    int i;
    int maxChars;

    if (!out || outCount <= 0)
        return;
    maxChars = count * 2;
    if (maxChars >= outCount)
        maxChars = outCount - 1;
    for (i = 0; i < maxChars; i++)
        out[i] = '0';
    out[maxChars] = 0;
}

static int xboxSystemLinkProbe_HexDecode(const char *in, unsigned char *out, int count)
{
    int i;

    if (!in || !out)
        return 0;
    for (i = 0; i < count; i++)
    {
        int hi = xboxSystemLinkProbe_HexValue(in[i * 2]);
        int lo = xboxSystemLinkProbe_HexValue(in[i * 2 + 1]);
        if (hi < 0 || lo < 0)
            return 0;
        out[i] = (unsigned char)((hi << 4) | lo);
    }
    return 1;
}

static int xboxSystemLinkProbe_SplitPacket(char *buffer, char **tokens, int maxTokens)
{
    int count;
    char *cursor;

    if (!buffer || !tokens || maxTokens <= 0)
        return 0;

    count = 0;
    cursor = buffer;
    for (;;)
    {
        char *separator;
        if (count >= maxTokens)
            return -1;

        tokens[count++] = cursor;
        separator = strchr(cursor, '|');
        if (!separator)
            break;

        *separator = 0;
        cursor = separator + 1;
    }

    return count;
}

static int xboxSystemLinkProbe_ParseUInt(const char *text, int base, unsigned long *out)
{
    char *end;
    unsigned long value;

    if (!text || !text[0] || !out)
        return 0;

    value = strtoul(text, &end, base);
    if (!end || *end)
        return 0;

    *out = value;
    return 1;
}

static void xboxSystemLinkProbe_CopyPacketString(const char *in, char *out, int outCount)
{
    if (!out || outCount <= 0)
        return;

    if (!in)
    {
        out[0] = 0;
        return;
    }

    strncpy(out, in, outCount - 1);
    out[outCount - 1] = 0;
}

static int xboxSystemLinkProbe_ParsePacket(char *buffer,
                                           unsigned long *id,
                                           int *port,
                                           unsigned long *counter,
                                           int *role,
                                           unsigned long *hostId,
                                           int *phase,
                                           int *gamePort,
                                           int *localPlayerCount,
                                           int *readyMask,
                                           int *confirmed,
                                           unsigned long *launchId,
                                           unsigned long *launchAckId,
                                           int *rosterMax,
                                           jkMultiEntry3 *entry,
                                           int *secureBits,
                                           char *xnAddrHex,
                                           int xnAddrHexCount,
                                           char *keyIdHex,
                                           int keyIdHexCount,
                                           char *keyHex,
                                           int keyHexCount)
{
    enum
    {
        XSL_PACKET_TOKEN_COUNT = 24
    };
    char *tokens[XSL_PACKET_TOKEN_COUNT];
    unsigned long value;

    if (!buffer || !entry)
        return 0;

    if (xboxSystemLinkProbe_SplitPacket(buffer, tokens, XSL_PACKET_TOKEN_COUNT) != XSL_PACKET_TOKEN_COUNT)
        return 0;

    if (strcmp(tokens[0], XSL_PACKET_MAGIC) != 0)
        return 0;

    if (!xboxSystemLinkProbe_ParseUInt(tokens[1], 16, id))
        return 0;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[2], 10, &value))
        return 0;
    *port = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[3], 10, counter))
        return 0;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[4], 10, &value))
        return 0;
    *role = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[5], 16, hostId))
        return 0;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[6], 10, &value))
        return 0;
    *phase = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[7], 10, &value))
        return 0;
    *gamePort = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[8], 10, &value))
        return 0;
    *localPlayerCount = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[9], 10, &value))
        return 0;
    *readyMask = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[10], 10, &value))
        return 0;
    *confirmed = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[11], 16, launchId))
        return 0;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[12], 16, launchAckId))
        return 0;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[13], 10, &value))
        return 0;
    *rosterMax = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[14], 10, &value))
        return 0;
    entry->multiModeFlags = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[15], 10, &value))
        return 0;
    entry->scoreLimit = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[16], 10, &value))
        return 0;
    entry->timeLimit = (int)value;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[17], 10, &value))
        return 0;
    entry->maxRank = (int)value;
    xboxSystemLinkProbe_CopyPacketString(tokens[18], entry->episodeGobName, sizeof(entry->episodeGobName));
    xboxSystemLinkProbe_CopyPacketString(tokens[19], entry->mapJklFname, sizeof(entry->mapJklFname));
    if (!strcmp(entry->episodeGobName, "-"))
        entry->episodeGobName[0] = 0;
    if (!strcmp(entry->mapJklFname, "-"))
        entry->mapJklFname[0] = 0;
    if (!xboxSystemLinkProbe_ParseUInt(tokens[20], 10, &value))
        return 0;
    *secureBits = (int)value;
    xboxSystemLinkProbe_CopyPacketString(tokens[21], xnAddrHex, xnAddrHexCount);
    xboxSystemLinkProbe_CopyPacketString(tokens[22], keyIdHex, keyIdHexCount);
    xboxSystemLinkProbe_CopyPacketString(tokens[23], keyHex, keyHexCount);
    return 1;
}

static int xboxSystemLinkProbe_SameKeyId(const XNKID *a, const XNKID *b)
{
    return a && b && memcmp(a, b, sizeof(*a)) == 0;
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

static const char *xboxSystemLinkProbe_PacketStringOrDash(const char *text)
{
    return (text && text[0]) ? text : "-";
}

static int xboxSystemLinkProbe_InitSockets(void)
{
    XNetStartupParams params;
    WSADATA wsaData;
    int xnetResult;
    int wsaResult;
    unsigned long linkStatus;

    xboxSystemLinkProbe_EnsureState();
    if (g_xsl.socketsReady)
        return 1;

    memset(&params, 0, sizeof(params));
    params.cfgSizeOfStruct = sizeof(params);
    params.cfgFlags = XNET_STARTUP_BYPASS_SECURITY;
    params.cfgPrivatePoolSizeInPages = 12;
    params.cfgEnetReceiveQueueLength = 8;
    params.cfgIpFragMaxSimultaneous = 4;
    params.cfgIpFragMaxPacketDiv256 = 8;
    params.cfgSockMaxSockets = 64;
    params.cfgSockDefaultRecvBufsizeInK = 32;
    params.cfgSockDefaultSendBufsizeInK = 32;
    params.cfgKeyRegMax = 8;
    params.cfgSecRegMax = 32;
    params.cfgQosDataLimitDiv4 = 64;

    xnetResult = XNetStartup(&params);
    g_xsl.xnetStartupOwned = (xnetResult == 0);
    linkStatus = XNetGetEthernetLinkStatus();
    memset(&wsaData, 0, sizeof(wsaData));
    wsaResult = WSAStartup(MAKEWORD(1, 1), &wsaData);
    g_xsl.wsaStartupOwned = (wsaResult == 0);

    g_xsl.socketsReady = ((xnetResult == 0 || xnetResult == WSAEALREADY) && wsaResult == 0);
    g_xsl.lastError = g_xsl.socketsReady ? 0 : (wsaResult ? wsaResult : xnetResult);
    xbox_debug_Printf("XSL net init xnet=%d wsa=%d ready=%d link=0x%08X active=%d\n",
                      xnetResult,
                      wsaResult,
                      g_xsl.socketsReady,
                      linkStatus,
                      (linkStatus & XNET_ETHERNET_LINK_ACTIVE) ? 1 : 0);
    if (!g_xsl.socketsReady)
    {
        if (g_xsl.wsaStartupOwned)
        {
            WSACleanup();
            g_xsl.wsaStartupOwned = 0;
        }
        if (g_xsl.xnetStartupOwned)
        {
            XNetCleanup();
            g_xsl.xnetStartupOwned = 0;
        }
    }
    return g_xsl.socketsReady;
}

static void xboxSystemLinkProbe_CleanupSockets(const char *reason)
{
    int cleanedWsa;
    int cleanedXnet;

    cleanedWsa = g_xsl.wsaStartupOwned;
    cleanedXnet = g_xsl.xnetStartupOwned;

    if (!g_xsl.socketsReady && !cleanedWsa && !cleanedXnet)
        return;

    if (g_xsl.wsaStartupOwned)
    {
        WSACleanup();
        g_xsl.wsaStartupOwned = 0;
    }
    if (g_xsl.xnetStartupOwned)
    {
        XNetCleanup();
        g_xsl.xnetStartupOwned = 0;
    }

    g_xsl.socketsReady = 0;
    g_xsl.hasLocalXnAddr = 0;
    g_xsl.localXnAddrStatus = 0;
    xbox_debug_Printf("XSL net cleanup reason=%s wsa=%d xnet=%d\n",
                      reason ? reason : "unknown",
                      cleanedWsa,
                      cleanedXnet);
}

static void xboxSystemLinkProbe_ClearPeerSecureAddress(XboxSystemLinkPeerState *peer, const char *reason)
{
    IN_ADDR secureAddr;
    int result;

    if (!peer || !peer->hasSecureAddress)
        return;

    secureAddr.s_addr = peer->secureAddress;
    result = XNetUnregisterInAddr(secureAddr);
    xbox_debug_Printf("XSL secure addr unregister reason=%s peer=0x%08X addr=0x%08X result=%d\n",
                      reason ? reason : "unknown",
                      peer->publicPeer.id,
                      peer->secureAddress,
                      result);
    peer->hasSecureAddress = 0;
    peer->secureAddress = 0;
}

static void xboxSystemLinkProbe_ClearAllSecureAddresses(const char *reason)
{
    int i;

    for (i = 0; i < g_xsl.peerCount; i++)
        xboxSystemLinkProbe_ClearPeerSecureAddress(&g_xsl.peers[i], reason);
    g_xsl.hasSecureHostAddress = 0;
    g_xsl.secureHostAddress = 0;
}

static void xboxSystemLinkProbe_UnregisterSession(const char *reason)
{
    int result;

    if (!g_xsl.sessionRegistered)
        return;

    xboxSystemLinkProbe_ClearAllSecureAddresses(reason);

    result = 0;
    if (g_xsl.sessionKeyOwned)
        result = XNetUnregisterKey(&g_xsl.sessionKeyId);

    xbox_debug_Printf("XSL session unregister reason=%s host=%d hostId=0x%08X owned=%d result=%d\n",
                      reason ? reason : "unknown",
                      g_xsl.sessionIsHost,
                      g_xsl.sessionHostId,
                      g_xsl.sessionKeyOwned,
                      result);
    g_xsl.sessionRegistered = 0;
    g_xsl.sessionIsHost = 0;
    g_xsl.sessionKeyOwned = 0;
    g_xsl.sessionHostId = 0;
    memset(&g_xsl.sessionKeyId, 0, sizeof(g_xsl.sessionKeyId));
    memset(&g_xsl.sessionKey, 0, sizeof(g_xsl.sessionKey));
}

static int xboxSystemLinkProbe_UpdateLocalXnAddr(void)
{
    unsigned long status;

    if (!g_xsl.socketsReady)
        return 0;

    status = XNetGetTitleXnAddr(&g_xsl.localXnAddr);
    g_xsl.localXnAddrStatus = status;
    g_xsl.hasLocalXnAddr =
        status != XNET_GET_XNADDR_PENDING
        && (status & XNET_GET_XNADDR_NONE) == 0
        && (status & XNET_GET_XNADDR_TROUBLESHOOT) == 0;
    return g_xsl.hasLocalXnAddr;
}

static int xboxSystemLinkProbe_EnsureHostSession(void)
{
    int created;
    int registered;

    xboxSystemLinkProbe_EnsureState();
    if (g_xsl.sessionRegistered && g_xsl.sessionIsHost)
        return 1;

    if (g_xsl.sessionRegistered)
        xboxSystemLinkProbe_UnregisterSession("host role change");

    created = XNetCreateKey(&g_xsl.sessionKeyId, &g_xsl.sessionKey);
    registered = (created == 0) ? XNetRegisterKey(&g_xsl.sessionKeyId, &g_xsl.sessionKey) : created;
    if (created != 0 || registered != 0)
    {
        g_xsl.lastError = registered ? registered : created;
        xbox_debug_Printf("XSL host session failed create=%d register=%d\n", created, registered);
        return 0;
    }

    g_xsl.sessionRegistered = 1;
    g_xsl.sessionIsHost = 1;
    g_xsl.sessionKeyOwned = 1;
    g_xsl.sessionHostId = g_xsl.localId;
    xbox_debug_Printf("XSL host session registered id=0x%08X\n", g_xsl.localId);
    return 1;
}

static XboxSystemLinkPeerState *xboxSystemLinkProbe_FindPeer(unsigned long id)
{
    int i;

    for (i = 0; i < g_xsl.peerCount; i++)
        if (g_xsl.peers[i].publicPeer.id == id)
            return &g_xsl.peers[i];
    return 0;
}

static int xboxSystemLinkProbe_IsXNetVirtualAddress(unsigned long address)
{
    unsigned char *b = (unsigned char *)&address;
    return b[0] == 0 && b[1] == 0;
}

static int xboxSystemLinkProbe_ResolvePeerSecureAddress(XboxSystemLinkPeerState *peer, unsigned long *outAddress)
{
    IN_ADDR secureAddr;
    int result;
    unsigned long connectStatus;
    int connectResult;

    if (outAddress)
        *outAddress = 0;
    if (!peer || !peer->hasXnAddr || !g_xsl.sessionRegistered)
        return 0;

    memset(&secureAddr, 0, sizeof(secureAddr));
    result = XNetXnAddrToInAddr(&peer->xnAddr, &g_xsl.sessionKeyId, &secureAddr);
    if (result != 0)
    {
        g_xsl.lastError = result;
        xbox_debug_Printf("XSL secure addr translate failed peer=0x%08X result=%d\n",
                          peer->publicPeer.id,
                          result);
        return 0;
    }

    connectStatus = XNetGetConnectStatus(secureAddr);
    if (!peer->hasSecureAddress
        || peer->secureAddress != secureAddr.s_addr
        || connectStatus == XNET_CONNECT_STATUS_IDLE
        || connectStatus == XNET_CONNECT_STATUS_LOST)
    {
        connectResult = XNetConnect(secureAddr);
        if (connectResult != 0)
            g_xsl.lastError = connectResult;
        xbox_debug_Printf("XSL secure addr translated peer=0x%08X addr=0x%08X status=%lu connect=%d\n",
                          peer->publicPeer.id,
                          secureAddr.s_addr,
                          connectStatus,
                          connectResult);
    }

    if (peer->hasSecureAddress && peer->secureAddress != secureAddr.s_addr)
        xboxSystemLinkProbe_ClearPeerSecureAddress(peer, "secure addr changed");
    peer->secureAddress = secureAddr.s_addr;
    peer->hasSecureAddress = 1;
    if (outAddress)
        *outAddress = secureAddr.s_addr;
    return 1;
}

static int xboxSystemLinkProbe_ResolveHostSecureAddress(XboxSystemLinkPeerState *hostPeer, unsigned long *outAddress)
{
    int result;

    if (outAddress)
        *outAddress = 0;
    if (!hostPeer || !hostPeer->hasXnAddr || !hostPeer->hasSessionKey)
        return 0;

    if (g_xsl.sessionRegistered
        && (g_xsl.sessionIsHost
            || g_xsl.sessionHostId != hostPeer->publicPeer.id
            || !xboxSystemLinkProbe_SameKeyId(&g_xsl.sessionKeyId, &hostPeer->sessionKeyId)))
    {
        xboxSystemLinkProbe_UnregisterSession("client host change");
    }

    if (!g_xsl.sessionRegistered)
    {
        result = XNetRegisterKey(&hostPeer->sessionKeyId, &hostPeer->sessionKey);
        if (result != 0 && result != WSAEALREADY)
        {
            g_xsl.lastError = result;
            xbox_debug_Printf("XSL client session register failed host=0x%08X result=%d\n",
                              hostPeer->publicPeer.id,
                              result);
            return 0;
        }

        g_xsl.sessionRegistered = 1;
        g_xsl.sessionIsHost = 0;
        g_xsl.sessionKeyOwned = (result == 0);
        g_xsl.sessionHostId = hostPeer->publicPeer.id;
        memcpy(&g_xsl.sessionKeyId, &hostPeer->sessionKeyId, sizeof(g_xsl.sessionKeyId));
        memcpy(&g_xsl.sessionKey, &hostPeer->sessionKey, sizeof(g_xsl.sessionKey));
        xbox_debug_Printf("XSL client session registered host=0x%08X result=%d\n",
                          hostPeer->publicPeer.id,
                          result);
    }

    if (!xboxSystemLinkProbe_ResolvePeerSecureAddress(hostPeer, outAddress))
        return 0;

    g_xsl.secureHostAddress = hostPeer->secureAddress;
    g_xsl.hasSecureHostAddress = 1;
    return 1;
}

static int xboxSystemLinkProbe_OpenSocketBound(SOCKET *socketOut, int basePort, int portCount, int *portOut, const char *label)
{
    SOCKET sock;
    BOOL yes;
    u_long noBlock;
    int i;

    if (!socketOut || !portOut)
        return 0;
    if (*socketOut != INVALID_SOCKET)
        return 1;
    if (!xboxSystemLinkProbe_InitSockets())
        return 0;

    sock = socket(AF_INET, SOCK_DGRAM, IPPROTO_UDP);
    if (sock == INVALID_SOCKET)
    {
        g_xsl.lastError = WSAGetLastError();
        xbox_debug_Printf("XSL %s socket failed err=%d\n", label, g_xsl.lastError);
        return 0;
    }

    yes = TRUE;
    setsockopt(sock, SOL_SOCKET, SO_BROADCAST, (char *)&yes, sizeof(yes));
    setsockopt(sock, SOL_SOCKET, SO_REUSEADDR, (char *)&yes, sizeof(yes));
    noBlock = 1;
    ioctlsocket(sock, FIONBIO, &noBlock);

    for (i = 0; i < portCount; i++)
    {
        struct sockaddr_in addr;
        memset(&addr, 0, sizeof(addr));
        addr.sin_family = AF_INET;
        addr.sin_addr.s_addr = INADDR_ANY;
        addr.sin_port = htons((u_short)(basePort + i));

        if (bind(sock, (struct sockaddr *)&addr, sizeof(addr)) == 0)
        {
            noBlock = 1;
            if (ioctlsocket(sock, FIONBIO, &noBlock) == SOCKET_ERROR)
            {
                g_xsl.lastError = WSAGetLastError();
                xbox_debug_Printf("XSL %s nonblocking-after-bind failed err=%d\n", label, g_xsl.lastError);
            }
            *socketOut = sock;
            *portOut = basePort + i;
            xbox_debug_Printf("XSL %s socket bound port=%d\n", label, *portOut);
            return 1;
        }
    }

    g_xsl.lastError = WSAGetLastError();
    xbox_debug_Printf("XSL %s bind failed err=%d\n", label, g_xsl.lastError);
    closesocket(sock);
    return 0;
}

static int xboxSystemLinkProbe_OpenGameSocket(void)
{
    return xboxSystemLinkProbe_OpenSocketBound(&g_xsl.gameSocket,
                                               XSL_GAME_BASE_PORT,
                                               XSL_GAME_PORT_COUNT,
                                               &g_xsl.localGamePort,
                                               "game");
}

static void xboxSystemLinkProbe_CloseGameSocket(void)
{
    if (g_xsl.gameSocket != INVALID_SOCKET)
    {
        closesocket(g_xsl.gameSocket);
        g_xsl.gameSocket = INVALID_SOCKET;
    }
    g_xsl.localGamePort = 0;
}

static void xboxSystemLinkProbe_CloseLobbySocket(void)
{
    if (g_xsl.lobbySocket != INVALID_SOCKET)
    {
        closesocket(g_xsl.lobbySocket);
        g_xsl.lobbySocket = INVALID_SOCKET;
    }
    g_xsl.localPort = 0;
}

static int xboxSystemLinkProbe_CompareIds(unsigned long a, unsigned long b)
{
    if (a < b)
        return -1;
    if (a > b)
        return 1;
    return 0;
}

static void xboxSystemLinkProbe_UpdateRoster(void)
{
    unsigned long ids[XBOX_SYSTEMLINK_MAX_MACHINES];
    int count;
    int i;
    int j;

    count = 0;
    if (g_xsl.localId)
        ids[count++] = g_xsl.localId;
    for (i = 0; i < g_xsl.peerCount && count < XBOX_SYSTEMLINK_MAX_MACHINES; i++)
        ids[count++] = g_xsl.peers[i].publicPeer.id;

    for (i = 0; i < count; i++)
    {
        for (j = i + 1; j < count; j++)
        {
            if (xboxSystemLinkProbe_CompareIds(ids[j], ids[i]) < 0)
            {
                unsigned long tmp = ids[i];
                ids[i] = ids[j];
                ids[j] = tmp;
            }
        }
    }

    g_xsl.localMachineIndex = 0;
    for (i = 0; i < count; i++)
    {
        XboxSystemLinkPeerState *peer;
        if (ids[i] == g_xsl.localId)
        {
            g_xsl.localMachineIndex = i;
            continue;
        }
        peer = xboxSystemLinkProbe_FindPeer(ids[i]);
        if (peer)
        {
            peer->publicPeer.machineIndex = i;
            peer->publicPeer.firstPlayerIndex = i * XBOX_SYSTEMLINK_PLAYER_STRIDE;
        }
    }

    g_xsl.localFirstPlayerIndex = g_xsl.localMachineIndex * XBOX_SYSTEMLINK_PLAYER_STRIDE;
    g_xsl.rosterMaxPlayers = count * XBOX_SYSTEMLINK_PLAYER_STRIDE;
    if (g_xsl.rosterMaxPlayers < XBOX_SYSTEMLINK_PLAYER_STRIDE)
        g_xsl.rosterMaxPlayers = XBOX_SYSTEMLINK_PLAYER_STRIDE;
    if (g_xsl.rosterMaxPlayers > JKPLAYER_NUM_INFOS)
        g_xsl.rosterMaxPlayers = JKPLAYER_NUM_INFOS;
}

static void xboxSystemLinkProbe_UpdateElection(unsigned long nowMs)
{
    unsigned long bestHost;
    int newRole;
    int i;

    bestHost = 0;
    if (g_xsl.peerCount > 0 || nowMs - g_xsl.enterMs >= XSL_HOST_CLAIM_MS)
        bestHost = g_xsl.localId;
    for (i = 0; i < g_xsl.peerCount; i++)
    {
        unsigned long peerId = g_xsl.peers[i].publicPeer.id;
        if (!bestHost || peerId < bestHost)
            bestHost = peerId;
    }

    newRole = XBOX_SYSTEMLINK_ROLE_SEEKING;
    if (bestHost)
        newRole = (bestHost == g_xsl.localId) ? XBOX_SYSTEMLINK_ROLE_HOST : XBOX_SYSTEMLINK_ROLE_CLIENT;

    if (newRole != g_xsl.role || bestHost != g_xsl.hostId)
    {
        int oldRole = g_xsl.role;
        g_xsl.role = newRole;
        g_xsl.hostId = bestHost;
        if (oldRole == XBOX_SYSTEMLINK_ROLE_HOST && newRole != XBOX_SYSTEMLINK_ROLE_HOST)
            xboxSystemLinkProbe_UnregisterSession("lost host election");
        if (newRole != XBOX_SYSTEMLINK_ROLE_CLIENT && g_xsl.sessionRegistered && !g_xsl.sessionIsHost)
            xboxSystemLinkProbe_UnregisterSession("left client role");
        if (newRole == XBOX_SYSTEMLINK_ROLE_HOST)
            xboxSystemLinkProbe_EnsureHostSession();
        if (g_xsl.phase == XBOX_SYSTEMLINK_PHASE_DISCOVERY && newRole != XBOX_SYSTEMLINK_ROLE_SEEKING)
            g_xsl.phase = XBOX_SYSTEMLINK_PHASE_READY;
    }

    xboxSystemLinkProbe_UpdateRoster();

    if (g_xsl.role != g_xsl.lastLoggedRole || g_xsl.hostId != g_xsl.lastLoggedHostId)
    {
        g_xsl.lastLoggedRole = g_xsl.role;
        g_xsl.lastLoggedHostId = g_xsl.hostId;
        xbox_debug_Printf("XSL lobby role local=0x%08X role=%d host=0x%08X peers=%d machines=%d firstPlayer=%d\n",
                          g_xsl.localId,
                          g_xsl.role,
                          g_xsl.hostId,
                          g_xsl.peerCount,
                          xboxSystemLinkProbe_GroupMachineCount(),
                          g_xsl.localFirstPlayerIndex);
    }
}

static void xboxSystemLinkProbe_PrintGameplayHealthWarning(unsigned long nowMs, int code, const char *message)
{
    unsigned long recvAgeMs;

    if (!message || code == XSL_HEALTH_OK)
        return;
    if (code == g_xsl.lastHealthWarnCode
        && nowMs - g_xsl.lastHealthWarnMs < XSL_HEALTH_WARN_INTERVAL_MS)
        return;

    recvAgeMs = g_xsl.lastGameReceiveMs ? nowMs - g_xsl.lastGameReceiveMs : 0;
    sithConsole_Print(message);
    xbox_debug_Printf("XSL health warning code=%d peers=%d host=%d session=%d secureHost=%d lastSendAge=%lu lastRecvAge=%lu lastErr=%d\n",
                      code,
                      g_xsl.peerCount,
                      g_xsl.gameplayIsHost,
                      g_xsl.sessionRegistered,
                      g_xsl.hasSecureHostAddress,
                      g_xsl.lastGameSendMs ? nowMs - g_xsl.lastGameSendMs : 0,
                      recvAgeMs,
                      g_xsl.lastError);
    g_xsl.lastHealthWarnMs = nowMs;
    g_xsl.lastHealthWarnCode = code;
}

static void xboxSystemLinkProbe_TickGameplayHealth(unsigned long nowMs)
{
    int code;
    const char *message;

    if (!g_xsl.gameplayActive || g_xsl.smokeHarness)
        return;

    code = XSL_HEALTH_OK;
    message = 0;
    if (g_xsl.peerCount <= 0)
    {
        code = XSL_HEALTH_NO_PEER;
        message = "SYSTEM LINK WARNING: no remote Xbox detected.";
    }
    else if (!g_xsl.sessionRegistered)
    {
        code = XSL_HEALTH_NO_SESSION;
        message = "SYSTEM LINK WARNING: secure session is not active.";
    }
    else if (!g_xsl.gameplayIsHost && !g_xsl.hasSecureHostAddress)
    {
        code = XSL_HEALTH_NO_HOST_ADDRESS;
        message = "SYSTEM LINK WARNING: host address is not resolved.";
    }
    else if (g_xsl.lastGameReceiveMs
        && nowMs - g_xsl.lastGameReceiveMs >= XSL_HEALTH_PACKET_STALE_MS)
    {
        code = XSL_HEALTH_STALE_PACKETS;
        message = "SYSTEM LINK WARNING: no network packets received.";
    }

    if (code == XSL_HEALTH_OK)
    {
        if (g_xsl.lastHealthWarnCode != XSL_HEALTH_OK)
            xbox_debug_Printf("XSL health restored peers=%d host=%d session=%d\n",
                              g_xsl.peerCount,
                              g_xsl.gameplayIsHost,
                              g_xsl.sessionRegistered);
        g_xsl.lastHealthWarnCode = XSL_HEALTH_OK;
        return;
    }

    xboxSystemLinkProbe_PrintGameplayHealthWarning(nowMs, code, message);
}

static void xboxSystemLinkProbe_ExpirePeers(unsigned long nowMs)
{
    int i;

    for (i = g_xsl.peerCount - 1; i >= 0; i--)
    {
        if (nowMs - g_xsl.peers[i].publicPeer.lastSeenMs > XSL_PEER_TIMEOUT_MS)
        {
            xbox_debug_Printf("XSL peer expired id=0x%08X packets=%lu\n",
                              g_xsl.peers[i].publicPeer.id,
                              g_xsl.peers[i].publicPeer.packets);
            xboxSystemLinkProbe_ClearPeerSecureAddress(&g_xsl.peers[i], "peer expired");
            if (i < g_xsl.peerCount - 1)
            {
                memmove(&g_xsl.peers[i],
                        &g_xsl.peers[i + 1],
                        sizeof(g_xsl.peers[0]) * (g_xsl.peerCount - i - 1));
            }
            g_xsl.peerCount--;
        }
    }
}

static void xboxSystemLinkProbe_CopyEntry(jkMultiEntry3 *dst, const jkMultiEntry3 *src)
{
    if (!dst || !src)
        return;
    memcpy(dst, src, sizeof(*dst));
    dst->episodeGobName[sizeof(dst->episodeGobName) - 1] = 0;
    dst->mapJklFname[sizeof(dst->mapJklFname) - 1] = 0;
}

static void xboxSystemLinkProbe_RecordPeer(unsigned long id,
                                           unsigned long address,
                                           int port,
                                           int gamePort,
                                           int role,
                                           unsigned long hostId,
                                           int phase,
                                           int localPlayerCount,
                                           int readyMask,
                                           int confirmed,
                                           unsigned long launchId,
                                           unsigned long launchAckId,
                                           const jkMultiEntry3 *entry,
                                           int secureBits,
                                           const XNADDR *xnAddr,
                                           const XNKID *keyId,
                                           const XNKEY *key,
                                           unsigned long nowMs)
{
    XboxSystemLinkPeerState *peer;
    int isNew;
    char addressText[32];

    if (!id || id == g_xsl.localId)
        return;

    if (role < XBOX_SYSTEMLINK_ROLE_SEEKING || role > XBOX_SYSTEMLINK_ROLE_CLIENT)
        role = XBOX_SYSTEMLINK_ROLE_SEEKING;
    if (phase < XBOX_SYSTEMLINK_PHASE_DISCOVERY || phase > XBOX_SYSTEMLINK_PHASE_IN_GAME)
        phase = XBOX_SYSTEMLINK_PHASE_DISCOVERY;
    if (localPlayerCount < 0)
        localPlayerCount = 0;
    if (localPlayerCount > XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS)
        localPlayerCount = XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS;
    if (port < XSL_BASE_PORT || port >= XSL_BASE_PORT + XSL_PORT_COUNT)
        port = 0;
    if (gamePort < XSL_GAME_BASE_PORT || gamePort >= XSL_GAME_BASE_PORT + XSL_GAME_PORT_COUNT)
        gamePort = 0;

    peer = xboxSystemLinkProbe_FindPeer(id);
    isNew = 0;
    if (!peer)
    {
        if (g_xsl.peerCount >= XBOX_SYSTEMLINK_PROBE_MAX_PEERS)
        {
            memmove(&g_xsl.peers[0],
                    &g_xsl.peers[1],
                    sizeof(g_xsl.peers[0]) * (XBOX_SYSTEMLINK_PROBE_MAX_PEERS - 1));
            g_xsl.peerCount = XBOX_SYSTEMLINK_PROBE_MAX_PEERS - 1;
        }
        peer = &g_xsl.peers[g_xsl.peerCount++];
        memset(peer, 0, sizeof(*peer));
        peer->publicPeer.id = id;
        peer->firstSeenMs = nowMs;
        isNew = 1;
    }

    peer->publicPeer.address = address;
    peer->publicPeer.port = port;
    peer->publicPeer.gamePort = gamePort;
    peer->publicPeer.role = role;
    peer->publicPeer.hostId = hostId;
    peer->publicPeer.phase = phase;
    peer->publicPeer.localPlayerCount = localPlayerCount;
    peer->publicPeer.readyMask = readyMask;
    peer->publicPeer.confirmed = confirmed ? 1 : 0;
    peer->publicPeer.lastSeenMs = nowMs;
    peer->publicPeer.packets++;
    peer->publicPeer.hasSecureInfo = secureBits ? 1 : 0;
    peer->publicPeer.launchId = launchId;
    peer->publicPeer.launchAckId = launchAckId;

    if ((secureBits & 1) && xnAddr)
    {
        memcpy(&peer->xnAddr, xnAddr, sizeof(peer->xnAddr));
        peer->hasXnAddr = 1;
    }
    if ((secureBits & 2) && keyId && key)
    {
        memcpy(&peer->sessionKeyId, keyId, sizeof(peer->sessionKeyId));
        memcpy(&peer->sessionKey, key, sizeof(peer->sessionKey));
        peer->hasSessionKey = 1;
    }
    if (entry && phase >= XBOX_SYSTEMLINK_PHASE_MAP_SELECT)
        xboxSystemLinkProbe_CopyEntry(&g_xsl.selectedEntry, entry);

    xboxSystemLinkProbe_UpdateRoster();

    if (g_xsl.role == XBOX_SYSTEMLINK_ROLE_HOST && g_xsl.sessionRegistered && peer->hasXnAddr)
        xboxSystemLinkProbe_ResolvePeerSecureAddress(peer, 0);

    if (isNew)
    {
        xboxSystemLinkProbe_FormatAddress(address, addressText, sizeof(addressText));
        xbox_debug_Printf("XSL peer discovered id=0x%08X addr=%s port=%d game=%d role=%d host=0x%08X phase=%d locals=%d secure=%d\n",
                          id,
                          addressText,
                          port,
                          gamePort,
                          role,
                          hostId,
                          phase,
                          localPlayerCount,
                          secureBits);
    }
}

static void xboxSystemLinkProbe_SendTo(unsigned long address, int port, const char *packet, int packetLen)
{
    struct sockaddr_in to;
    int sent;

    if (!packet || packetLen <= 0 || g_xsl.lobbySocket == INVALID_SOCKET || !port)
        return;

    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = address;
    to.sin_port = htons((u_short)port);

    sent = sendto(g_xsl.lobbySocket, packet, packetLen, 0, (struct sockaddr *)&to, sizeof(to));
    if (sent == SOCKET_ERROR)
    {
        char addressText[32];
        g_xsl.lastError = WSAGetLastError();
        if (g_xslSendTraceBudget > 0)
        {
            xboxSystemLinkProbe_FormatAddress(address, addressText, sizeof(addressText));
            xbox_debug_Printf("XSL send failed addr=%s port=%d err=%d\n",
                              addressText,
                              port,
                              g_xsl.lastError);
            g_xslSendTraceBudget--;
        }
    }
    else if (g_xslSendTraceBudget > 0)
    {
        char addressText[32];
        xboxSystemLinkProbe_FormatAddress(address, addressText, sizeof(addressText));
        xbox_debug_Printf("XSL send ok addr=%s port=%d bytes=%d\n",
                          addressText,
                          port,
                          sent);
        g_xslSendTraceBudget--;
    }
}

static void xboxSystemLinkProbe_Send(void)
{
    char packet[768];
    char xnAddrHex[sizeof(XNADDR) * 2 + 1];
    char keyIdHex[sizeof(XNKID) * 2 + 1];
    char keyHex[sizeof(XNKEY) * 2 + 1];
    int secureBits;
    int packetLen;
    int i;

    if (!g_xsl.started || g_xsl.lobbySocket == INVALID_SOCKET)
        return;

    xboxSystemLinkProbe_UpdateLocalXnAddr();
    if (g_xsl.role == XBOX_SYSTEMLINK_ROLE_HOST)
    {
        xboxSystemLinkProbe_EnsureHostSession();
        xboxSystemLinkProbe_OpenGameSocket();
    }

    xboxSystemLinkProbe_HexZeroes(sizeof(XNADDR), xnAddrHex, sizeof(xnAddrHex));
    xboxSystemLinkProbe_HexZeroes(sizeof(XNKID), keyIdHex, sizeof(keyIdHex));
    xboxSystemLinkProbe_HexZeroes(sizeof(XNKEY), keyHex, sizeof(keyHex));
    secureBits = 0;
    if (g_xsl.hasLocalXnAddr)
    {
        xboxSystemLinkProbe_HexEncode((const unsigned char *)&g_xsl.localXnAddr,
                                      sizeof(g_xsl.localXnAddr),
                                      xnAddrHex,
                                      sizeof(xnAddrHex));
        secureBits |= 1;
    }
    if (g_xsl.role == XBOX_SYSTEMLINK_ROLE_HOST
        && g_xsl.sessionRegistered
        && g_xsl.sessionIsHost)
    {
        xboxSystemLinkProbe_HexEncode((const unsigned char *)&g_xsl.sessionKeyId,
                                      sizeof(g_xsl.sessionKeyId),
                                      keyIdHex,
                                      sizeof(keyIdHex));
        xboxSystemLinkProbe_HexEncode((const unsigned char *)&g_xsl.sessionKey,
                                      sizeof(g_xsl.sessionKey),
                                      keyHex,
                                      sizeof(keyHex));
        secureBits |= 2;
    }

    _snprintf(packet,
              sizeof(packet),
              XSL_PACKET_MAGIC "|%08X|%d|%lu|%d|%08X|%d|%d|%d|%d|%d|%08X|%08X|%d|%d|%d|%d|%d|%s|%s|%d|%s|%s|%s",
              g_xsl.localId,
              g_xsl.localPort,
              g_xsl.sendCounter++,
              g_xsl.role,
              g_xsl.hostId,
              g_xsl.phase,
              g_xsl.localGamePort,
              g_xsl.localPlayerCount,
              g_xsl.readyMask,
              g_xsl.confirmed,
              g_xsl.launchId,
              g_xsl.launchAckId,
              g_xsl.rosterMaxPlayers,
              g_xsl.selectedEntry.multiModeFlags,
              g_xsl.selectedEntry.scoreLimit,
              g_xsl.selectedEntry.timeLimit,
              g_xsl.selectedEntry.maxRank,
              xboxSystemLinkProbe_PacketStringOrDash(g_xsl.selectedEntry.episodeGobName),
              xboxSystemLinkProbe_PacketStringOrDash(g_xsl.selectedEntry.mapJklFname),
              secureBits,
              xnAddrHex,
              keyIdHex,
              keyHex);
    packet[sizeof(packet) - 1] = 0;
    packetLen = (int)strlen(packet);

    for (i = 0; i < XSL_PORT_COUNT; i++)
        xboxSystemLinkProbe_SendTo(INADDR_BROADCAST, XSL_BASE_PORT + i, packet, packetLen);

    for (i = 0; i < g_xsl.peerCount; i++)
    {
        XboxSystemLinkPeerState *peer = &g_xsl.peers[i];
        if (peer->publicPeer.address && peer->publicPeer.port)
            xboxSystemLinkProbe_SendTo(peer->publicPeer.address, peer->publicPeer.port, packet, packetLen);
        if (peer->hasSecureAddress && peer->publicPeer.port)
            xboxSystemLinkProbe_SendTo(peer->secureAddress, peer->publicPeer.port, packet, packetLen);
    }
}

static int xboxSystemLinkProbe_HasPeerAcksForLaunch(void)
{
    int i;

    for (i = 0; i < g_xsl.peerCount; i++)
    {
        if (!g_xsl.peers[i].publicPeer.confirmed)
            continue;
        if (g_xsl.peers[i].publicPeer.launchAckId != g_xsl.launchId)
            return 0;
    }
    return 1;
}

static void xboxSystemLinkProbe_UpdateLaunch(unsigned long nowMs)
{
    if (g_xsl.role == XBOX_SYSTEMLINK_ROLE_HOST
        && g_xsl.phase == XBOX_SYSTEMLINK_PHASE_LAUNCHING
        && g_xsl.launchId
        && g_xsl.launchAckId != g_xsl.launchId)
    {
        if (xboxSystemLinkProbe_HasPeerAcksForLaunch() || nowMs >= g_xsl.launchDeadlineMs)
        {
            g_xsl.launchAckId = g_xsl.launchId;
            g_xsl.pendingTravel = 1;
            g_xsl.pendingTravelIsHost = 1;
            g_xsl.pendingTravelMs = nowMs + XSL_LAUNCH_DELAY_MS;
            xbox_debug_Printf("XSL host launch commit announced launch=0x%08X acks=%d peers=%d\n",
                              g_xsl.launchId,
                              xboxSystemLinkProbe_HasPeerAcksForLaunch(),
                              g_xsl.peerCount);
            xboxSystemLinkProbe_Send();
        }
    }
    else if (g_xsl.role == XBOX_SYSTEMLINK_ROLE_CLIENT && g_xsl.hostId)
    {
        XboxSystemLinkPeerState *host = xboxSystemLinkProbe_FindPeer(g_xsl.hostId);
        unsigned long secureAddr;

        if (!host || host->publicPeer.phase != XBOX_SYSTEMLINK_PHASE_LAUNCHING || !host->publicPeer.launchId)
            return;
        if (host->publicPeer.launchId != g_xsl.launchId)
        {
            if (!xboxSystemLinkProbe_ResolveHostSecureAddress(host, &secureAddr))
            {
                xbox_debug_Printf("XSL client launch waiting secure host=0x%08X secure=%d key=%d\n",
                                  host->publicPeer.id,
                                  host->hasXnAddr,
                                  host->hasSessionKey);
                return;
            }

            g_xsl.launchId = host->publicPeer.launchId;
            g_xsl.launchAckId = g_xsl.launchId;
            g_xsl.phase = XBOX_SYSTEMLINK_PHASE_LAUNCHING;
            xboxSystemLinkProbe_CopyEntry(&g_xsl.selectedEntry, &g_xsl.selectedEntry);
            xbox_debug_Printf("XSL client launch acked launch=0x%08X host=0x%08X secureAddr=0x%08X\n",
                              g_xsl.launchId,
                              g_xsl.hostId,
                              secureAddr);
            xboxSystemLinkProbe_Send();
        }
        if (host->publicPeer.launchAckId == host->publicPeer.launchId
            && g_xsl.lastSeenLaunchId != host->publicPeer.launchId)
        {
            g_xsl.pendingTravel = 1;
            g_xsl.pendingTravelIsHost = 0;
            g_xsl.pendingTravelMs = nowMs + XSL_LAUNCH_DELAY_MS;
            g_xsl.lastSeenLaunchId = host->publicPeer.launchId;
            xbox_debug_Printf("XSL client launch commit received launch=0x%08X host=0x%08X\n",
                              g_xsl.launchId,
                              g_xsl.hostId);
        }
    }
}

void xboxSystemLinkProbe_Stop(void)
{
    xboxSystemLinkProbe_EnsureState();
    xboxSystemLinkProbe_CloseLobbySocket();
    xboxSystemLinkProbe_CloseGameSocket();
    xboxSystemLinkProbe_UnregisterSession("stop");
    xboxSystemLinkProbe_CleanupSockets("stop");
    g_xsl.started = 0;
    g_xsl.gameplayActive = 0;
    g_xsl.lastGameSendMs = 0;
    g_xsl.lastGameReceiveMs = 0;
    g_xsl.lastHealthWarnMs = 0;
    g_xsl.lastHealthWarnCode = XSL_HEALTH_OK;
    g_xsl.phase = XBOX_SYSTEMLINK_PHASE_DISCOVERY;
    g_xsl.peerCount = 0;
    xbox_debug_Print("XSL stopped\n");
}

void xboxSystemLinkProbe_StopForTravel(void)
{
    xboxSystemLinkProbe_EnsureState();
    xboxSystemLinkProbe_CloseLobbySocket();
    g_xsl.started = 0;
    g_xsl.phase = XBOX_SYSTEMLINK_PHASE_IN_GAME;
    xbox_debug_Print("XSL lobby stopped for travel\n");
}

int xboxSystemLinkProbe_Start(void)
{
    xboxSystemLinkProbe_EnsureState();
    if (g_xsl.started)
        return 1;

    if (!xboxSystemLinkProbe_OpenSocketBound(&g_xsl.lobbySocket,
                                             XSL_BASE_PORT,
                                             XSL_PORT_COUNT,
                                             &g_xsl.localPort,
                                             "lobby"))
        return 0;

    if (!g_xsl.localId)
    {
        unsigned long randomId;
        randomId = 0;
        if (XNetRandom((unsigned char *)&randomId, sizeof(randomId)) == 0 && randomId)
            g_xsl.localId = randomId;
        else
            g_xsl.localId = xboxSystemLinkProbe_NowMs() ^ (unsigned long)&g_xsl;
    }
    xboxSystemLinkProbe_UpdateLocalXnAddr();

    g_xsl.started = 1;
    g_xsl.enterMs = xboxSystemLinkProbe_NowMs();
    g_xsl.lastSendMs = 0;
    g_xsl.lastLogMs = 0;
    g_xsl.sendCounter = 0;
    g_xslSendTraceBudget = 12;
    g_xslRecvTraceBudget = 48;
    g_xsl.peerCount = 0;
    g_xsl.role = XBOX_SYSTEMLINK_ROLE_SEEKING;
    g_xsl.lastLoggedRole = -1;
    g_xsl.hostId = 0;
    g_xsl.phase = g_xsl.confirmed ? XBOX_SYSTEMLINK_PHASE_READY : XBOX_SYSTEMLINK_PHASE_DISCOVERY;
    xbox_debug_Printf("XSL lobby started id=0x%08X port=%d localCount=%d xnaddr=0x%08X has=%d\n",
                      g_xsl.localId,
                      g_xsl.localPort,
                      g_xsl.localPlayerCount,
                      g_xsl.localXnAddrStatus,
                      g_xsl.hasLocalXnAddr);
    return 1;
}

void xboxSystemLinkProbe_Tick(void)
{
    unsigned long nowMs;

    xboxSystemLinkProbe_EnsureState();
    if (!g_xsl.started)
        xboxSystemLinkProbe_Start();
    if (!g_xsl.started)
        return;

    nowMs = xboxSystemLinkProbe_NowMs();
    xboxSystemLinkProbe_ExpirePeers(nowMs);
    xboxSystemLinkProbe_UpdateElection(nowMs);

    if (nowMs - g_xsl.lastSendMs >= XSL_SEND_INTERVAL_MS)
    {
        xboxSystemLinkProbe_Send();
        g_xsl.lastSendMs = nowMs;
    }

    for (;;)
    {
        char buffer[768];
        struct sockaddr_in from;
        int fromSize;
        int count;
        unsigned long id;
        int port;
        unsigned long counter;
        int role;
        unsigned long hostId;
        int phase;
        int gamePort;
        int localPlayerCount;
        int readyMask;
        int confirmed;
        unsigned long launchId;
        unsigned long launchAckId;
        int rosterMax;
        int secureBits;
        jkMultiEntry3 entry;
        char xnAddrHex[sizeof(XNADDR) * 2 + 1];
        char keyIdHex[sizeof(XNKID) * 2 + 1];
        char keyHex[sizeof(XNKEY) * 2 + 1];
        XNADDR packetXnAddr;
        XNKID packetKeyId;
        XNKEY packetKey;

        fromSize = sizeof(from);
        count = recvfrom(g_xsl.lobbySocket, buffer, sizeof(buffer) - 1, 0, (struct sockaddr *)&from, &fromSize);
        if (count == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)
                g_xsl.lastError = err;
            break;
        }

        buffer[count] = 0;
        if (g_xslRecvTraceBudget > 0)
        {
            char addressText[32];
            xboxSystemLinkProbe_FormatAddress(from.sin_addr.s_addr, addressText, sizeof(addressText));
            xbox_debug_Printf("XSL recv packet addr=%s bytes=%d head=%.12s\n",
                              addressText,
                              count,
                              buffer);
            g_xslRecvTraceBudget--;
        }
        memset(&entry, 0, sizeof(entry));
        memset(xnAddrHex, 0, sizeof(xnAddrHex));
        memset(keyIdHex, 0, sizeof(keyIdHex));
        memset(keyHex, 0, sizeof(keyHex));
        memset(&packetXnAddr, 0, sizeof(packetXnAddr));
        memset(&packetKeyId, 0, sizeof(packetKeyId));
        memset(&packetKey, 0, sizeof(packetKey));

        if (strncmp(buffer, XSL_PACKET_MAGIC "|", strlen(XSL_PACKET_MAGIC "|")) == 0
            && xboxSystemLinkProbe_ParsePacket(buffer,
                                               &id,
                                               &port,
                                               &counter,
                                               &role,
                                               &hostId,
                                               &phase,
                                               &gamePort,
                                               &localPlayerCount,
                                               &readyMask,
                                               &confirmed,
                                               &launchId,
                                               &launchAckId,
                                               &rosterMax,
                                               &entry,
                                               &secureBits,
                                               xnAddrHex,
                                               sizeof(xnAddrHex),
                                               keyIdHex,
                                               sizeof(keyIdHex),
                                               keyHex,
                                               sizeof(keyHex)))
        {
            if ((secureBits & 1) && !xboxSystemLinkProbe_HexDecode(xnAddrHex, (unsigned char *)&packetXnAddr, sizeof(packetXnAddr)))
                secureBits &= ~1;
            if ((secureBits & 2)
                && (!xboxSystemLinkProbe_HexDecode(keyIdHex, (unsigned char *)&packetKeyId, sizeof(packetKeyId))
                    || !xboxSystemLinkProbe_HexDecode(keyHex, (unsigned char *)&packetKey, sizeof(packetKey))))
            {
                secureBits &= ~2;
            }
            xboxSystemLinkProbe_RecordPeer(id,
                                           from.sin_addr.s_addr,
                                           port,
                                           gamePort,
                                           role,
                                           hostId,
                                           phase,
                                           localPlayerCount,
                                           readyMask,
                                           confirmed,
                                           launchId,
                                           launchAckId,
                                           &entry,
                                           secureBits,
                                           &packetXnAddr,
                                           &packetKeyId,
                                           &packetKey,
                                           nowMs);
            if (g_xslRecvTraceBudget > 0)
            {
                xbox_debug_Printf("XSL recv parsed id=0x%08X local=0x%08X role=%d phase=%d host=0x%08X secure=%d\n",
                                  id,
                                  g_xsl.localId,
                                  role,
                                  phase,
                                  hostId,
                                  secureBits);
                g_xslRecvTraceBudget--;
            }
        }
        else if (sscanf(buffer, "JKXSL1|%08X|%d|%lu", &id, &port, &counter) == 3)
        {
            xboxSystemLinkProbe_RecordPeer(id,
                                           from.sin_addr.s_addr,
                                           port,
                                           0,
                                           XBOX_SYSTEMLINK_ROLE_SEEKING,
                                           0,
                                           XBOX_SYSTEMLINK_PHASE_DISCOVERY,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           0,
                                           nowMs);
        }
        else if (g_xslRecvTraceBudget > 0)
        {
            xbox_debug_Printf("XSL recv ignored parse-failed bytes=%d head=%.32s\n",
                              count,
                              buffer);
            g_xslRecvTraceBudget--;
        }
    }

    xboxSystemLinkProbe_UpdateElection(nowMs);
    xboxSystemLinkProbe_UpdateLaunch(nowMs);

    if (nowMs - g_xsl.lastLogMs >= 5000)
    {
        g_xsl.lastLogMs = nowMs;
        xbox_debug_Printf("XSL lobby status id=0x%08X role=%d phase=%d host=0x%08X port=%d game=%d peers=%d machines=%d confirmed=%d/%d sent=%lu lastErr=%d\n",
                          g_xsl.localId,
                          g_xsl.role,
                          g_xsl.phase,
                          g_xsl.hostId,
                          g_xsl.localPort,
                          g_xsl.localGamePort,
                          g_xsl.peerCount,
                          xboxSystemLinkProbe_GroupMachineCount(),
                          xboxSystemLinkProbe_AllMachinesConfirmed(),
                          xboxSystemLinkProbe_GroupMachineCount(),
                          g_xsl.sendCounter,
                          g_xsl.lastError);
    }
}

void xboxSystemLinkProbe_GetStatus(XboxSystemLinkProbeStatus *outStatus)
{
    int i;

    if (!outStatus)
        return;

    xboxSystemLinkProbe_EnsureState();
    memset(outStatus, 0, sizeof(*outStatus));
    outStatus->started = g_xsl.started;
    outStatus->socketsReady = g_xsl.socketsReady;
    outStatus->localPort = g_xsl.localPort;
    outStatus->localGamePort = g_xsl.localGamePort;
    outStatus->localId = g_xsl.localId;
    outStatus->role = g_xsl.role;
    outStatus->hostId = g_xsl.hostId;
    outStatus->phase = g_xsl.phase;
    outStatus->localPlayerCount = g_xsl.localPlayerCount;
    outStatus->readyMask = g_xsl.readyMask;
    outStatus->confirmed = g_xsl.confirmed;
    outStatus->sessionRegistered = g_xsl.sessionRegistered;
    outStatus->hasLocalXnAddr = g_xsl.hasLocalXnAddr;
    outStatus->localXnAddrStatus = g_xsl.localXnAddrStatus;
    outStatus->hasSecureHostAddress = g_xsl.hasSecureHostAddress;
    outStatus->groupMachineCount = xboxSystemLinkProbe_GroupMachineCount();
    outStatus->allConfirmed = xboxSystemLinkProbe_AllMachinesConfirmed();
    outStatus->localFirstPlayerIndex = g_xsl.localFirstPlayerIndex;
    outStatus->rosterMaxPlayers = g_xsl.rosterMaxPlayers;
    outStatus->launchId = g_xsl.launchId;
    outStatus->launchAckId = g_xsl.launchAckId;
    outStatus->sent = g_xsl.sendCounter;
    outStatus->lastError = g_xsl.lastError;
    outStatus->peerCount = g_xsl.peerCount;
    for (i = 0; i < g_xsl.peerCount && i < XBOX_SYSTEMLINK_PROBE_MAX_PEERS; i++)
        outStatus->peers[i] = g_xsl.peers[i].publicPeer;
}

void xboxSystemLinkProbe_SetLocalReady(int localPlayerCount)
{
    int i;

    xboxSystemLinkProbe_EnsureState();
    if (localPlayerCount < 1)
        localPlayerCount = 1;
    if (localPlayerCount > XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS)
        localPlayerCount = XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS;
    g_xsl.localPlayerCount = localPlayerCount;
    g_xsl.readyMask = 0;
    for (i = 0; i < localPlayerCount; i++)
        g_xsl.readyMask |= (1 << i);
    if (g_xsl.phase == XBOX_SYSTEMLINK_PHASE_DISCOVERY)
        g_xsl.phase = XBOX_SYSTEMLINK_PHASE_READY;
    xboxSystemLinkProbe_UpdateRoster();
    xbox_debug_Printf("XSL local ready locals=%d readyMask=0x%X rosterMax=%d\n",
                      g_xsl.localPlayerCount,
                      g_xsl.readyMask,
                      g_xsl.rosterMaxPlayers);
}

void xboxSystemLinkProbe_SetLocalConfirmed(int confirmed)
{
    xboxSystemLinkProbe_EnsureState();
    g_xsl.confirmed = confirmed ? 1 : 0;
    if (g_xsl.confirmed && g_xsl.phase < XBOX_SYSTEMLINK_PHASE_READY)
        g_xsl.phase = XBOX_SYSTEMLINK_PHASE_READY;
    xbox_debug_Printf("XSL local confirmed=%d phase=%d\n",
                      g_xsl.confirmed,
                      g_xsl.phase);
}

int xboxSystemLinkProbe_IsHost(void)
{
    xboxSystemLinkProbe_EnsureState();
    return g_xsl.role == XBOX_SYSTEMLINK_ROLE_HOST;
}

int xboxSystemLinkProbe_IsClient(void)
{
    xboxSystemLinkProbe_EnsureState();
    return g_xsl.role == XBOX_SYSTEMLINK_ROLE_CLIENT;
}

int xboxSystemLinkProbe_GroupMachineCount(void)
{
    xboxSystemLinkProbe_EnsureState();
    return 1 + g_xsl.peerCount;
}

int xboxSystemLinkProbe_AllMachinesConfirmed(void)
{
    int i;

    xboxSystemLinkProbe_EnsureState();
    if (xboxSystemLinkProbe_GroupMachineCount() < 2)
        return 0;
    if (!g_xsl.confirmed)
        return 0;
    for (i = 0; i < g_xsl.peerCount; i++)
    {
        if (!g_xsl.peers[i].publicPeer.confirmed)
            return 0;
    }
    return 1;
}

int xboxSystemLinkProbe_SmokeHarnessBegin(const jkMultiEntry3 *entry, int isHost, int remoteLocalPlayerCount)
{
    XboxSystemLinkPeerState *peer;
    unsigned long nowMs;
    unsigned long peerId;
    int localCount;

    xboxSystemLinkProbe_EnsureState();
    if (!entry)
        return 0;
    if (!g_xsl.localId)
        g_xsl.localId = xboxSystemLinkProbe_NowMs() ^ (unsigned long)&g_xsl;

    localCount = g_xsl.localPlayerCount;
    if (localCount < 1)
        localCount = 1;
    if (localCount > XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS)
        localCount = XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS;
    if (remoteLocalPlayerCount < 1)
        remoteLocalPlayerCount = 1;
    if (remoteLocalPlayerCount > XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS)
        remoteLocalPlayerCount = XBOX_SYSTEMLINK_MAX_LOCAL_PLAYERS;

    nowMs = xboxSystemLinkProbe_NowMs();
    peerId = g_xsl.localId ^ 0x13572468u;
    if (!peerId || peerId == g_xsl.localId)
        peerId = g_xsl.localId + 1;

    g_xsl.peerCount = 1;
    peer = &g_xsl.peers[0];
    memset(peer, 0, sizeof(*peer));
    peer->publicPeer.id = peerId;
    peer->publicPeer.address = INADDR_BROADCAST;
    peer->publicPeer.port = XSL_BASE_PORT;
    peer->publicPeer.gamePort = XSL_GAME_BASE_PORT;
    peer->publicPeer.role = isHost ? XBOX_SYSTEMLINK_ROLE_CLIENT : XBOX_SYSTEMLINK_ROLE_HOST;
    peer->publicPeer.hostId = isHost ? g_xsl.localId : peerId;
    peer->publicPeer.phase = XBOX_SYSTEMLINK_PHASE_LAUNCHING;
    peer->publicPeer.localPlayerCount = remoteLocalPlayerCount;
    peer->publicPeer.readyMask = (1 << remoteLocalPlayerCount) - 1;
    peer->publicPeer.confirmed = 1;
    peer->publicPeer.machineIndex = isHost ? 1 : 0;
    peer->publicPeer.firstPlayerIndex = peer->publicPeer.machineIndex * XBOX_SYSTEMLINK_PLAYER_STRIDE;
    peer->publicPeer.launchId = 1;
    peer->publicPeer.launchAckId = 1;
    peer->publicPeer.lastSeenMs = nowMs;
    peer->publicPeer.packets = 1;
    peer->firstSeenMs = nowMs;

    xboxSystemLinkProbe_CopyEntry(&g_xsl.selectedEntry, entry);
    g_xsl.localPlayerCount = localCount;
    g_xsl.readyMask = (1 << localCount) - 1;
    g_xsl.confirmed = 1;
    g_xsl.role = isHost ? XBOX_SYSTEMLINK_ROLE_HOST : XBOX_SYSTEMLINK_ROLE_CLIENT;
    g_xsl.hostId = isHost ? g_xsl.localId : peerId;
    g_xsl.localMachineIndex = isHost ? 0 : 1;
    g_xsl.localFirstPlayerIndex = g_xsl.localMachineIndex * XBOX_SYSTEMLINK_PLAYER_STRIDE;
    g_xsl.rosterMaxPlayers = XBOX_SYSTEMLINK_PLAYER_STRIDE * 2;
    if (g_xsl.rosterMaxPlayers > JKPLAYER_NUM_INFOS)
        g_xsl.rosterMaxPlayers = JKPLAYER_NUM_INFOS;
    g_xsl.phase = XBOX_SYSTEMLINK_PHASE_LAUNCHING;
    g_xsl.launchId = 1;
    g_xsl.launchAckId = 1;
    g_xsl.smokeHarness = 1;
    g_xsl.pendingTravel = 1;
    g_xsl.pendingTravelIsHost = isHost ? 1 : 0;
    g_xsl.pendingTravelMs = nowMs;
    g_xsl.launchDeadlineMs = nowMs;

    xbox_debug_Printf("XSL SMOKE harness role=%s local=0x%08X peer=0x%08X localFirst=%d locals=%d remoteLocals=%d rosterMax=%d\n",
                      isHost ? "host" : "client",
                      g_xsl.localId,
                      peerId,
                      g_xsl.localFirstPlayerIndex,
                      g_xsl.localPlayerCount,
                      remoteLocalPlayerCount,
                      g_xsl.rosterMaxPlayers);
    xbox_debug_Printf("XSL peer discovered smoke-harness synthetic id=0x%08X addr=255.255.255.255 port=%d game=%d role=%d locals=%d\n",
                      peerId,
                      peer->publicPeer.port,
                      peer->publicPeer.gamePort,
                      peer->publicPeer.role,
                      peer->publicPeer.localPlayerCount);
    if (isHost)
    {
        xbox_debug_Printf("XSL host launch scheduled smoke-harness launch=0x%08X gob=%s jkl=%s machines=2 locals=%d rosterMax=%d\n",
                          g_xsl.launchId,
                          g_xsl.selectedEntry.episodeGobName,
                          g_xsl.selectedEntry.mapJklFname,
                          g_xsl.localPlayerCount,
                          g_xsl.rosterMaxPlayers);
        xbox_debug_Printf("XSL host launch commit announced smoke-harness launch=0x%08X acks=1 peers=1\n",
                          g_xsl.launchId);
    }
    else
    {
        xbox_debug_Printf("XSL client launch acked smoke-harness launch=0x%08X host=0x%08X\n",
                          g_xsl.launchId,
                          g_xsl.hostId);
        xbox_debug_Printf("XSL client launch commit received smoke-harness launch=0x%08X host=0x%08X\n",
                          g_xsl.launchId,
                          g_xsl.hostId);
    }

    return 1;
}

int xboxSystemLinkProbe_ScheduleHostLaunch(const jkMultiEntry3 *entry)
{
    unsigned long nowMs;

    xboxSystemLinkProbe_EnsureState();
    if (g_xsl.role != XBOX_SYSTEMLINK_ROLE_HOST || !entry)
        return 0;
    if (!xboxSystemLinkProbe_AllMachinesConfirmed())
    {
        xbox_debug_Printf("XSL host launch blocked confirmed=%d machines=%d\n",
                          xboxSystemLinkProbe_AllMachinesConfirmed(),
                          xboxSystemLinkProbe_GroupMachineCount());
        return 0;
    }
    if (!xboxSystemLinkProbe_EnsureHostSession() || !xboxSystemLinkProbe_UpdateLocalXnAddr())
    {
        xbox_debug_Printf("XSL host launch blocked secure session xnaddr=0x%08X\n",
                          g_xsl.localXnAddrStatus);
        return 0;
    }
    if (!xboxSystemLinkProbe_OpenGameSocket())
        return 0;

    nowMs = xboxSystemLinkProbe_NowMs();
    xboxSystemLinkProbe_CopyEntry(&g_xsl.selectedEntry, entry);
    g_xsl.phase = XBOX_SYSTEMLINK_PHASE_LAUNCHING;
    g_xsl.launchId++;
    if (!g_xsl.launchId)
        g_xsl.launchId = 1;
    g_xsl.launchAckId = 0;
    g_xsl.pendingTravel = 0;
    g_xsl.pendingTravelIsHost = 1;
    g_xsl.pendingTravelMs = 0;
    g_xsl.launchDeadlineMs = nowMs + XSL_LAUNCH_DEADLINE_MS;
    xbox_debug_Printf("XSL host launch scheduled launch=0x%08X gob=%s jkl=%s machines=%d locals=%d rosterMax=%d\n",
                      g_xsl.launchId,
                      g_xsl.selectedEntry.episodeGobName,
                      g_xsl.selectedEntry.mapJklFname,
                      xboxSystemLinkProbe_GroupMachineCount(),
                      g_xsl.localPlayerCount,
                      g_xsl.rosterMaxPlayers);
    xboxSystemLinkProbe_Send();
    return 1;
}

int xboxSystemLinkProbe_PollLaunch(jkMultiEntry3 *outEntry, int *outIsHost)
{
    unsigned long nowMs;

    xboxSystemLinkProbe_EnsureState();
    xboxSystemLinkProbe_Tick();
    if (!g_xsl.pendingTravel)
        return 0;
    nowMs = xboxSystemLinkProbe_NowMs();
    if (nowMs < g_xsl.pendingTravelMs)
        return 0;
    if (outEntry)
        xboxSystemLinkProbe_CopyEntry(outEntry, &g_xsl.selectedEntry);
    if (outIsHost)
        *outIsHost = g_xsl.pendingTravelIsHost;
    g_xsl.pendingTravel = 0;
    return 1;
}

int xboxSystemLinkProbe_BeginGameplay(const jkMultiEntry3 *entry, int isHost)
{
    xboxSystemLinkProbe_EnsureState();
    if (entry)
        xboxSystemLinkProbe_CopyEntry(&g_xsl.selectedEntry, entry);
    if (!xboxSystemLinkProbe_OpenGameSocket())
        return 0;
    g_xsl.gameplayActive = 1;
    g_xsl.gameplayIsHost = isHost ? 1 : 0;
    g_xsl.phase = XBOX_SYSTEMLINK_PHASE_IN_GAME;
    g_xsl.lastGameSendMs = 0;
    g_xsl.lastGameReceiveMs = xboxSystemLinkProbe_NowMs();
    g_xsl.lastHealthWarnMs = 0;
    g_xsl.lastHealthWarnCode = XSL_HEALTH_OK;
    xboxSplitScreen_SetFirstPlayerIndex(g_xsl.localFirstPlayerIndex);
    xbox_debug_Printf("XSL gameplay begin host=%d localFirst=%d locals=%d rosterMax=%d gamePort=%d gob=%s jkl=%s\n",
                      g_xsl.gameplayIsHost,
                      g_xsl.localFirstPlayerIndex,
                      g_xsl.localPlayerCount,
                      g_xsl.rosterMaxPlayers,
                      g_xsl.localGamePort,
                      g_xsl.selectedEntry.episodeGobName,
                      g_xsl.selectedEntry.mapJklFname);
    xboxSystemLinkProbe_StopForTravel();
    return 1;
}

int xboxSystemLinkProbe_IsGameplayActive(void)
{
    xboxSystemLinkProbe_EnsureState();
    return g_xsl.gameplayActive;
}

int xboxSystemLinkProbe_IsGameplayHost(void)
{
    xboxSystemLinkProbe_EnsureState();
    return g_xsl.gameplayActive && g_xsl.gameplayIsHost;
}

int xboxSystemLinkProbe_IsSmokeHarness(void)
{
    xboxSystemLinkProbe_EnsureState();
    return g_xsl.gameplayActive && g_xsl.smokeHarness;
}

int xboxSystemLinkProbe_GetLocalFirstPlayerIndex(void)
{
    xboxSystemLinkProbe_EnsureState();
    return g_xsl.localFirstPlayerIndex;
}

int xboxSystemLinkProbe_GetLocalPlayerCount(void)
{
    xboxSystemLinkProbe_EnsureState();
    return g_xsl.localPlayerCount;
}

int xboxSystemLinkProbe_GetRosterMaxPlayers(void)
{
    xboxSystemLinkProbe_EnsureState();
    return g_xsl.rosterMaxPlayers;
}

int xboxSystemLinkProbe_IsLocalPlayerIndex(int playerIndex)
{
    xboxSystemLinkProbe_EnsureState();
    return playerIndex >= g_xsl.localFirstPlayerIndex
        && playerIndex < g_xsl.localFirstPlayerIndex + g_xsl.localPlayerCount;
}

int xboxSystemLinkProbe_NetIdForPlayerIndex(int playerIndex)
{
    if (playerIndex < 0)
        return 0;
    return playerIndex + 1;
}

int xboxSystemLinkProbe_PlayerIndexForNetId(int netId)
{
    if (netId <= 0)
        return -1;
    return netId - 1;
}

static int xboxSystemLinkProbe_FindMachineIdForIndex(int machineIndex, unsigned long *outId)
{
    int i;

    if (machineIndex == g_xsl.localMachineIndex)
    {
        if (outId)
            *outId = g_xsl.localId;
        return 1;
    }
    for (i = 0; i < g_xsl.peerCount; i++)
    {
        if (g_xsl.peers[i].publicPeer.machineIndex == machineIndex)
        {
            if (outId)
                *outId = g_xsl.peers[i].publicPeer.id;
            return 1;
        }
    }
    return 0;
}

static int xboxSystemLinkProbe_IsRosterPlayerActive(int playerIndex)
{
    int machineIndex;
    int slot;
    unsigned long machineId;
    XboxSystemLinkPeerState *peer;

    if (playerIndex < 0 || playerIndex >= g_xsl.rosterMaxPlayers)
        return 0;
    machineIndex = playerIndex / XBOX_SYSTEMLINK_PLAYER_STRIDE;
    slot = playerIndex % XBOX_SYSTEMLINK_PLAYER_STRIDE;
    if (!xboxSystemLinkProbe_FindMachineIdForIndex(machineIndex, &machineId))
        return 0;
    if (machineId == g_xsl.localId)
        return slot < g_xsl.localPlayerCount;
    peer = xboxSystemLinkProbe_FindPeer(machineId);
    return peer && slot < peer->publicPeer.localPlayerCount;
}

void xboxSystemLinkProbe_ApplyRosterAfterStartup(void)
{
    int i;

    xboxSystemLinkProbe_EnsureState();
    if (!g_xsl.gameplayActive)
        return;

    if (g_xsl.rosterMaxPlayers > jkPlayer_maxPlayers)
        jkPlayer_maxPlayers = g_xsl.rosterMaxPlayers;
    if (jkPlayer_maxPlayers > JKPLAYER_NUM_INFOS)
        jkPlayer_maxPlayers = JKPLAYER_NUM_INFOS;

    sithNet_serverNetId = 1;
    for (i = 0; i < jkPlayer_maxPlayers; i++)
    {
        if (!xboxSystemLinkProbe_IsRosterPlayerActive(i))
            continue;
        xbox_debug_Printf("XSL roster slot=%d active local=%d thing=%p sector=%p flags=0x%X net=%d\n",
                          i,
                          xboxSystemLinkProbe_IsLocalPlayerIndex(i),
                          (void*)jkPlayer_playerInfos[i].playerThing,
                          jkPlayer_playerInfos[i].playerThing ? (void*)jkPlayer_playerInfos[i].playerThing->sector : 0,
                          jkPlayer_playerInfos[i].flags,
                          jkPlayer_playerInfos[i].net_id);
        if (!jkPlayer_playerInfos[i].playerThing)
            continue;
        sithPlayer_sub_4C87C0(i, xboxSystemLinkProbe_NetIdForPlayerIndex(i));
        if (!jkPlayer_playerInfos[i].player_name[0])
        {
            jk_snwprintf(jkPlayer_playerInfos[i].player_name, 32, L"System P%d", i + 1);
            jk_snwprintf(jkPlayer_playerInfos[i].multi_name, 32, L"System P%d", i + 1);
        }
    }

    xbox_debug_Printf("XSL roster applied host=%d maxPlayers=%d localFirst=%d locals=%d serverNetId=%d\n",
                      g_xsl.gameplayIsHost,
                      jkPlayer_maxPlayers,
                      g_xsl.localFirstPlayerIndex,
                      g_xsl.localPlayerCount,
                      sithNet_serverNetId);
}

static int xboxSystemLinkProbe_QueueLoopback(int idFrom, const void *data, int len)
{
    int i;

    if (!data || len <= 0 || len > 2052)
        return 0;
    for (i = 0; i < XSL_GAME_QUEUE; i++)
    {
        if (!g_xsl.loopback[i].used)
        {
            g_xsl.loopback[i].used = 1;
            g_xsl.loopback[i].idFrom = idFrom;
            g_xsl.loopback[i].len = len;
            memcpy(g_xsl.loopback[i].data, data, len);
            return 1;
        }
    }
    return 0;
}

static int xboxSystemLinkProbe_PopLoopback(int *pIdOut, int *pMsgIdOut, int *pLenOut)
{
    int i;
    int len;

    for (i = 0; i < XSL_GAME_QUEUE; i++)
    {
        if (!g_xsl.loopback[i].used)
            continue;
        len = g_xsl.loopback[i].len;
        if (pLenOut && len > *pLenOut)
            len = *pLenOut;
        if (pIdOut)
            *pIdOut = g_xsl.loopback[i].idFrom;
        if (pMsgIdOut && len > 0)
            memcpy(pMsgIdOut, g_xsl.loopback[i].data, len);
        if (pLenOut)
            *pLenOut = len;
        g_xsl.loopback[i].used = 0;
        return 0;
    }
    return -1;
}

static int xboxSystemLinkProbe_TargetIsLocal(int netId)
{
    int idx = xboxSystemLinkProbe_PlayerIndexForNetId(netId);
    return xboxSystemLinkProbe_IsLocalPlayerIndex(idx);
}

static int xboxSystemLinkProbe_MachineIndexForNetId(int netId)
{
    int playerIndex = xboxSystemLinkProbe_PlayerIndexForNetId(netId);
    if (playerIndex < 0)
        return -1;
    return playerIndex / XBOX_SYSTEMLINK_PLAYER_STRIDE;
}

static int xboxSystemLinkProbe_GetRemoteEndpointForNetId(int netId, unsigned long *outAddress, int *outPort)
{
    int playerIndex;
    int machineIndex;
    unsigned long machineId;
    XboxSystemLinkPeerState *peer;

    if (outAddress)
        *outAddress = 0;
    if (outPort)
        *outPort = 0;

    playerIndex = xboxSystemLinkProbe_PlayerIndexForNetId(netId);
    if (playerIndex < 0)
        return 0;
    machineIndex = playerIndex / XBOX_SYSTEMLINK_PLAYER_STRIDE;

    if (g_xsl.role == XBOX_SYSTEMLINK_ROLE_CLIENT && machineIndex != g_xsl.localMachineIndex)
    {
        machineId = g_xsl.hostId;
    }
    else if (!xboxSystemLinkProbe_FindMachineIdForIndex(machineIndex, &machineId))
    {
        return 0;
    }

    if (machineId == g_xsl.localId)
        return 0;

    peer = xboxSystemLinkProbe_FindPeer(machineId);
    if (!peer)
        return 0;

    if (g_xsl.sessionRegistered && peer->hasXnAddr)
        xboxSystemLinkProbe_ResolvePeerSecureAddress(peer, 0);
    if (outAddress)
        *outAddress = (g_xsl.smokeHarness && !peer->hasSecureAddress)
            ? INADDR_BROADCAST
            : (peer->hasSecureAddress ? peer->secureAddress : peer->publicPeer.address);
    if (outPort)
        *outPort = peer->publicPeer.gamePort ? peer->publicPeer.gamePort : XSL_GAME_BASE_PORT;
    return outAddress && *outAddress && outPort && *outPort;
}

BOOL xboxSystemLinkProbe_GameSend(DPID idFrom, DPID idTo, void *lpData, DWORD dwDataSize)
{
    unsigned char packet[XSL_MAX_PACKET];
    XboxSystemLinkGamePacketHeader header;
    unsigned long address;
    int port;
    struct sockaddr_in to;
    int sent;

    xboxSystemLinkProbe_EnsureState();
    if (!g_xsl.gameplayActive || !lpData || dwDataSize <= 0 || dwDataSize > 2052)
        return 0;

    if (xboxSystemLinkProbe_TargetIsLocal((int)idTo))
        return xboxSystemLinkProbe_QueueLoopback((int)idFrom, lpData, (int)dwDataSize);

    if (!xboxSystemLinkProbe_OpenGameSocket())
        return 0;
    if (!xboxSystemLinkProbe_GetRemoteEndpointForNetId((int)idTo, &address, &port))
        return 0;

    memset(&header, 0, sizeof(header));
    header.magic = XSL_GAME_MAGIC;
    header.version = XSL_GAME_VERSION;
    header.headerSize = sizeof(header);
    header.idFrom = idFrom;
    header.idTo = idTo;
    header.payloadSize = dwDataSize;
    header.sequence = g_xsl.gameSendCounter++;
    memcpy(packet, &header, sizeof(header));
    memcpy(packet + sizeof(header), lpData, dwDataSize);

    memset(&to, 0, sizeof(to));
    to.sin_family = AF_INET;
    to.sin_addr.s_addr = address;
    to.sin_port = htons((u_short)port);
    sent = sendto(g_xsl.gameSocket,
                  (const char *)packet,
                  sizeof(header) + (int)dwDataSize,
                  0,
                  (struct sockaddr *)&to,
                  sizeof(to));
    if (sent == SOCKET_ERROR)
    {
        g_xsl.lastError = WSAGetLastError();
        return 0;
    }
    g_xsl.lastGameSendMs = xboxSystemLinkProbe_NowMs();
    return 1;
}

int xboxSystemLinkProbe_GameReceive(int *pIdOut, int *pMsgIdOut, int *pLenOut)
{
    int loopRet;
    int scanCount;
    unsigned long nowMs;

    xboxSystemLinkProbe_EnsureState();
    if (!g_xsl.gameplayActive)
        return -1;

    nowMs = xboxSystemLinkProbe_NowMs();
    xboxSystemLinkProbe_TickGameplayHealth(nowMs);

    loopRet = xboxSystemLinkProbe_PopLoopback(pIdOut, pMsgIdOut, pLenOut);
    if (loopRet == 0)
        return 0;

    if (!xboxSystemLinkProbe_OpenGameSocket())
        return -1;

    scanCount = 0;
    for (;;)
    {
        unsigned char packet[XSL_MAX_PACKET];
        XboxSystemLinkGamePacketHeader header;
        struct sockaddr_in from;
        fd_set readSet;
        struct timeval timeout;
        int ready;
        int fromSize;
        int count;
        int payloadLen;

        FD_ZERO(&readSet);
        FD_SET(g_xsl.gameSocket, &readSet);
        timeout.tv_sec = 0;
        timeout.tv_usec = 0;
        ready = select((int)g_xsl.gameSocket + 1, &readSet, NULL, NULL, &timeout);
        if (ready == SOCKET_ERROR)
        {
            g_xsl.lastError = WSAGetLastError();
            return -1;
        }
        if (ready <= 0 || !FD_ISSET(g_xsl.gameSocket, &readSet))
            return -1;

        fromSize = sizeof(from);
        count = recvfrom(g_xsl.gameSocket, (char *)packet, sizeof(packet), 0, (struct sockaddr *)&from, &fromSize);
        if (count == SOCKET_ERROR)
        {
            int err = WSAGetLastError();
            if (err != WSAEWOULDBLOCK)
                g_xsl.lastError = err;
            return -1;
        }
        scanCount++;
        if (count < (int)sizeof(header))
        {
            if (scanCount >= XSL_GAME_RECV_SCAN_LIMIT)
                return -1;
            continue;
        }
        memcpy(&header, packet, sizeof(header));
        if (header.magic != XSL_GAME_MAGIC || header.version != XSL_GAME_VERSION || header.headerSize != sizeof(header))
        {
            if (scanCount >= XSL_GAME_RECV_SCAN_LIMIT)
                return -1;
            continue;
        }
        if (header.payloadSize > 2052 || (int)(sizeof(header) + header.payloadSize) > count)
        {
            if (scanCount >= XSL_GAME_RECV_SCAN_LIMIT)
                return -1;
            continue;
        }
        g_xsl.lastGameReceiveMs = xboxSystemLinkProbe_NowMs();

        if (!xboxSystemLinkProbe_TargetIsLocal((int)header.idTo))
        {
            int fromMachine = xboxSystemLinkProbe_MachineIndexForNetId((int)header.idFrom);
            int toMachine = xboxSystemLinkProbe_MachineIndexForNetId((int)header.idTo);
            if (g_xsl.gameplayIsHost
                && fromMachine >= 0
                && toMachine >= 0
                && fromMachine != g_xsl.localMachineIndex
                && fromMachine != toMachine)
                xboxSystemLinkProbe_GameSend(header.idFrom, header.idTo, packet + sizeof(header), header.payloadSize);
            if (scanCount >= XSL_GAME_RECV_SCAN_LIMIT)
                return -1;
            continue;
        }

        payloadLen = (int)header.payloadSize;
        if (pLenOut && payloadLen > *pLenOut)
            payloadLen = *pLenOut;
        if (pIdOut)
            *pIdOut = (int)header.idFrom;
        if (pMsgIdOut && payloadLen > 0)
            memcpy(pMsgIdOut, packet + sizeof(header), payloadLen);
        if (pLenOut)
            *pLenOut = payloadLen;
        return 0;
    }
}

DPID xboxSystemLinkProbe_GameCreatePlayer(wchar_t *pwName, int flags)
{
    (void)pwName;
    (void)flags;
    xboxSystemLinkProbe_EnsureState();
    return xboxSystemLinkProbe_NetIdForPlayerIndex(g_xsl.localFirstPlayerIndex);
}

int xboxSystemLinkProbe_GameOpenHost(jkMultiEntry *entry)
{
    xboxSystemLinkProbe_EnsureState();
    if (!g_xsl.gameplayActive)
        return 0;
    if (!xboxSystemLinkProbe_OpenGameSocket())
        return 0x80004005;
    if (entry)
        jkPlayer_maxPlayers = g_xsl.rosterMaxPlayers;
    stdComm_bIsServer = g_xsl.gameplayIsHost ? 1 : 0;
    return 0;
}

int xboxSystemLinkProbe_GameOpenClient(int idx)
{
    (void)idx;
    xboxSystemLinkProbe_EnsureState();
    if (!g_xsl.gameplayActive)
        return 0;
    if (!xboxSystemLinkProbe_OpenGameSocket())
        return 0x80004005;
    stdComm_dword_8321E8 = 0;
    stdComm_dword_8321E0 = 1;
    stdComm_bIsServer = 0;
    stdComm_dplayIdSelf = xboxSystemLinkProbe_NetIdForPlayerIndex(g_xsl.localFirstPlayerIndex);
    jkGuiMultiplayer_checksumSeed = g_xsl.selectedEntry.field_0;
    return 0;
}

void xboxSystemLinkProbe_GameClose(void)
{
    xboxSystemLinkProbe_EnsureState();
    if (!g_xsl.gameplayActive)
        return;
    g_xsl.gameplayActive = 0;
    g_xsl.gameplayIsHost = 0;
    g_xsl.smokeHarness = 0;
    g_xsl.lastGameSendMs = 0;
    g_xsl.lastGameReceiveMs = 0;
    g_xsl.lastHealthWarnMs = 0;
    g_xsl.lastHealthWarnCode = XSL_HEALTH_OK;
    xboxSystemLinkProbe_CloseGameSocket();
    xboxSystemLinkProbe_UnregisterSession("game close");
    if (!g_xsl.started)
        xboxSystemLinkProbe_CleanupSockets("game close");
}

void xboxSystemLinkProbe_GameEnumPlayers(void)
{
    int i;

    xboxSystemLinkProbe_EnsureState();
    DirectPlay_numPlayers = 0;
    if (!g_xsl.gameplayActive)
        return;
    for (i = 0; i < g_xsl.rosterMaxPlayers && DirectPlay_numPlayers < 32; i++)
    {
        if (!xboxSystemLinkProbe_IsRosterPlayerActive(i))
            continue;
        DirectPlay_aPlayers[DirectPlay_numPlayers].dpId = xboxSystemLinkProbe_NetIdForPlayerIndex(i);
        jk_snwprintf(DirectPlay_aPlayers[DirectPlay_numPlayers].waName, 32, L"System P%d", i + 1);
        DirectPlay_numPlayers++;
    }
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
void xboxSystemLinkProbe_StopForTravel(void) {}
void xboxSystemLinkProbe_SetLocalReady(int localPlayerCount) { (void)localPlayerCount; }
void xboxSystemLinkProbe_SetLocalConfirmed(int confirmed) { (void)confirmed; }
int xboxSystemLinkProbe_IsHost(void) { return 0; }
int xboxSystemLinkProbe_IsClient(void) { return 0; }
int xboxSystemLinkProbe_GroupMachineCount(void) { return 1; }
int xboxSystemLinkProbe_AllMachinesConfirmed(void) { return 0; }
int xboxSystemLinkProbe_SmokeHarnessBegin(const jkMultiEntry3 *entry, int isHost, int remoteLocalPlayerCount) { (void)entry; (void)isHost; (void)remoteLocalPlayerCount; return 0; }
int xboxSystemLinkProbe_ScheduleHostLaunch(const jkMultiEntry3 *entry) { (void)entry; return 0; }
int xboxSystemLinkProbe_PollLaunch(jkMultiEntry3 *outEntry, int *outIsHost) { (void)outEntry; (void)outIsHost; return 0; }
int xboxSystemLinkProbe_BeginGameplay(const jkMultiEntry3 *entry, int isHost) { (void)entry; (void)isHost; return 0; }
int xboxSystemLinkProbe_IsGameplayActive(void) { return 0; }
int xboxSystemLinkProbe_IsGameplayHost(void) { return 0; }
int xboxSystemLinkProbe_IsSmokeHarness(void) { return 0; }
int xboxSystemLinkProbe_GetLocalFirstPlayerIndex(void) { return 0; }
int xboxSystemLinkProbe_GetLocalPlayerCount(void) { return 1; }
int xboxSystemLinkProbe_GetRosterMaxPlayers(void) { return 1; }
int xboxSystemLinkProbe_IsLocalPlayerIndex(int playerIndex) { return playerIndex == 0; }
int xboxSystemLinkProbe_NetIdForPlayerIndex(int playerIndex) { return playerIndex + 1; }
int xboxSystemLinkProbe_PlayerIndexForNetId(int netId) { return netId - 1; }
void xboxSystemLinkProbe_ApplyRosterAfterStartup(void) {}
int xboxSystemLinkProbe_GameReceive(int *pIdOut, int *pMsgIdOut, int *pLenOut) { (void)pIdOut; (void)pMsgIdOut; (void)pLenOut; return -1; }
BOOL xboxSystemLinkProbe_GameSend(DPID idFrom, DPID idTo, void *lpData, DWORD dwDataSize) { (void)idFrom; (void)idTo; (void)lpData; (void)dwDataSize; return 0; }
DPID xboxSystemLinkProbe_GameCreatePlayer(wchar_t *pwName, int flags) { (void)pwName; (void)flags; return 1; }
int xboxSystemLinkProbe_GameOpenHost(jkMultiEntry *entry) { (void)entry; return 0; }
int xboxSystemLinkProbe_GameOpenClient(int idx) { (void)idx; return 0; }
void xboxSystemLinkProbe_GameClose(void) {}
void xboxSystemLinkProbe_GameEnumPlayers(void) {}
#endif
