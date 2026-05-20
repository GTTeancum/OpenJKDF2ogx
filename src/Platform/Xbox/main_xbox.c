/*
 * main_xbox.c  —  OpenJKDF2 Xbox entry point
 *
 * Location:  src/Platform/Xbox/main_xbox.c
 *
 * Boot sequence:
 *   1. Debug log
 *   2. D3D8 device + swapchain (Window_xbox_Startup)
 *   3. File system (stdFile_xbox_Startup)
 *   4. Engine init (Main_Startup)
 *   5. Game loop — jkMain_GuiAdvance drives the state machine
 */

#include "platform_xbox.h"
#include "xbox_debug.h"
#include "../../engine_config.h"
#include <xtl.h>

/* Declared in Window_xbox.c */
int  Window_xbox_Startup(void);
void Window_xbox_Shutdown(void);
int  Window_xbox_PumpMessages(void);
void Window_xbox_Present(void);

/* Forward declarations */
extern "C" int std3D_StartScene(void);
extern "C" int std3D_EndScene(void);
extern "C" void std3D_Present(void);
int Main_Startup(const char *cmdline);
void stdFile_xbox_Startup(void);
int  stdControl_Startup(void);
void jkMain_GuiAdvance(void);
extern int jkSmack_stopTick;
extern int jkSmack_nextGuiState;
extern int jkSmack_currentGuiState;
extern int jkPlayer_setDisableCutscenes;
extern uint32_t g_app_suspended;
extern void jkRes_LoadGob(char *a1);
extern int sithMain_Load(char *jklFname);
extern int jkHudInv_InitItems(void);
extern void jkGuiTitle_WorldLoadCallback(float percentage);
extern void jkGuiTitle_LoadingStaticFinalizeMenu(void);
extern int jkGuiTitle_whichLoading;
extern float jkPlayer_hudScale;
struct stdVideoMode;
struct stdVBuffer;
extern struct stdVideoMode* stdDisplay_pCurVideoMode;
extern struct stdVBuffer* Video_pMenuBuffer;
extern struct stdVBuffer* Video_pVbufIdk;
extern struct stdVBuffer* Video_pOverlayMapBuffer;

#ifdef XBOX_PERF_SMOKE
static const char *xbox_read_smoke_autostart_args(char *buf, unsigned int bufSize)
{
    DWORD readBytes = 0;
    HANDLE h;
    unsigned int i;

    if (!buf || bufSize < 2)
        return "-autostart -sp -episode JK1 -map 01narshadda.jkl";

    h = CreateFileA("D:\\xbox_smoke_autostart_args.txt", GENERIC_READ,
                    FILE_SHARE_READ, NULL, OPEN_EXISTING, FILE_ATTRIBUTE_NORMAL, NULL);
    if (h == INVALID_HANDLE_VALUE)
        return "-autostart -sp -episode JK1 -map 01narshadda.jkl";

    if (!ReadFile(h, buf, bufSize - 1, &readBytes, NULL))
        readBytes = 0;
    CloseHandle(h);
    buf[readBytes] = 0;

    for (i = 0; i < readBytes; i++)
    {
        if (buf[i] == '\r' || buf[i] == '\n')
        {
            buf[i] = 0;
            break;
        }
    }

    return buf[0] ? buf : "-autostart -sp -episode JK1 -map 01narshadda.jkl";
}
#endif

/* =========================================================================
 * void main(void)  —  Xbox entry point (no argc/argv on Xbox)
 * ====================================================================== */
void __cdecl main(void)
{
    /* Debug logging must come first — everything else uses XDBG */
    xbox_debug_Startup();
    XDBG("OpenJKDF2 Xbox: main() entered\n");

    /* ----------------------------------------------------------------
     * 0. XInput device enumeration — MUST come before D3D init.
     *    XDK requirement: XInitDevices must be called before
     *    Direct3D_CreateDevice (ordering required by the USB host
     *    controller initialisation sequence on NV2A hardware).
     *    stdControl_Startup skips the XInitDevices call and only
     *    calls XInputOpen to open the controller handle.
     * -------------------------------------------------------------- */
    {
        XDEVICE_PREALLOC_TYPE xdpt[2];
        xdpt[0].DeviceType      = XDEVICE_TYPE_GAMEPAD;
        xdpt[0].dwPreallocCount = 4;
        xdpt[1].DeviceType      = XDEVICE_TYPE_MEMORY_UNIT;
        xdpt[1].dwPreallocCount = 8;
        XDBG("main: calling XInitDevices\n");
        XInitDevices(2, xdpt);
        XDBG("main: XInitDevices done\n");
    }

    /* ----------------------------------------------------------------
     * 1. D3D8 device + swapchain
     * -------------------------------------------------------------- */
    if (!Window_xbox_Startup())
    {
        XDBG("main: Window_xbox_Startup failed — halting\n");
        for (;;) {}
    }

    /* ----------------------------------------------------------------
     * 2. File system — must be before Main_Startup
     * -------------------------------------------------------------- */
    stdFile_xbox_Startup();

    /* Initialize display globals so jkMain_SetVideoMode and rdCanvas_New don't crash.
     * These are raw byte buffers matching struct layouts since we can't include types.h
     * (conflicts with xtl.h). Layout verified from types.h definitions. */
    {
        /* stdVideoMode: { int32 field_0; float widthMaybe; stdVBufferTexFmt format {...} } */
        static char xboxVideoModeBuf[128];
        memset(xboxVideoModeBuf, 0, sizeof(xboxVideoModeBuf));
        *(float*)(xboxVideoModeBuf + 4) = 640.0f;   /* widthMaybe */
        *(int*)(xboxVideoModeBuf + 8)  = 640;        /* format.width */
        *(int*)(xboxVideoModeBuf + 12) = 480;        /* format.height */
        *(int*)(xboxVideoModeBuf + 16) = 640*480;    /* format.texture_size_in_bytes */
        *(int*)(xboxVideoModeBuf + 20) = 640;        /* format.width_in_bytes */
        *(int*)(xboxVideoModeBuf + 24) = 640;        /* format.width_in_pixels */
        *(int*)(xboxVideoModeBuf + 32) = 8;          /* format.format.bpp */
        stdDisplay_pCurVideoMode = (struct stdVideoMode*)xboxVideoModeBuf;

        /* stdVBuffer: { uint32 bSurfaceLocked; uint32 lock_cnt; uint32 gap8;
         *   stdVBufferTexFmt format { int32 width, height, ... }; ... }
         * Video_pMenuBuffer and Video_pVbufIdk must be non-NULL for rdCanvas_New */
        static char xboxMenuBuf[256];
        static char xboxVbufIdk[256];
        static char xboxOverlayBuf[256];
        memset(xboxMenuBuf, 0, sizeof(xboxMenuBuf));
        memset(xboxVbufIdk, 0, sizeof(xboxVbufIdk));
        memset(xboxOverlayBuf, 0, sizeof(xboxOverlayBuf));
        /* format starts at offset 12 (after bSurfaceLocked + lock_cnt + gap8) */
        *(int*)(xboxMenuBuf + 12) = 640;  /* format.width */
        *(int*)(xboxMenuBuf + 16) = 480;  /* format.height */
        *(int*)(xboxVbufIdk + 12) = 640;
        *(int*)(xboxVbufIdk + 16) = 480;
        *(int*)(xboxOverlayBuf + 12) = 640;
        *(int*)(xboxOverlayBuf + 16) = 480;
        Video_pMenuBuffer = (struct stdVBuffer*)xboxMenuBuf;
        Video_pVbufIdk = (struct stdVBuffer*)xboxVbufIdk;
        Video_pOverlayMapBuffer = (struct stdVBuffer*)xboxOverlayBuf;
    }

    XDBG("main: calling Main_Startup\n");

    /* ----------------------------------------------------------------
     * 3. Game engine init
     *    Main_Startup takes a command-line string, same as the PC build.
     *    Pass empty string for default behaviour (no episode override).
     * -------------------------------------------------------------- */
#ifdef XBOX_PERF_SMOKE
    {
        char smokeArgs[160];
        const char *startupArgs = xbox_read_smoke_autostart_args(smokeArgs, sizeof(smokeArgs));
        XPERF("Smoke: Main_Startup args='%s'\n", startupArgs);
        Main_Startup((char *)startupArgs);
        XPERF("Smoke: Main_Startup returned\n");
    }
#else
    Main_Startup("");
#endif

    /* Match the upstream startup flow: the title GUI state loads static.jkl
     * and items.dat after the intro FMV, before the main menu is shown. */
    jkPlayer_hudScale = 1.0f;

    /* Initialize XInput — must be after Main_Startup (XDK subsystems up) */
    stdControl_Startup();

    /* Establish default input bindings.  On PC, sithControl_InputInit is
     * called from inside jkPlayer_ReadConf / jkPlayer_CreateConf — i.e.
     * profile-load triggers it.  Xbox skips the GUI/profile flow and
     * goes straight to gameplay, so we'd otherwise have zero bindings
     * (joystick axes populate but no INPUT_FUNC_* routes to them).
     *
     * Calling InputInit here decouples "establish default bindings"
     * from "load a profile" — defaults exist as part of input-subsystem
     * init regardless of whether a profile loads later. */
    {
        extern void sithControl_InputInit(void);
        sithControl_InputInit();
        XDBG("main: sithControl_InputInit done\n");
    }

    /* Let Main_Startup's normal jkSmack state transition play cutscenes. */
    g_app_suspended = 1;

    if (g_app_suspended) { XDBG("main: g_app_suspended=1 OK\n"); }
    else { XDBG("main: WARNING g_app_suspended=0!\n"); }
    XDBG("main: entering game loop\n");

    /* ----------------------------------------------------------------
     * 4. Game loop
     * -------------------------------------------------------------- */
    { static int loopCount = 0;
#ifdef XBOX_PERF_SMOKE
#ifndef XBOX_PERF_PHASE_TRACE
#define XBOX_PERF_PHASE_TRACE 0
#endif
    static DWORD s_perfLoopLastMs = 0;
    static unsigned long s_perfStartMs = 0;
    static unsigned long s_perfGuiMs = 0;
    static unsigned long s_perfEndMs = 0;
    static unsigned long s_perfPresentMs = 0;
    static unsigned long s_perfSleepMs = 0;
    static unsigned int s_perfPhaseFrames = 0;
    static int s_perfTraceNextFrame = 0;
#endif
    while (Window_xbox_PumpMessages())
    {
        /* Heartbeat / per-step tick logging.  Commented out — game is
         * healthy and these flood D:\debug_openjkdf2.txt faster than we
         * can read it.  Restore by uncommenting for boot-path diagnosis
         * or main-loop hangs. */
        /*
        if (loopCount < 5 || (loopCount % 100) == 0) {
            XDBGF("main: tick %d\n", loopCount);
        }
        */
        loopCount++;
#ifdef XBOX_PERF_SMOKE
        {
            DWORD nowMs = GetTickCount();
            if (!s_perfLoopLastMs)
                s_perfLoopLastMs = nowMs;
            if (nowMs - s_perfLoopLastMs >= 5000)
            {
                XPERF("Perf: mainLoop=%d elapsedMs=%lu suspended=%d gui=%d phaseFrames=%u startMs=%lu advanceMs=%lu endMs=%lu presentMs=%lu sleepMs=%lu\n",
                      loopCount, (unsigned long)nowMs, g_app_suspended, jkSmack_currentGuiState,
                      s_perfPhaseFrames, s_perfStartMs, s_perfGuiMs, s_perfEndMs,
                      s_perfPresentMs, s_perfSleepMs);
                s_perfStartMs = 0;
                s_perfGuiMs = 0;
                s_perfEndMs = 0;
                s_perfPresentMs = 0;
                s_perfSleepMs = 0;
                s_perfPhaseFrames = 0;
                s_perfTraceNextFrame = 1;
                s_perfLoopLastMs = nowMs;
            }
        }
#endif
        /* if (loopCount < 5) XDBG("main: -> StartScene\n"); */
#ifdef XBOX_PERF_SMOKE
        { DWORD t0 = GetTickCount();
          int traceFrame = XBOX_PERF_PHASE_TRACE ? s_perfTraceNextFrame : 0;
          if (traceFrame) XPERF("PerfPhase: loop=%d before StartScene\n", loopCount);
#endif
        std3D_StartScene();
#ifdef XBOX_PERF_SMOKE
          { DWORD t1 = GetTickCount();
            if (traceFrame) XPERF("PerfPhase: loop=%d after StartScene before GuiAdvance\n", loopCount);
#endif
        /* if (loopCount < 5) XDBG("main: -> GuiAdvance\n"); */
        jkMain_GuiAdvance();
#ifdef XBOX_PERF_SMOKE
            { DWORD t2 = GetTickCount();
              if (traceFrame) XPERF("PerfPhase: loop=%d after GuiAdvance before EndScene\n", loopCount);
#endif
        /* if (loopCount < 5) XDBG("main: -> EndScene\n"); */
        std3D_EndScene();
#ifdef XBOX_PERF_SMOKE
              { DWORD t3 = GetTickCount();
                if (traceFrame) XPERF("PerfPhase: loop=%d after EndScene before Present\n", loopCount);
#endif
        /* if (loopCount < 5) XDBG("main: -> Present\n"); */
        std3D_Present();  /* Swap buffers AFTER EndScene, outside scene block */
#ifdef XBOX_PERF_SMOKE
                { DWORD t4 = GetTickCount();
                  if (traceFrame) XPERF("PerfPhase: loop=%d after Present before Sleep\n", loopCount);
#endif
        /* if (loopCount < 5) XDBG("main: -> Sleep\n"); */
        /* Sleep(1) yields to the kernel scheduler without imposing a
         * long idle.  Sleep(16) was capping us to ~30 fps on hardware:
         * render+logic ≈ 18ms per frame, plus 16ms sleep = ~34ms ≈ 29
         * fps.  With Sleep(1) the loop becomes render-bound and dt
         * shrinks, giving the engine more responsive physics ticks
         * (the engine itself is fully fixed-timestep, so running the
         * outer loop faster doesn't speed up gameplay — it just
         * reduces input-to-display latency and increases dt accuracy). */
#if defined(XBOX_PERF_SMOKE) || defined(XBOX_MAIN_LOOP_PERF_YIELD)
        /* CXBX-R eventually turns in-game Sleep(0)/Sleep(1) into ~300 ms
         * stalls in this port's smoke loop. Yield occasionally during
         * title/load UI so load progress stays cooperative, then let present
         * plus the host emulator loop throttle gameplay. */
        if (jkGuiTitle_whichLoading && ((loopCount & 63) == 0))
            Sleep(1);
#else
        Sleep(1);
#endif
#ifdef XBOX_PERF_SMOKE
                  { DWORD t5 = GetTickCount();
                    if (traceFrame) XPERF("PerfPhase: loop=%d after Sleep\n", loopCount);
                    s_perfStartMs += (unsigned long)(t1 - t0);
                    s_perfGuiMs += (unsigned long)(t2 - t1);
                    s_perfEndMs += (unsigned long)(t3 - t2);
                    s_perfPresentMs += (unsigned long)(t4 - t3);
                    s_perfSleepMs += (unsigned long)(t5 - t4);
                    s_perfPhaseFrames++;
                    if (traceFrame) s_perfTraceNextFrame = 0;
                  }
                }
              }
            }
          }
        }
#endif
    }
    } /* close loopCount scope */

    Window_xbox_Shutdown();
    xbox_debug_Shutdown();
    XDBG("main: clean exit\n");
}
