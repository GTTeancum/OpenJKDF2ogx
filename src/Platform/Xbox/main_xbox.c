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
extern int jkPlayer_setDisableCutscenes;
extern uint32_t g_app_suspended;
extern void jkRes_LoadGob(char *a1);
extern int sithMain_Load(char *jklFname);
extern int jkHudInv_InitItems(void);
extern void jkGuiTitle_WorldLoadCallback(float percentage);
extern void jkGuiTitle_LoadingStaticFinalizeMenu(void);
extern float jkPlayer_hudScale;
struct stdVideoMode;
struct stdVBuffer;
extern struct stdVideoMode* stdDisplay_pCurVideoMode;
extern struct stdVBuffer* Video_pMenuBuffer;
extern struct stdVBuffer* Video_pVbufIdk;
extern struct stdVBuffer* Video_pOverlayMapBuffer;

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
    Main_Startup("");
    jkPlayer_setDisableCutscenes = 1;

    /* Preserve the pre-menu Xbox gameplay init path: the old jkGuiMain_Show
     * stub loaded JK1/static.jkl/items.dat before jumping into a level.  Keep
     * that real engine initialization, but let the real menu choose the level. */
    jkPlayer_hudScale = 1.0f;
    jkGuiTitle_WorldLoadCallback(93.0f);
    jkRes_LoadGob("JK1");
    jkGuiTitle_WorldLoadCallback(96.0f);
    sithMain_Load("static.jkl");
    jkGuiTitle_WorldLoadCallback(98.0f);
    jkHudInv_InitItems();
    jkGuiTitle_WorldLoadCallback(100.0f);
    jkGuiTitle_LoadingStaticFinalizeMenu();

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

    /* Skip video intros — go straight to main menu state */
    jkSmack_nextGuiState = 3; /* JK_GAMEMODE_MAIN */
    jkSmack_stopTick = 1;
    g_app_suspended = 1;

    if (g_app_suspended) { XDBG("main: g_app_suspended=1 OK\n"); }
    else { XDBG("main: WARNING g_app_suspended=0!\n"); }
    XDBG("main: entering game loop\n");

    /* ----------------------------------------------------------------
     * 4. Game loop
     * -------------------------------------------------------------- */
    { static int loopCount = 0;
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
        /* if (loopCount < 5) XDBG("main: -> StartScene\n"); */
        std3D_StartScene();
        /* if (loopCount < 5) XDBG("main: -> GuiAdvance\n"); */
        jkMain_GuiAdvance();
        /* if (loopCount < 5) XDBG("main: -> EndScene\n"); */
        std3D_EndScene();
        /* if (loopCount < 5) XDBG("main: -> Present\n"); */
        std3D_Present();  /* Swap buffers AFTER EndScene, outside scene block */
        /* if (loopCount < 5) XDBG("main: -> Sleep\n"); */
        /* Sleep(1) yields to the kernel scheduler without imposing a
         * long idle.  Sleep(16) was capping us to ~30 fps on hardware:
         * render+logic ≈ 18ms per frame, plus 16ms sleep = ~34ms ≈ 29
         * fps.  With Sleep(1) the loop becomes render-bound and dt
         * shrinks, giving the engine more responsive physics ticks
         * (the engine itself is fully fixed-timestep, so running the
         * outer loop faster doesn't speed up gameplay — it just
         * reduces input-to-display latency and increases dt accuracy). */
        Sleep(1);
    }
    } /* close loopCount scope */

    Window_xbox_Shutdown();
    xbox_debug_Shutdown();
    XDBG("main: clean exit\n");
}
