/*
 * stdSound_xbox.c  -  OpenJKDF2 Xbox audio
 * Location:  src/Platform/Xbox/stdSound_xbox.c
 *
 * Compiled as C (not C++).
 * CINTERFACE is defined before dsound.h so the XDK exposes flat C macros
 * (IDirectSoundBuffer_Stop, etc.) that match the symbols in dsound.lib.
 */

/* CINTERFACE must come before any DirectSound header */
#define CINTERFACE

#include "platform_xbox.h"
#include "xbox_debug.h"
#include <xtl.h>
#include <dsound.h>
#include "types.h"
#include "Win95/stdSound.h"
#include "Gui/jkGUISound.h"
#include "globals.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

#define MAX_DS_BUFFERS 128

typedef struct
{
    stdSound_buffer_t  *pBuf;
    IDirectSoundBuffer *pDS;
    int                 bPlaying;
    int                 bLooping;
    int                 b3D;
} XboxDSEntry;

static XboxDSEntry  g_dsTable[MAX_DS_BUFFERS];
static int          g_dsCount = 0;
static IDirectSound *g_pDS    = NULL;

float stdSound_fMenuVolume = 1.0f;

typedef struct XboxPCMStream
{
    IDirectSoundBuffer *pDS;
    DWORD bufferBytes;
    DWORD writePos;
    DWORD queuedBytes;
    DWORD prefillBytes;
    DWORD blockAlign;
    DWORD bytesPerSec;
    DWORD lastPlayPos;
    uint64_t totalSubmittedBytes;
    uint64_t totalPlayedBytes;
    int playPosValid;
    int started;
    int bStereo;
    unsigned int sampleRate;
    unsigned short bitsPerSample;
    float volume;
    unsigned int writeCalls;
    unsigned int fullWrites;
    unsigned int partialWrites;
    unsigned int lowWaterMarks;
    unsigned int droppedWrites;
    unsigned int silenceWrites;
} XboxPCMStream;

static XboxPCMStream g_pcmStream;

IDirectSound *stdSound_XboxGetDirectSound(void)
{
    return g_pDS;
}

unsigned int stdSound_ParseWav(stdFile_t sound_file, unsigned int *nSamplesPerSec,
                               int *bitsPerSample, int *bStereo, int *seekOffset)
{
    unsigned int dataBytes;
    char tag[4];
    stdWaveFormat fmt;
    unsigned int chunkSize;

    if (!std_pHS || !sound_file || !nSamplesPerSec || !bitsPerSample || !bStereo || !seekOffset)
        return 0;

    std_pHS->fseek(sound_file, 8, 0);
    if (std_pHS->fileRead(sound_file, tag, 4) != 4 || memcmp(tag, "WAVE", 4) != 0)
        return 0;

    std_pHS->fseek(sound_file, 4, 1);
    if (std_pHS->fileRead(sound_file, &chunkSize, 4) != 4)
        return 0;
    if (std_pHS->fileRead(sound_file, &fmt, sizeof(stdWaveFormat)) != sizeof(stdWaveFormat))
        return 0;

    if (!fmt.nChannels || !fmt.nBlockAlign)
        return 0;

    *nSamplesPerSec = fmt.nSamplesPerSec;
    *bitsPerSample = 8 * (fmt.nBlockAlign / (int)fmt.nChannels);
    *bStereo = fmt.nChannels == 2;

    if (chunkSize > 0x10)
        std_pHS->fseek(sound_file, chunkSize - 16, 1);

    dataBytes = 0;
    while (!std_pHS->fileEof(sound_file))
    {
        if (std_pHS->fileRead(sound_file, tag, 4) != 4)
            break;
        if (std_pHS->fileRead(sound_file, &dataBytes, 4) != 4)
            break;
        if (memcmp(tag, "data", 4) == 0)
        {
            *seekOffset = std_pHS->ftell(sound_file);
            return dataBytes;
        }
        std_pHS->fseek(sound_file, dataBytes, 1);
    }

    return 0;
}

static XboxDSEntry *xbox_DSFind(stdSound_buffer_t *p)
{
    int i;
    for (i = 0; i < g_dsCount; i++)
        if (g_dsTable[i].pBuf == p) return &g_dsTable[i];
    return NULL;
}

static XboxDSEntry *xbox_DSAlloc(stdSound_buffer_t *p)
{
    XboxDSEntry *e = xbox_DSFind(p);
    if (e) return e;
    if (g_dsCount >= MAX_DS_BUFFERS) return NULL;
    e = &g_dsTable[g_dsCount++];
    memset(e, 0, sizeof(XboxDSEntry));
    e->pBuf = p;
    return e;
}

static void xbox_DSFree(stdSound_buffer_t *p)
{
    int i;
    for (i = 0; i < g_dsCount; i++)
    {
        if (g_dsTable[i].pBuf == p)
        {
            if (g_dsTable[i].pDS)
            {
                IDirectSoundBuffer_Stop(g_dsTable[i].pDS);
                IDirectSoundBuffer_Release(g_dsTable[i].pDS);
            }
            g_dsTable[i] = g_dsTable[--g_dsCount];
            return;
        }
    }
}

static int xbox_DSCreateBuffer(stdSound_buffer_t *sound, XboxDSEntry *e, int want3D)
{
    DSBUFFERDESC desc;
    WAVEFORMATEX wfx;
    HRESULT hr;
    void *pLock = NULL;
    DWORD lockSz = 0;
    int use3D;

    if (!sound || !e || !g_pDS)
        return 0;

    use3D = want3D && !sound->bStereo;

    if (e->pDS)
    {
        IDirectSoundBuffer_Stop(e->pDS);
        IDirectSoundBuffer_Release(e->pDS);
        e->pDS = NULL;
    }

    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = (WORD)(sound->bStereo ? 2 : 1);
    wfx.nSamplesPerSec  = sound->nSamplesPerSec;
    wfx.wBitsPerSample  = (WORD)sound->bitsPerSample;
    wfx.nBlockAlign     = (WORD)(wfx.nChannels * wfx.wBitsPerSample / 8);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    memset(&desc, 0, sizeof(desc));
    desc.dwSize        = sizeof(desc);
    desc.dwFlags       = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY;
    if (use3D)
        desc.dwFlags |= DSBCAPS_CTRL3D;
    desc.dwBufferBytes = (DWORD)sound->bufferBytes;
    desc.lpwfxFormat   = &wfx;

    hr = IDirectSound_CreateSoundBuffer(g_pDS, &desc, &e->pDS, NULL);
    if (FAILED(hr))
    {
        XDBGF("stdSound_XboxCreateBuffer: failed 0x%X stereo=%d rate=%u bits=%u bytes=%d 3d=%d\n",
              hr, sound->bStereo, sound->nSamplesPerSec, sound->bitsPerSample,
              sound->bufferBytes, use3D);
        e->pDS = NULL;
        e->b3D = 0;
        return 0;
    }

    e->b3D = use3D;
    e->bPlaying = 0;
    e->bLooping = 0;

    if (sound->data && sound->bufferBytes > 0)
    {
        if (SUCCEEDED(IDirectSoundBuffer_Lock(e->pDS, 0, (DWORD)sound->bufferBytes, &pLock, &lockSz, NULL, NULL, 0)))
        {
            memcpy(pLock, sound->data, lockSz);
            IDirectSoundBuffer_Unlock(e->pDS, pLock, lockSz, NULL, 0);
        }
    }

    return 1;
}

static LONG xbox_VolToDS(float v)
{
    if (v <= 0.0f) return -10000;
    if (v >= 1.0f) return 0;
    return (LONG)(2000.0f * (float)log10((double)v));
}

static DWORD xbox_StreamDistance(DWORD from, DWORD to, DWORD size)
{
    return (to >= from) ? (to - from) : (size - from + to);
}

static void xbox_StreamUpdateQueued(void)
{
    DWORD play = 0;
    DWORD write = 0;
    DWORD played;

    if (!g_pcmStream.pDS || !g_pcmStream.started)
        return;

    if (FAILED(IDirectSoundBuffer_GetCurrentPosition(g_pcmStream.pDS, &play, &write)))
        return;

    if (!g_pcmStream.playPosValid)
    {
        g_pcmStream.lastPlayPos = play;
        g_pcmStream.playPosValid = 1;
        return;
    }

    played = xbox_StreamDistance(g_pcmStream.lastPlayPos, play, g_pcmStream.bufferBytes);
    g_pcmStream.lastPlayPos = play;

    if (played >= g_pcmStream.queuedBytes)
    {
        g_pcmStream.totalPlayedBytes += g_pcmStream.queuedBytes;
        g_pcmStream.queuedBytes = 0;
    }
    else
    {
        g_pcmStream.totalPlayedBytes += played;
        g_pcmStream.queuedBytes -= played;
    }
}

static int xbox_StreamWriteSilence(DWORD offset, DWORD bytes)
{
    void *p1 = NULL;
    void *p2 = NULL;
    DWORD s1 = 0;
    DWORD s2 = 0;

    if (!g_pcmStream.pDS || !bytes)
        return 0;

    if (FAILED(IDirectSoundBuffer_Lock(g_pcmStream.pDS, offset, bytes, &p1, &s1, &p2, &s2, 0)))
        return 0;

    if (p1 && s1) memset(p1, 0, s1);
    if (p2 && s2) memset(p2, 0, s2);
    IDirectSoundBuffer_Unlock(g_pcmStream.pDS, p1, s1, p2, s2);
    return 1;
}

void stdSound_XboxStreamClose(void)
{
    if (g_pcmStream.pDS)
    {
        IDirectSoundBuffer_Stop(g_pcmStream.pDS);
        IDirectSoundBuffer_Release(g_pcmStream.pDS);
    }
    memset(&g_pcmStream, 0, sizeof(g_pcmStream));
}

int stdSound_XboxStreamOpen(int bStereo, unsigned int sampleRate,
                            unsigned short bitsPerSample, unsigned int bufferBytes,
                            float volume)
{
    DSBUFFERDESC desc;
    WAVEFORMATEX wfx;
    HRESULT hr;
    DWORD blockAlign;

    stdSound_XboxStreamClose();

    if (!g_pDS || !sampleRate || !bitsPerSample)
        return 0;

    blockAlign = (DWORD)((bStereo ? 2 : 1) * (bitsPerSample / 8));
    if (!blockAlign)
        return 0;

    if (bufferBytes < 0x4000)
        bufferBytes = 0x4000;
    bufferBytes -= bufferBytes % blockAlign;

    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag      = WAVE_FORMAT_PCM;
    wfx.nChannels       = (WORD)(bStereo ? 2 : 1);
    wfx.nSamplesPerSec  = sampleRate;
    wfx.wBitsPerSample  = bitsPerSample;
    wfx.nBlockAlign     = (WORD)blockAlign;
    wfx.nAvgBytesPerSec = sampleRate * blockAlign;

    memset(&desc, 0, sizeof(desc));
    desc.dwSize        = sizeof(desc);
    desc.dwFlags       = DSBCAPS_CTRLVOLUME | DSBCAPS_CTRLFREQUENCY;
    desc.dwBufferBytes = bufferBytes;
    desc.lpwfxFormat   = &wfx;

    hr = IDirectSound_CreateSoundBuffer(g_pDS, &desc, &g_pcmStream.pDS, NULL);
    if (FAILED(hr))
    {
        XDBGF("stdSound_XboxStreamOpen: CreateSoundBuffer failed 0x%X\n", hr);
        memset(&g_pcmStream, 0, sizeof(g_pcmStream));
        return 0;
    }

    g_pcmStream.bufferBytes   = bufferBytes;
    g_pcmStream.writePos      = 0;
    g_pcmStream.queuedBytes   = 0;
    g_pcmStream.blockAlign    = blockAlign;
    g_pcmStream.bytesPerSec   = sampleRate * blockAlign;
    g_pcmStream.prefillBytes  = g_pcmStream.bytesPerSec / 25;
    g_pcmStream.prefillBytes -= g_pcmStream.prefillBytes % blockAlign;
    if (g_pcmStream.prefillBytes < blockAlign * 4)
        g_pcmStream.prefillBytes = blockAlign * 4;
    if (g_pcmStream.prefillBytes > bufferBytes / 2)
        g_pcmStream.prefillBytes = bufferBytes / 2;
    g_pcmStream.started       = 0;
    g_pcmStream.bStereo       = bStereo;
    g_pcmStream.sampleRate    = sampleRate;
    g_pcmStream.bitsPerSample = bitsPerSample;
    g_pcmStream.volume        = volume;

    xbox_StreamWriteSilence(0, bufferBytes);
    IDirectSoundBuffer_SetCurrentPosition(g_pcmStream.pDS, 0);
    IDirectSoundBuffer_SetVolume(g_pcmStream.pDS, xbox_VolToDS(volume * stdSound_fMenuVolume));
    XDBGF("stdSound_XboxStreamOpen[v7-audioclock]: ch=%u rate=%u bits=%u buf=%lu prefill=%lu\n",
          (unsigned)wfx.nChannels, sampleRate, (unsigned)bitsPerSample,
          (unsigned long)bufferBytes, (unsigned long)g_pcmStream.prefillBytes);
    return 1;
}

int stdSound_XboxStreamWrite(const void *data, unsigned int bytes)
{
    const unsigned char *src = (const unsigned char *)data;
    void *p1 = NULL;
    void *p2 = NULL;
    DWORD s1 = 0;
    DWORD s2 = 0;
    DWORD writable;
    DWORD requested;

    if (!g_pcmStream.pDS || !src || !bytes)
        return 0;

    bytes -= bytes % g_pcmStream.blockAlign;
    if (!bytes)
        return 0;
    requested = bytes;
    g_pcmStream.writeCalls++;

    xbox_StreamUpdateQueued();
    if (g_pcmStream.started && g_pcmStream.queuedBytes < g_pcmStream.prefillBytes / 2)
    {
        g_pcmStream.lowWaterMarks++;
        if (g_pcmStream.lowWaterMarks <= 8 || (g_pcmStream.lowWaterMarks % 120) == 0)
        {
            XDBGF("stdSound_XboxStreamWrite: low queued=%lu write=%u\n",
                  (unsigned long)g_pcmStream.queuedBytes, g_pcmStream.writeCalls);
        }
    }

    if (g_pcmStream.queuedBytes >= g_pcmStream.bufferBytes - g_pcmStream.blockAlign)
        writable = 0;
    else
        writable = g_pcmStream.bufferBytes - g_pcmStream.queuedBytes - g_pcmStream.blockAlign;
    if (bytes > writable)
        bytes = writable - (writable % g_pcmStream.blockAlign);
    if (!bytes)
    {
        g_pcmStream.fullWrites++;
        if (g_pcmStream.fullWrites <= 8 || (g_pcmStream.fullWrites % 120) == 0)
        {
            XDBGF("stdSound_XboxStreamWrite: full queued=%lu req=%lu full=%u\n",
                  (unsigned long)g_pcmStream.queuedBytes, (unsigned long)requested,
                  g_pcmStream.fullWrites);
        }
        return 0;
    }
    if (bytes < requested)
    {
        g_pcmStream.partialWrites++;
        if (g_pcmStream.partialWrites <= 8 || (g_pcmStream.partialWrites % 120) == 0)
        {
            XDBGF("stdSound_XboxStreamWrite: partial wrote=%lu req=%lu queued=%lu partial=%u\n",
                  (unsigned long)bytes, (unsigned long)requested,
                  (unsigned long)g_pcmStream.queuedBytes, g_pcmStream.partialWrites);
        }
    }

    if (FAILED(IDirectSoundBuffer_Lock(g_pcmStream.pDS, g_pcmStream.writePos, bytes,
                                       &p1, &s1, &p2, &s2, 0)))
        return 0;

    if (p1 && s1) memcpy(p1, src, s1);
    if (p2 && s2) memcpy(p2, src + s1, s2);
    IDirectSoundBuffer_Unlock(g_pcmStream.pDS, p1, s1, p2, s2);

    g_pcmStream.writePos = (g_pcmStream.writePos + bytes) % g_pcmStream.bufferBytes;
    g_pcmStream.queuedBytes += bytes;
    g_pcmStream.totalSubmittedBytes += bytes;
    if (g_pcmStream.queuedBytes > g_pcmStream.bufferBytes)
        g_pcmStream.queuedBytes = g_pcmStream.bufferBytes;

    if (!g_pcmStream.started && g_pcmStream.queuedBytes >= g_pcmStream.prefillBytes)
    {
        IDirectSoundBuffer_SetCurrentPosition(g_pcmStream.pDS, 0);
        IDirectSoundBuffer_SetVolume(g_pcmStream.pDS, xbox_VolToDS(g_pcmStream.volume * stdSound_fMenuVolume));
        g_pcmStream.started = SUCCEEDED(IDirectSoundBuffer_Play(g_pcmStream.pDS, 0, 0, DSBPLAY_LOOPING)) ? 1 : 0;
        g_pcmStream.lastPlayPos = 0;
        g_pcmStream.playPosValid = g_pcmStream.started;
        XDBGF("stdSound_XboxStreamWrite: start queued=%lu ok=%d\n",
              (unsigned long)g_pcmStream.queuedBytes, g_pcmStream.started);
    }

    return (int)bytes;
}

int stdSound_XboxStreamWriteMaxLatency(const void *data, unsigned int bytes, unsigned int maxQueuedBytes)
{
    const unsigned char *src = (const unsigned char *)data;
    DWORD writable;
    DWORD room;
    int written;

    if (!g_pcmStream.pDS || !src || !bytes)
        return 0;

    bytes -= bytes % g_pcmStream.blockAlign;
    maxQueuedBytes -= maxQueuedBytes % g_pcmStream.blockAlign;
    if (!bytes || !maxQueuedBytes)
        return 0;

    if (maxQueuedBytes > g_pcmStream.bufferBytes - g_pcmStream.blockAlign)
        maxQueuedBytes = g_pcmStream.bufferBytes - g_pcmStream.blockAlign;

    xbox_StreamUpdateQueued();

    if (g_pcmStream.queuedBytes >= maxQueuedBytes)
    {
        return 0;
    }

    room = maxQueuedBytes - g_pcmStream.queuedBytes;
    if (bytes > room)
    {
        DWORD drop = bytes - room;
        drop -= drop % g_pcmStream.blockAlign;
        if (drop)
        {
            bytes -= drop;
            g_pcmStream.droppedWrites++;
            if (0)
            {
                XDBGF("stdSound_XboxStreamWriteMaxLatency: defer-tail keep=%lu write=%lu queued=%lu max=%lu defers=%u\n",
                      (unsigned long)drop, (unsigned long)bytes,
                      (unsigned long)g_pcmStream.queuedBytes, (unsigned long)maxQueuedBytes,
                      g_pcmStream.droppedWrites);
            }
        }
    }

    if (g_pcmStream.queuedBytes >= g_pcmStream.bufferBytes - g_pcmStream.blockAlign)
        writable = 0;
    else
        writable = g_pcmStream.bufferBytes - g_pcmStream.queuedBytes - g_pcmStream.blockAlign;
    if (bytes > writable)
        bytes = writable - (writable % g_pcmStream.blockAlign);
    if (!bytes)
        return 0;

    written = stdSound_XboxStreamWrite(src, bytes);
    return written;
}

int stdSound_XboxStreamMaintainSilence(unsigned int maxQueuedBytes)
{
    void *p1 = NULL;
    void *p2 = NULL;
    DWORD s1 = 0;
    DWORD s2 = 0;
    DWORD bytes;
    DWORD writable;

    if (!g_pcmStream.pDS || !g_pcmStream.started || !maxQueuedBytes)
        return 0;

    maxQueuedBytes -= maxQueuedBytes % g_pcmStream.blockAlign;
    if (!maxQueuedBytes)
        return 0;

    if (maxQueuedBytes > g_pcmStream.bufferBytes - g_pcmStream.blockAlign)
        maxQueuedBytes = g_pcmStream.bufferBytes - g_pcmStream.blockAlign;

    xbox_StreamUpdateQueued();
    if (g_pcmStream.queuedBytes >= maxQueuedBytes)
        return 0;

    bytes = maxQueuedBytes - g_pcmStream.queuedBytes;
    bytes -= bytes % g_pcmStream.blockAlign;
    if (!bytes)
        return 0;

    if (g_pcmStream.queuedBytes >= g_pcmStream.bufferBytes - g_pcmStream.blockAlign)
        writable = 0;
    else
        writable = g_pcmStream.bufferBytes - g_pcmStream.queuedBytes - g_pcmStream.blockAlign;
    if (bytes > writable)
        bytes = writable - (writable % g_pcmStream.blockAlign);
    if (!bytes)
        return 0;

    if (FAILED(IDirectSoundBuffer_Lock(g_pcmStream.pDS, g_pcmStream.writePos, bytes,
                                       &p1, &s1, &p2, &s2, 0)))
        return 0;

    if (p1 && s1) memset(p1, 0, s1);
    if (p2 && s2) memset(p2, 0, s2);
    IDirectSoundBuffer_Unlock(g_pcmStream.pDS, p1, s1, p2, s2);

    g_pcmStream.writePos = (g_pcmStream.writePos + bytes) % g_pcmStream.bufferBytes;
    g_pcmStream.queuedBytes += bytes;
    g_pcmStream.totalSubmittedBytes += bytes;
    if (g_pcmStream.queuedBytes > g_pcmStream.bufferBytes)
        g_pcmStream.queuedBytes = g_pcmStream.bufferBytes;

    g_pcmStream.silenceWrites++;
    if (g_pcmStream.silenceWrites <= 8 || (g_pcmStream.silenceWrites % 120) == 0)
    {
        XDBGF("stdSound_XboxStreamMaintainSilence: wrote=%lu queued=%lu max=%lu sil=%u\n",
              (unsigned long)bytes, (unsigned long)g_pcmStream.queuedBytes,
              (unsigned long)maxQueuedBytes, g_pcmStream.silenceWrites);
    }

    return (int)bytes;
}

uint64_t stdSound_XboxStreamGetPlayedUs(void)
{
    if (!g_pcmStream.pDS || !g_pcmStream.bytesPerSec)
        return 0;

    xbox_StreamUpdateQueued();
    return (uint64_t)((g_pcmStream.totalPlayedBytes * 1000000ULL) / g_pcmStream.bytesPerSec);
}

int stdSound_XboxStreamPause(int pause)
{
    if (!g_pcmStream.pDS)
        return 0;

    if (pause)
    {
        xbox_StreamUpdateQueued();
        IDirectSoundBuffer_Stop(g_pcmStream.pDS);
        g_pcmStream.started = 0;
        g_pcmStream.playPosValid = 0;
        return 1;
    }

    if (g_pcmStream.queuedBytes >= g_pcmStream.blockAlign)
    {
        g_pcmStream.started = SUCCEEDED(IDirectSoundBuffer_Play(g_pcmStream.pDS, 0, 0, DSBPLAY_LOOPING)) ? 1 : 0;
        g_pcmStream.playPosValid = 0;
        return g_pcmStream.started;
    }

    return 1;
}

static void xbox_DS3DToXbox(float *x, float *y, float *z, const rdVector3 *in)
{
    *x = in->x * 0.1f;
    *y = in->y * 0.1f;
    *z = in->z * 0.1f;
}

int stdSound_Startup(void)
{
    HRESULT hr = DirectSoundCreate(NULL, &g_pDS, NULL);
    if (FAILED(hr)) { XDBGF("stdSound_Startup: failed 0x%X\n", hr); return 0; }
    jkGuiSound_b3DSound_3 = 1;
    jkGuiSound_b3DSound = 1;
    jkGuiSound_b3DSound_2 = 1;
    IDirectSound_SetDistanceFactor(g_pDS, 1.0f, DS3D_IMMEDIATE);
    IDirectSound_SetRolloffFactor(g_pDS, 1.0f, DS3D_IMMEDIATE);
    IDirectSound_SetDopplerFactor(g_pDS, 1.0f, DS3D_IMMEDIATE);
    XDBG("stdSound_Startup: DirectSound ready\n");
    return 1;
}

void stdSound_Shutdown(void)
{
    int i;
    stdSound_XboxStreamClose();
    for (i = 0; i < g_dsCount; i++)
    {
        if (g_dsTable[i].pDS)
        {
            IDirectSoundBuffer_Stop(g_dsTable[i].pDS);
            IDirectSoundBuffer_Release(g_dsTable[i].pDS);
        }
    }
    g_dsCount = 0;
    if (g_pDS) { IDirectSound_Release(g_pDS); g_pDS = NULL; }
    XDBG("stdSound_Shutdown\n");
}

stdSound_buffer_t *stdSound_BufferCreate(int bStereo, unsigned int nSamplesPerSec,
                                          unsigned short bitsPerSample, int bufferLen)
{
    stdSound_buffer_t *buf = (stdSound_buffer_t*)malloc(sizeof(stdSound_buffer_t));
    if (!buf) return NULL;
    memset(buf, 0, sizeof(stdSound_buffer_t));
    buf->bStereo        = bStereo;
    buf->nSamplesPerSec = nSamplesPerSec;
    buf->bitsPerSample  = bitsPerSample;
    buf->bufferLen      = bufferLen;
    buf->vol            = 1.0f;
    buf->refcnt         = 1;
    return buf;
}

void *stdSound_BufferSetData(stdSound_buffer_t *sound, int bufferBytes, int *bufferMaxSize)
{
    XboxDSEntry    *e;

    if (!sound) return NULL;
    if (sound->data) free(sound->data);
    sound->data        = malloc(bufferBytes);
    sound->bufferBytes = bufferBytes;
    if (bufferMaxSize) *bufferMaxSize = bufferBytes;
    if (!sound->data || !g_pDS) return sound->data;

    e = xbox_DSAlloc(sound);
    if (!e) return sound->data;

    xbox_DSCreateBuffer(sound, e, 0);
    return sound->data;
}

int stdSound_BufferUnlock(stdSound_buffer_t *sound, void *buffer, int bufferReadLen)
{
    XboxDSEntry *e;
    void *pLock = NULL;
    DWORD lockSz = 0;
    (void)buffer;
    if (!sound || !sound->data) return 0;
    if (bufferReadLen <= 0 || bufferReadLen > sound->bufferBytes)
        bufferReadLen = sound->bufferBytes;
    e = xbox_DSFind(sound);
    if (!e || !e->pDS) return 0;
    if (FAILED(IDirectSoundBuffer_Lock(e->pDS, 0, (DWORD)bufferReadLen, &pLock, &lockSz, NULL, NULL, 0))) return 0;
    memcpy(pLock, sound->data, lockSz);
    IDirectSoundBuffer_Unlock(e->pDS, pLock, lockSz, NULL, 0);
    return 1;
}

int stdSound_BufferPlay(stdSound_buffer_t *buf, int loop)
{
    XboxDSEntry *e;
    if (!buf) return 0;
    e = xbox_DSFind(buf);
    if (!e || !e->pDS) return 0;
    IDirectSoundBuffer_SetCurrentPosition(e->pDS, 0);
    IDirectSoundBuffer_SetVolume(e->pDS, xbox_VolToDS(buf->vol * stdSound_fMenuVolume));
    e->bLooping = loop;
    e->bPlaying = SUCCEEDED(IDirectSoundBuffer_Play(e->pDS, 0, 0, loop ? DSBPLAY_LOOPING : 0)) ? 1 : 0;
    return e->bPlaying;
}

int stdSound_BufferStop(stdSound_buffer_t *a1)
{
    XboxDSEntry *e;
    if (!a1) return 0;
    e = xbox_DSFind(a1);
    if (e && e->pDS) { IDirectSoundBuffer_Stop(e->pDS); e->bPlaying = 0; }
    return 1;
}

int stdSound_BufferReset(stdSound_buffer_t *a1)
{
    XboxDSEntry *e;
    if (!a1) return 0;
    e = xbox_DSFind(a1);
    if (e && e->pDS)
    {
        IDirectSoundBuffer_Stop(e->pDS);
        IDirectSoundBuffer_SetCurrentPosition(e->pDS, 0);
        e->bPlaying = 0;
    }
    return 1;
}

void stdSound_BufferRelease(stdSound_buffer_t *sound)
{
    if (!sound) return;
    sound->refcnt--;
    if (sound->refcnt > 0) return;
    xbox_DSFree(sound);
    if (sound->data && !sound->bIsCopy) free(sound->data);
    free(sound);
}

void stdSound_3DBufferRelease(stdSound_3dBuffer_t *p)
{
    if (p)
        free(p);
}

stdSound_buffer_t *stdSound_BufferDuplicate(stdSound_buffer_t *sound)
{
    stdSound_buffer_t *copy;
    XboxDSEntry *src, *dst;
    if (!sound) return NULL;
    copy = (stdSound_buffer_t*)malloc(sizeof(stdSound_buffer_t));
    if (!copy) return NULL;
    memcpy(copy, sound, sizeof(stdSound_buffer_t));
    copy->bIsCopy = 1;
    copy->refcnt  = 1;
    dst = xbox_DSAlloc(copy);
    src = xbox_DSFind(sound);
    if (dst && src && src->pDS && g_pDS && sound->bufferBytes > 0)
        xbox_DSCreateBuffer(copy, dst, src->b3D);
    return copy;
}

void stdSound_BufferSetVolume(stdSound_buffer_t *a1, float a2)
{
    XboxDSEntry *e;
    if (!a1) return;
    a1->vol = a2;
    e = xbox_DSFind(a1);
    if (e && e->pDS) IDirectSoundBuffer_SetVolume(e->pDS, xbox_VolToDS(a2 * stdSound_fMenuVolume));
}

void stdSound_BufferSetPan(stdSound_buffer_t *a1, float a2) { (void)a1; (void)a2; }

void stdSound_BufferSetFrequency(stdSound_buffer_t *a1, int a2)
{
    XboxDSEntry *e;
    if (!a1) return;
    e = xbox_DSFind(a1);
    if (e && e->pDS) IDirectSoundBuffer_SetFrequency(e->pDS, (DWORD)a2);
}

void stdSound_BufferSetFrequency2(stdSound_buffer_t *a1, int a2) { stdSound_BufferSetFrequency(a1, a2); }
void stdSound_SetMenuVolume(float a1) { stdSound_fMenuVolume = a1; }

stdSound_3dBuffer_t *stdSound_BufferQueryInterface(stdSound_buffer_t *a1)
{
    XboxDSEntry *e;
    stdXbox3DBuffer *buf3d;

    if (!a1 || a1->bStereo)
        return NULL;

    e = xbox_DSFind(a1);
    if (!e || !e->pDS)
        return NULL;
    if (!e->b3D && !xbox_DSCreateBuffer(a1, e, 1))
        return NULL;

    buf3d = (stdXbox3DBuffer*)malloc(sizeof(stdXbox3DBuffer));
    if (!buf3d)
        return NULL;

    memset(buf3d, 0, sizeof(stdXbox3DBuffer));
    buf3d->owner = a1;
    buf3d->pDS = e->pDS;
    IDirectSoundBuffer_SetMinDistance(buf3d->pDS, 1.0f, DS3D_DEFERRED);
    IDirectSoundBuffer_SetMaxDistance(buf3d->pDS, 1000.0f, DS3D_DEFERRED);
    return buf3d;
}

int stdSound_3DSetMode(stdSound_3dBuffer_t *a1, int a2)
{
    DWORD mode;
    if (!a1 || !a1->pDS)
        return 0;
    mode = (a2 == 2) ? DS3DMODE_DISABLE : ((a2 == 1) ? DS3DMODE_HEADRELATIVE : DS3DMODE_NORMAL);
    return SUCCEEDED(IDirectSoundBuffer_SetMode(a1->pDS, mode, DS3D_DEFERRED));
}

void stdSound_SetPositionOrientation(rdVector3 *p, rdVector3 *l, rdVector3 *u)
{
    float px, py, pz;
    float lx, ly, lz;
    float ux, uy, uz;

    if (!g_pDS || !p || !l || !u)
        return;

    xbox_DS3DToXbox(&px, &py, &pz, p);
    xbox_DS3DToXbox(&lx, &ly, &lz, l);
    xbox_DS3DToXbox(&ux, &uy, &uz, u);
    IDirectSound_SetPosition(g_pDS, px, py, pz, DS3D_DEFERRED);
    IDirectSound_SetOrientation(g_pDS, lx, ly, lz, ux, uy, uz, DS3D_DEFERRED);
}

void stdSound_SetPosition(stdSound_3dBuffer_t *s, rdVector3 *p)
{
    float x, y, z;
    if (!s || !s->pDS || !p)
        return;
    s->pos = *p;
    xbox_DS3DToXbox(&x, &y, &z, p);
    IDirectSoundBuffer_SetPosition(s->pDS, x, y, z, DS3D_DEFERRED);
}

void stdSound_SetVelocity(stdSound_3dBuffer_t *s, rdVector3 *v)
{
    float x, y, z;
    if (!s || !s->pDS || !v)
        return;
    s->vel = *v;
    xbox_DS3DToXbox(&x, &y, &z, v);
    IDirectSoundBuffer_SetVelocity(s->pDS, x, y, z, DS3D_DEFERRED);
}

void stdSound_CommitDeferredSettings(void)
{
    if (g_pDS)
        IDirectSound_CommitDeferredSettings(g_pDS);
}
void stdSound_IA3D_idk(float a) { (void)a; }

int stdSound_IsPlaying(stdSound_buffer_t *a1, rdVector3 *pos)
{
    XboxDSEntry *e;
    DWORD status;
    (void)pos;
    if (!a1) return 0;
    e = xbox_DSFind(a1);
    if (!e || !e->pDS) return 0;
    IDirectSoundBuffer_GetStatus(e->pDS, &status);
    return (status & DSBSTATUS_PLAYING) ? 1 : 0;
}

int stdSound_BufferQueueAfterAnother(stdSound_buffer_t *a, stdSound_buffer_t *b)
{
    enum { XBOX_STREAM_QUEUE_DEPTH = 64 };
    static stdSound_buffer_t *anchor = NULL;
    static stdSound_buffer_t *current = NULL;
    static stdSound_buffer_t *queue[XBOX_STREAM_QUEUE_DEPTH];
    static int head = 0;
    static int tail = 0;

    if (!a)
        return 0;

    if (anchor != a)
    {
        anchor = a;
        current = NULL;
        head = 0;
        tail = 0;
        memset(queue, 0, sizeof(queue));
    }

    if (b)
    {
        int nextTail = (tail + 1) % XBOX_STREAM_QUEUE_DEPTH;
        if (nextTail != head)
        {
            queue[tail] = b;
            tail = nextTail;
        }
    }

    if (current && stdSound_IsPlaying(current, NULL))
        return 1;

    current = NULL;
    if (head != tail)
    {
        current = queue[head];
        queue[head] = NULL;
        head = (head + 1) % XBOX_STREAM_QUEUE_DEPTH;
        if (current)
            stdSound_BufferPlay(current, 0);
    }

    return 1;
}
void stdSound_SetMenuSoundFormat(void) { }
