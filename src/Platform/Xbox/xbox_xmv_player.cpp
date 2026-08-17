#include "platform_xbox.h"
#include "xbox_debug.h"

#include <xtl.h>
#include <xmv.h>
#include <stdio.h>
#include <string.h>

static int xboxXmv_FileExists(const char *path)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    CloseHandle(h);
    return 1;
}

static DWORD xboxXmv_FileSizeOrZero(const char *path)
{
    HANDLE h = CreateFileA(path, GENERIC_READ, FILE_SHARE_READ, NULL,
                           OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    DWORD size;
    if (h == INVALID_HANDLE_VALUE)
        return 0;
    size = GetFileSize(h, NULL);
    CloseHandle(h);
    return size == INVALID_FILE_SIZE ? 0 : size;
}

static void xboxXmv_BaseNameNoExt(const char *path, char *out, size_t outSize)
{
    const char *base = path;
    const char *p;
    size_t len;

    for (p = path; *p; p++)
    {
        if (*p == '\\' || *p == '/')
            base = p + 1;
    }

    len = strlen(base);
    while (len && base[len - 1] != '.')
        len--;
    if (len)
        len--;
    else
        len = strlen(base);

    if (len >= outSize)
        len = outSize - 1;
    memcpy(out, base, len);
    out[len] = 0;
}

static void xboxXmv_SameDirXmv(const char *smkPath, const char *ext, char *out, size_t outSize)
{
    size_t len = strlen(smkPath);
    size_t dot = len;
    size_t extLen = strlen(ext);
    while (dot && smkPath[dot - 1] != '.')
        dot--;
    if (dot)
        dot--;
    else
        dot = len;

    if (dot + extLen >= outSize)
        dot = outSize - extLen - 1;
    memcpy(out, smkPath, dot);
    memcpy(out + dot, ext, extLen + 1);
}

static void xboxXmv_ToUpper(char *s)
{
    while (*s)
    {
        if (*s >= 'a' && *s <= 'z')
            *s = (char)(*s - 'a' + 'A');
        s++;
    }
}

static int xboxXmv_FindForSmkPath(const char *smkPath, char *out, size_t outSize)
{
    char base[128];
    char upperBase[128];
    char candidates[9][260];
    int i;

    xboxXmv_BaseNameNoExt(smkPath, base, sizeof(base));
    strncpy(upperBase, base, sizeof(upperBase) - 1);
    upperBase[sizeof(upperBase) - 1] = 0;
    xboxXmv_ToUpper(upperBase);

    xboxXmv_SameDirXmv(smkPath, ".xmv", candidates[0], sizeof(candidates[0]));
    xboxXmv_SameDirXmv(smkPath, ".XMV", candidates[1], sizeof(candidates[1]));
    _snprintf(candidates[2], sizeof(candidates[2]), "D:\\resource\\video_xbox\\%s.xmv", base);
    _snprintf(candidates[3], sizeof(candidates[3]), "D:\\resource\\video\\%s.xmv", base);
    _snprintf(candidates[4], sizeof(candidates[4]), "D:\\video\\%s.xmv", base);
    _snprintf(candidates[5], sizeof(candidates[5]), "D:\\resource\\video_xbox\\%s.XMV", upperBase);
    _snprintf(candidates[6], sizeof(candidates[6]), "D:\\resource\\video\\%s.XMV", upperBase);
    _snprintf(candidates[7], sizeof(candidates[7]), "D:\\video\\%s.XMV", upperBase);
    _snprintf(candidates[8], sizeof(candidates[8]), "resource\\video_xbox\\%s.XMV", upperBase);

    for (i = 0; i < 9; i++)
    {
        candidates[i][sizeof(candidates[i]) - 1] = 0;
        if (xboxXmv_FileExists(candidates[i]))
        {
            strncpy(out, candidates[i], outSize - 1);
            out[outSize - 1] = 0;
            XPERF("XmvDbg: selected candidate[%d]='%s' size=%lu\n",
                  i, out, xboxXmv_FileSizeOrZero(out));
            return 1;
        }
    }

    XPERF("XmvDbg: no loose XMV for '%s' tried base='%s' upper='%s'\n", smkPath, base, upperBase);
    return 0;
}

static DWORD WINAPI xboxXmv_PlayThread(void *arg)
{
    XMVDecoder *decoder = (XMVDecoder *)arg;
    HRESULT hr = decoder->Play(XMVFLAG_NONE, NULL);
    XPERF("XmvDbg: Play returned hr=0x%08X\n", hr);
    return FAILED(hr) ? 1 : 0;
}

static DWORD xboxXmv_ReadSmokeLimitMs(void)
{
    HANDLE h;
    char buf[32];
    DWORD got = 0;
    DWORD seconds = 0;
    DWORD i;

    h = CreateFileA("D:\\xbox_smoke_fmv_seconds.txt", GENERIC_READ, FILE_SHARE_READ,
                    NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return 0;

    if (ReadFile(h, buf, sizeof(buf) - 1, &got, NULL) && got > 0)
    {
        buf[got] = 0;
        for (i = 0; i < got; ++i)
        {
            if (buf[i] < '0' || buf[i] > '9')
                break;
            seconds = seconds * 10 + (DWORD)(buf[i] - '0');
        }
    }
    CloseHandle(h);

    if (seconds == 0 || seconds > 600)
        return 0;

    XPERF("XmvDbg: smoke auto-skip armed seconds=%lu\n", seconds);
    return seconds * 1000;
}

extern "C" int stdControl_XboxMovieSkipRequested(int *outPort, const char **outReason);

extern "C" int xboxXmv_PlayForSmkPath(const char *smkPath)
{
    char xmvPath[260];
    XMVDecoder *decoder = NULL;
    XMVVIDEO_DESC videoDesc;
    HANDLE thread;
    HRESULT hr;
    int terminated = 0;
    DWORD smokeLimitMs;
    DWORD playbackStartMs;

    if (!smkPath || !xboxXmv_FindForSmkPath(smkPath, xmvPath, sizeof(xmvPath)))
        return 0;

    XPERF("XmvDbg: opening '%s' for smk '%s'\n", xmvPath, smkPath);
    hr = XMVDecoder_CreateDecoderForFile(XMVFLAG_SYNC_ON_NEXT_VBLANK, xmvPath, &decoder);
    if (FAILED(hr) || !decoder)
    {
        XPERF("XmvDbg: CreateDecoderForFile failed hr=0x%08X path='%s'\n", hr, xmvPath);
        return 0;
    }

    decoder->GetVideoDescriptor(&videoDesc);
    XPERF("XmvDbg: desc path='%s' w=%lu h=%lu fps=%lu audioStreams=%lu\n",
          xmvPath, videoDesc.Width, videoDesc.Height,
          videoDesc.FramesPerSecond, videoDesc.AudioStreamCount);
    {
        DWORD *raw = (DWORD *)&videoDesc;
        XPERF("XmvDbg: rawdesc sizeof=%u dwords=%08lX %08lX %08lX %08lX\n",
              (unsigned int)sizeof(videoDesc),
              raw[0], raw[1], raw[2], raw[3]);
    }

    if (videoDesc.AudioStreamCount)
    {
        hr = decoder->EnableAudioStream(0, 0, NULL, NULL);
        XPERF("XmvDbg: EnableAudioStream(0) hr=0x%08X\n", hr);
        if (SUCCEEDED(hr))
            decoder->SetSynchronizationStream(0);
    }

    XDBG("XmvDbg: using stdControl-owned controller handles for skip polling\n");
    smokeLimitMs = xboxXmv_ReadSmokeLimitMs();
    playbackStartMs = GetTickCount();

    thread = CreateThread(NULL, 0, xboxXmv_PlayThread, decoder, 0, NULL);
    if (!thread)
    {
        XDBGF("XmvDbg: CreateThread failed err=%lu\n", GetLastError());
        decoder->TerminateImmediately();
        decoder->CloseDecoder();
        return 0;
    }

    for (;;)
    {
        DWORD waitResult = WaitForSingleObject(thread, 1000 / 120);
        int skipPort;
        const char *skipReason;

        if (waitResult == WAIT_OBJECT_0)
            break;

        if (smokeLimitMs && (DWORD)(GetTickCount() - playbackStartMs) >= smokeLimitMs)
        {
            XPERF("XmvDbg: smoke auto-skip fired elapsedMs=%lu limitMs=%lu\n",
                  (DWORD)(GetTickCount() - playbackStartMs), smokeLimitMs);
            terminated = 1;
            XDBG("XmvDbg: requesting TerminatePlayback for smoke auto-skip\n");
            decoder->TerminatePlayback();
            goto wait_done;
        }

        if (stdControl_XboxMovieSkipRequested(&skipPort, &skipReason))
        {
            XPERF("XmvDbg: skip requested port=%d reason=%s\n", skipPort, skipReason);
            terminated = 1;
            XDBG("XmvDbg: requesting TerminatePlayback for user skip\n");
            decoder->TerminatePlayback();
            goto wait_done;
        }
    }

wait_done:
    XDBG("XmvDbg: waiting for playback thread\n");
    WaitForSingleObject(thread, INFINITE);
    {
        DWORD threadExit = 0xFFFFFFFF;
        GetExitCodeThread(thread, &threadExit);
        XPERF("XmvDbg: playback thread exit=%lu terminated=%d\n", threadExit, terminated);
    }
    CloseHandle(thread);
    XPERF("XmvDbg: playback done path='%s' terminated=%d\n", xmvPath, terminated);
    decoder->CloseDecoder();
    return 1;
}
