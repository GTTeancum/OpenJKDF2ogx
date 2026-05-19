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

static void xboxXmv_SameDirXmv(const char *smkPath, char *out, size_t outSize)
{
    size_t len = strlen(smkPath);
    size_t dot = len;
    while (dot && smkPath[dot - 1] != '.')
        dot--;
    if (dot)
        dot--;
    else
        dot = len;

    if (dot + 4 >= outSize)
        dot = outSize - 5;
    memcpy(out, smkPath, dot);
    memcpy(out + dot, ".xmv", 5);
}

static int xboxXmv_FindForSmkPath(const char *smkPath, char *out, size_t outSize)
{
    char base[128];
    char candidates[5][260];
    int i;

    xboxXmv_BaseNameNoExt(smkPath, base, sizeof(base));
    xboxXmv_SameDirXmv(smkPath, candidates[0], sizeof(candidates[0]));
    _snprintf(candidates[1], sizeof(candidates[1]), "D:\\resource\\video_xbox\\%s.xmv", base);
    _snprintf(candidates[2], sizeof(candidates[2]), "D:\\resource\\video\\%s.xmv", base);
    _snprintf(candidates[3], sizeof(candidates[3]), "D:\\video\\%s.xmv", base);
    _snprintf(candidates[4], sizeof(candidates[4]), "resource\\video_xbox\\%s.xmv", base);

    for (i = 0; i < 5; i++)
    {
        candidates[i][sizeof(candidates[i]) - 1] = 0;
        if (xboxXmv_FileExists(candidates[i]))
        {
            strncpy(out, candidates[i], outSize - 1);
            out[outSize - 1] = 0;
            return 1;
        }
    }

    XDBGF("XmvDbg: no loose XMV for '%s' tried base='%s'\n", smkPath, base);
    return 0;
}

static DWORD WINAPI xboxXmv_PlayThread(void *arg)
{
    XMVDecoder *decoder = (XMVDecoder *)arg;
    return FAILED(decoder->Play(XMVFLAG_NONE, NULL)) ? 1 : 0;
}

static int xboxXmv_OpenPads(HANDLE pads[4])
{
    DWORD mask = XGetDevices(XDEVICE_TYPE_GAMEPAD);
    int count = 0;
    int port;

    for (port = 0; port < 4; port++)
    {
        pads[port] = NULL;
        if (!(mask & (1u << port)))
            continue;

        pads[port] = XInputOpen(XDEVICE_TYPE_GAMEPAD, XDEVICE_PORT0 + port,
                                XDEVICE_NO_SLOT, NULL);
        if (pads[port])
            count++;
    }
    return count;
}

static void xboxXmv_ClosePads(HANDLE pads[4])
{
    int port;
    for (port = 0; port < 4; port++)
    {
        if (pads[port])
        {
            XInputClose(pads[port]);
            pads[port] = NULL;
        }
    }
}

static int xboxXmv_ShouldSkip(HANDLE pads[4], int *outPort, const char **outReason)
{
    static const char *kAnalogNames[8] = {
        "A", "B", "X", "Y", "black", "white", "left trigger", "right trigger"
    };
    int port;

    if (outPort)
        *outPort = -1;
    if (outReason)
        *outReason = "unknown";

    for (port = 0; port < 4; port++)
    {
        XINPUT_STATE state;
        XINPUT_GAMEPAD *pad;
        int i;

        if (!pads[port])
            continue;

        if (XInputGetState(pads[port], &state) != ERROR_SUCCESS)
            continue;

        pad = &state.Gamepad;
        if (pad->wButtons)
        {
            if (outPort)
                *outPort = port;
            if (outReason)
                *outReason = "digital";
            return 1;
        }

        for (i = 0; i < 8; i++)
        {
            if (pad->bAnalogButtons[i] > 10)
            {
                if (outPort)
                    *outPort = port;
                if (outReason)
                    *outReason = kAnalogNames[i];
                return 1;
            }
        }
    }

    return 0;
}

extern "C" int xboxXmv_PlayForSmkPath(const char *smkPath)
{
    char xmvPath[260];
    XMVDecoder *decoder = NULL;
    XMVVIDEO_DESC videoDesc;
    HANDLE pads[4] = { NULL, NULL, NULL, NULL };
    HANDLE thread;
    HRESULT hr;
    int terminated = 0;

    if (!smkPath || !xboxXmv_FindForSmkPath(smkPath, xmvPath, sizeof(xmvPath)))
        return 0;

    XDBGF("XmvDbg: opening '%s' for smk '%s'\n", xmvPath, smkPath);
    hr = XMVDecoder_CreateDecoderForFile(XMVFLAG_SYNC_ON_NEXT_VBLANK, xmvPath, &decoder);
    if (FAILED(hr) || !decoder)
    {
        XDBGF("XmvDbg: CreateDecoderForFile failed hr=0x%08X path='%s'\n", hr, xmvPath);
        return 0;
    }

    decoder->GetVideoDescriptor(&videoDesc);
    XDBGF("XmvDbg: desc path='%s' w=%lu h=%lu fps=%lu audioStreams=%lu\n",
          xmvPath, videoDesc.Width, videoDesc.Height,
          videoDesc.FramesPerSecond, videoDesc.AudioStreamCount);

    if (videoDesc.AudioStreamCount)
    {
        hr = decoder->EnableAudioStream(0, 0, NULL, NULL);
        XDBGF("XmvDbg: EnableAudioStream(0) hr=0x%08X\n", hr);
    if (SUCCEEDED(hr))
            decoder->SetSynchronizationStream(0);
    }

    XDBGF("XmvDbg: opened %d controller(s) for skip polling\n", xboxXmv_OpenPads(pads));

    thread = CreateThread(NULL, 0, xboxXmv_PlayThread, decoder, 0, NULL);
    if (!thread)
    {
        XDBGF("XmvDbg: CreateThread failed err=%lu\n", GetLastError());
        xboxXmv_ClosePads(pads);
        decoder->TerminateImmediately();
        decoder->CloseDecoder();
        return 0;
    }

    for (;;)
    {
        DWORD waitResult = WaitForSingleObject(thread, 1000 / 60);
        int skipPort;
        const char *skipReason;

        if (waitResult == WAIT_OBJECT_0)
            break;

        if (xboxXmv_ShouldSkip(pads, &skipPort, &skipReason))
        {
            XDBGF("XmvDbg: skip requested port=%d reason=%s\n", skipPort, skipReason);
            terminated = 1;
            decoder->TerminatePlayback();
            goto wait_done;
        }
    }

wait_done:
    WaitForSingleObject(thread, INFINITE);
    CloseHandle(thread);
    xboxXmv_ClosePads(pads);
    XDBGF("XmvDbg: playback done path='%s' terminated=%d\n", xmvPath, terminated);
    decoder->CloseDecoder();
    return 1;
}
