/*
 * Xbox stdMci music backend.
 *
 * GoG installs store music as Ogg/Vorbis in \Music\TrackNN.ogg.  The engine
 * still talks through the old CD-audio stdMci API, so this file maps track
 * numbers to those OGG files and streams decoded PCM into a looping
 * DirectSound secondary buffer.
 */

#define CINTERFACE
#define STB_VORBIS_NO_STDIO
#define STB_VORBIS_NO_FAST_SCALED_FLOAT
#define STB_VORBIS_HEADER_ONLY

#include "platform_xbox.h"
#include "xbox_debug.h"
#include <xtl.h>
#include <dsound.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>
#include "types.h"
#include "globals.h"
#include "Win95/stdMci.h"
#include "General/stdString.h"
#include "Main/jkMain.h"
#include "Main/Main.h"
#include "../../../lib/SDL_mixer/src/codecs/stb_vorbis/stb_vorbis.h"

#undef STB_VORBIS_HEADER_ONLY
#include "../../../lib/SDL_mixer/src/codecs/stb_vorbis/stb_vorbis.h"

#define MCI_STREAM_CHUNKS 4
#define MCI_CHUNK_BYTES   (64 * 1024)
#define MCI_BUFFER_BYTES  (MCI_STREAM_CHUNKS * MCI_CHUNK_BYTES)

extern IDirectSound *stdSound_XboxGetDirectSound(void);

int stdMci_trackFrom = 0;
int stdMci_trackTo = 0;
int stdMci_trackCurrent = 0;

static IDirectSoundBuffer *stdMci_pBuffer = NULL;
static unsigned char *stdMci_oggData = NULL;
static unsigned int stdMci_oggBytes = 0;
static stb_vorbis *stdMci_vorbis = NULL;
static int stdMci_channels = 2;
static int stdMci_sampleRate = 44100;
static int stdMci_nextChunk = 0;
static int stdMci_musicPlaying = 0;
static flex_t stdMci_volume = 1.0f;

static LONG stdMci_VolToDS(flex_t v)
{
    if (v <= 0.0f) return -10000;
    if (v >= 1.0f) return 0;
    return (LONG)(2000.0f * (float)log10((double)v));
}

static void stdMci_CloseVorbis(void)
{
    if (stdMci_vorbis)
    {
        stb_vorbis_close(stdMci_vorbis);
        stdMci_vorbis = NULL;
    }
    if (stdMci_oggData)
    {
        free(stdMci_oggData);
        stdMci_oggData = NULL;
    }
    stdMci_oggBytes = 0;
}

static void stdMci_ReleaseBuffer(void)
{
    if (stdMci_pBuffer)
    {
        IDirectSoundBuffer_Stop(stdMci_pBuffer);
        IDirectSoundBuffer_Release(stdMci_pBuffer);
        stdMci_pBuffer = NULL;
    }
}

static int stdMci_ReadWholeFile(const char *path, unsigned char **outData, unsigned int *outBytes)
{
    stdFile_t fd;
    int size;
    unsigned char *data;

    if (!std_pHS || !path || !outData || !outBytes)
        return 0;

    fd = std_pHS->fileOpen(path, "rb");
    if (!fd)
        return 0;

    size = std_pHS->fileSize(fd);
    if (size <= 0)
    {
        std_pHS->fileClose(fd);
        return 0;
    }

    data = (unsigned char*)malloc((size_t)size);
    if (!data)
    {
        std_pHS->fileClose(fd);
        return 0;
    }

    if (std_pHS->fileRead(fd, data, (size_t)size) != (size_t)size)
    {
        free(data);
        std_pHS->fileClose(fd);
        return 0;
    }

    std_pHS->fileClose(fd);
    *outData = data;
    *outBytes = (unsigned int)size;
    return 1;
}

static int stdMci_TryOpenPath(const char *path)
{
    int err = 0;
    stb_vorbis_info info;

    stdMci_CloseVorbis();
    if (!stdMci_ReadWholeFile(path, &stdMci_oggData, &stdMci_oggBytes))
        return 0;

    stdMci_vorbis = stb_vorbis_open_memory(stdMci_oggData, (int)stdMci_oggBytes, &err, NULL);
    if (!stdMci_vorbis)
    {
        XDBGF("stdMci: stb_vorbis_open_memory failed %d for %s\n", err, path);
        stdMci_CloseVorbis();
        return 0;
    }

    info = stb_vorbis_get_info(stdMci_vorbis);
    stdMci_channels = info.channels;
    stdMci_sampleRate = info.sample_rate;
    if (stdMci_channels < 1 || stdMci_channels > 2 || stdMci_sampleRate <= 0)
    {
        XDBGF("stdMci: unsupported OGG format ch=%d rate=%d\n", stdMci_channels, stdMci_sampleRate);
        stdMci_CloseVorbis();
        return 0;
    }

    XDBGF("stdMci: opened %s ch=%d rate=%d\n", path, stdMci_channels, stdMci_sampleRate);
    return 1;
}

static int stdMci_OpenTrackFile(int track)
{
    char path[128];

    _snprintf(path, sizeof(path)-1, "\\Music\\Track%d.ogg", track);
    path[sizeof(path)-1] = 0;
    if (stdMci_TryOpenPath(path)) return 1;

    _snprintf(path, sizeof(path)-1, "Music\\Track%d.ogg", track);
    path[sizeof(path)-1] = 0;
    if (stdMci_TryOpenPath(path)) return 1;

    _snprintf(path, sizeof(path)-1, "MUSIC\\Track%d.ogg", track);
    path[sizeof(path)-1] = 0;
    if (stdMci_TryOpenPath(path)) return 1;

    if (track < 10)
    {
        _snprintf(path, sizeof(path)-1, "\\Music\\Track%02d.ogg", track);
        path[sizeof(path)-1] = 0;
        if (stdMci_TryOpenPath(path)) return 1;
    }

    return 0;
}

static int stdMci_OpenTrackWithFallbacks(int track)
{
    int cdNum = 1;
    if (jkMain_pEpisodeEnt)
        cdNum = jkMain_pEpisodeEnt->cdNum;
    else if (jkMain_pEpisodeEnt2)
        cdNum = jkMain_pEpisodeEnt2->cdNum;

    if (stdMci_OpenTrackFile(track))
        return 1;

    if (track <= 12 && !Main_bMotsCompat)
    {
        int shifted = track;
        if (cdNum == 1) shifted += 10;
        else if (cdNum == 2) shifted += 20;
        if (stdMci_OpenTrackFile(shifted))
            return 1;
    }

    return 0;
}

static int stdMci_CreateBuffer(void)
{
    IDirectSound *ds;
    DSBUFFERDESC desc;
    WAVEFORMATEX wfx;
    HRESULT hr;

    stdMci_ReleaseBuffer();
    ds = stdSound_XboxGetDirectSound();
    if (!ds)
        return 0;

    memset(&wfx, 0, sizeof(wfx));
    wfx.wFormatTag = WAVE_FORMAT_PCM;
    wfx.nChannels = (WORD)stdMci_channels;
    wfx.nSamplesPerSec = (DWORD)stdMci_sampleRate;
    wfx.wBitsPerSample = 16;
    wfx.nBlockAlign = (WORD)(wfx.nChannels * 2);
    wfx.nAvgBytesPerSec = wfx.nSamplesPerSec * wfx.nBlockAlign;

    memset(&desc, 0, sizeof(desc));
    desc.dwSize = sizeof(desc);
    desc.dwFlags = DSBCAPS_CTRLVOLUME;
    desc.dwBufferBytes = MCI_BUFFER_BYTES;
    desc.lpwfxFormat = &wfx;

    hr = IDirectSound_CreateSoundBuffer(ds, &desc, &stdMci_pBuffer, NULL);
    if (FAILED(hr))
    {
        XDBGF("stdMci: CreateSoundBuffer failed 0x%X\n", hr);
        stdMci_pBuffer = NULL;
        return 0;
    }

    IDirectSoundBuffer_SetVolume(stdMci_pBuffer, stdMci_VolToDS(stdMci_volume));
    return 1;
}

static int stdMci_AdvanceTrack(void)
{
    stdMci_trackCurrent++;
    if (stdMci_trackCurrent > stdMci_trackTo)
    {
        stdMci_musicPlaying = 0;
        return 0;
    }
    return stdMci_OpenTrackWithFallbacks(stdMci_trackCurrent);
}

static void stdMci_FillMemory(short *dst, int frames)
{
    int written = 0;
    while (written < frames)
    {
        int got;
        if (!stdMci_vorbis)
        {
            memset(dst + (written * stdMci_channels), 0,
                   (frames - written) * stdMci_channels * sizeof(short));
            return;
        }

        got = stb_vorbis_get_samples_short_interleaved(
            stdMci_vorbis,
            stdMci_channels,
            dst + (written * stdMci_channels),
            (frames - written) * stdMci_channels);

        if (got <= 0)
        {
            if (!stdMci_AdvanceTrack())
            {
                memset(dst + (written * stdMci_channels), 0,
                       (frames - written) * stdMci_channels * sizeof(short));
                return;
            }
            continue;
        }
        written += got;
    }
}

static void stdMci_FillChunk(int chunk)
{
    void *p1 = NULL, *p2 = NULL;
    DWORD s1 = 0, s2 = 0;
    DWORD offset = (DWORD)(chunk * MCI_CHUNK_BYTES);

    if (!stdMci_pBuffer)
        return;

    if (FAILED(IDirectSoundBuffer_Lock(stdMci_pBuffer, offset, MCI_CHUNK_BYTES, &p1, &s1, &p2, &s2, 0)))
        return;

    if (p1 && s1)
        stdMci_FillMemory((short*)p1, (int)(s1 / (stdMci_channels * sizeof(short))));
    if (p2 && s2)
        stdMci_FillMemory((short*)p2, (int)(s2 / (stdMci_channels * sizeof(short))));

    IDirectSoundBuffer_Unlock(stdMci_pBuffer, p1, s1, p2, s2);
}

static void stdMci_PrimeBuffer(void)
{
    int i;
    for (i = 0; i < MCI_STREAM_CHUNKS; i++)
        stdMci_FillChunk(i);
    stdMci_nextChunk = 0;
}

int stdMci_Startup(void)
{
    stdMci_uDeviceID = 0;
    stdMci_bInitted = 1;
    stdMci_bIsGOG = 1;
    return 1;
}

void stdMci_Shutdown(void)
{
    stdMci_Stop();
    stdMci_bInitted = 0;
    stdMci_trackFrom = 0;
    stdMci_trackTo = 0;
    stdMci_trackCurrent = 0;
    stdMci_bIsGOG = 1;
}

int stdMci_Play(uint8_t trackFrom, uint8_t trackTo)
{
    stdMci_Stop();
    stdMci_trackFrom = trackFrom;
    stdMci_trackTo = trackTo < trackFrom ? trackFrom : trackTo;
    stdMci_trackCurrent = trackFrom;

    XDBGF("stdMci: play track %d to %d\n", trackFrom, stdMci_trackTo);

    if (!stdMci_OpenTrackWithFallbacks(stdMci_trackCurrent))
    {
        XDBGF("stdMci: no OGG found for track %d\n", stdMci_trackCurrent);
        return 0;
    }

    if (!stdMci_CreateBuffer())
    {
        stdMci_CloseVorbis();
        return 0;
    }

    stdMci_musicPlaying = 1;
    stdMci_PrimeBuffer();
    IDirectSoundBuffer_SetCurrentPosition(stdMci_pBuffer, 0);
    if (FAILED(IDirectSoundBuffer_Play(stdMci_pBuffer, 0, 0, DSBPLAY_LOOPING)))
    {
        stdMci_Stop();
        return 0;
    }
    return 1;
}

void stdMci_SetVolume(flex_t vol)
{
    stdMci_volume = vol;
    if (stdMci_pBuffer)
        IDirectSoundBuffer_SetVolume(stdMci_pBuffer, stdMci_VolToDS(vol));
}

void stdMci_Stop(void)
{
    if (stdMci_musicPlaying)
        XDBG("stdMci: stop music\n");
    stdMci_musicPlaying = 0;
    stdMci_ReleaseBuffer();
    stdMci_CloseVorbis();
}

int stdMci_CheckStatus(void)
{
    DWORD playCursor = 0, writeCursor = 0;
    int playChunk;

    if (!stdMci_musicPlaying || !stdMci_pBuffer)
        return 0;

    if (FAILED(IDirectSoundBuffer_GetCurrentPosition(stdMci_pBuffer, &playCursor, &writeCursor)))
        return 1;

    playChunk = (int)(playCursor / MCI_CHUNK_BYTES);
    while (stdMci_nextChunk != playChunk)
    {
        stdMci_FillChunk(stdMci_nextChunk);
        stdMci_nextChunk = (stdMci_nextChunk + 1) % MCI_STREAM_CHUNKS;
    }

    if (!stdMci_musicPlaying)
        stdMci_ReleaseBuffer();

    return stdMci_musicPlaying;
}

flex_d_t stdMci_GetTrackLength(int track)
{
    (void)track;
    return 0.0;
}
