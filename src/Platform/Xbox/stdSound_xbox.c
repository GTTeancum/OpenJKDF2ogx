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
} XboxDSEntry;

static XboxDSEntry  g_dsTable[MAX_DS_BUFFERS];
static int          g_dsCount = 0;
static IDirectSound *g_pDS    = NULL;

float stdSound_fMenuVolume = 1.0f;

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

static LONG xbox_VolToDS(float v)
{
    if (v <= 0.0f) return -10000;
    if (v >= 1.0f) return 0;
    return (LONG)(2000.0f * (float)log10((double)v));
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
    DSBUFFERDESC    desc;
    WAVEFORMATEX    wfx;
    XboxDSEntry    *e;
    HRESULT         hr;

    if (!sound) return NULL;
    if (sound->data) free(sound->data);
    sound->data        = malloc(bufferBytes);
    sound->bufferBytes = bufferBytes;
    if (bufferMaxSize) *bufferMaxSize = bufferBytes;
    if (!sound->data || !g_pDS) return sound->data;

    e = xbox_DSAlloc(sound);
    if (!e) return sound->data;

    if (e->pDS) { IDirectSoundBuffer_Release(e->pDS); e->pDS = NULL; }

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
    if (!sound->bStereo)
        desc.dwFlags |= DSBCAPS_CTRL3D;
    desc.dwBufferBytes = (DWORD)bufferBytes;
    desc.lpwfxFormat   = &wfx;

    hr = IDirectSound_CreateSoundBuffer(g_pDS, &desc, &e->pDS, NULL);
    if (FAILED(hr)) { XDBGF("stdSound_BufferSetData: CreateSoundBuffer failed 0x%X\n", hr); e->pDS = NULL; }
    return sound->data;
}

int stdSound_BufferUnlock(stdSound_buffer_t *sound, void *buffer, int bufferReadLen)
{
    XboxDSEntry *e;
    void *pLock = NULL;
    DWORD lockSz = 0;
    (void)buffer;
    if (!sound || !sound->data) return 0;
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
    HRESULT hr;
    if (!sound) return NULL;
    copy = (stdSound_buffer_t*)malloc(sizeof(stdSound_buffer_t));
    if (!copy) return NULL;
    memcpy(copy, sound, sizeof(stdSound_buffer_t));
    copy->bIsCopy = 1;
    copy->refcnt  = 1;
    dst = xbox_DSAlloc(copy);
    src = xbox_DSFind(sound);
    if (dst && src && src->pDS && g_pDS && sound->bufferBytes > 0)
    {
        DSBUFFERDESC desc;
        WAVEFORMATEX wfx;
        void *pLock = NULL;
        DWORD lockSz = 0;
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
        if (!sound->bStereo)
            desc.dwFlags |= DSBCAPS_CTRL3D;
        desc.dwBufferBytes = (DWORD)sound->bufferBytes;
        desc.lpwfxFormat   = &wfx;
        hr = IDirectSound_CreateSoundBuffer(g_pDS, &desc, &dst->pDS, NULL);
        if (SUCCEEDED(hr) && sound->data)
        {
            if (SUCCEEDED(IDirectSoundBuffer_Lock(dst->pDS, 0, (DWORD)sound->bufferBytes, &pLock, &lockSz, NULL, NULL, 0)))
            {
                memcpy(pLock, sound->data, lockSz);
                IDirectSoundBuffer_Unlock(dst->pDS, pLock, lockSz, NULL, 0);
            }
        }
    }
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

int stdSound_BufferQueueAfterAnother(stdSound_buffer_t *a, stdSound_buffer_t *b) { (void)a; (void)b; return 0; }
void stdSound_SetMenuSoundFormat(void) { }
