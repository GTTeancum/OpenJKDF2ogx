/*
 * stdControl_xbox.c  —  OpenJKDF2 Xbox controller input  (Phase 5)
 *
 * XDK 5849 XINPUT_GAMEPAD:
 *   WORD  wButtons
 *   BYTE  bAnalogButtons[8]: [0]=A [1]=B [2]=X [3]=Y [4]=Black(LB) [5]=White(RB) [6]=LT [7]=RT
 *   SHORT sThumbLX, sThumbLY, sThumbRX, sThumbRY
 */

#include "platform_xbox.h"
#include "xbox_debug.h"
#include <xtl.h>
#include <string.h>
#include <math.h>

/* Engine extended-key constants (KEY_JOY1_B1..B17, etc.).  These are
 * the DirectInput-equivalent slot numbers the engine binds INPUT_FUNC_*
 * against — must match what sithControl_MapDefaultsJoystick uses. */
#include "../../types_enums.h"
#include "../../Main/jkMain.h"
#include "../../Main/jkSmack.h"
#include "../../globals.h"
#include "../../Platform/wuRegistry.h"
#include "xbox_wheels.h"
#include "xbox_splitscreen.h"

#define DIK_ESCAPE      0x01
#define DIK_TAB         0x0F
#define DIK_Q           0x10
#define DIK_E           0x12    /* Next Force Power */
#define DIK_F           0x21    /* Use Force */
#define DIK_X           0x2D    /* Jump */
#define DIK_Z           0x2C
#define DIK_C           0x2E    /* Duck/Crouch */
#define DIK_LBRACKET    0x1A    /* Prev Weapon */
#define DIK_RBRACKET    0x1B    /* Next Weapon */
#define DIK_RETURN      0x1C    /* Use Inventory */
#define DIK_LCONTROL    0x1D    /* Crouch */
#define DIK_LSHIFT      0x2A    /* Sprint (INPUT_FUNC_FAST) */
#define DIK_CAPITAL     0x3A    /* Walk  (INPUT_FUNC_SLOW)  */
#define DIK_F1          0x3B    /* Camera mode */
#define DIK_LALT        0x38
#define DIK_SPACE       0x39    /* Activate */
#define DIK_RCONTROL    0x9D

#define XB_BTN_A        0
#define XB_BTN_B        1
#define XB_BTN_X        2
#define XB_BTN_Y        3
#define XB_BTN_BLACK    4
#define XB_BTN_WHITE    5
#define XB_BTN_LT       6
#define XB_BTN_RT       7

#define ANALOG_THRESHOLD  30
#define STICK_DEADZONE    3933

/* Axis index layout MUST match the engine's AXIS_JOY1_* constants
 * (types_enums.h:340-345):
 *   index 0 = AXIS_JOY1_X  (horizontal of left stick — TURN)
 *   index 1 = AXIS_JOY1_Y  (vertical of left stick — FORWARD)
 *   index 2 = AXIS_JOY1_Z  (we use for right-stick X — look LR)
 *   index 3 = AXIS_JOY1_R  (we use for right-stick Y — look UD)
 * The engine binds INPUT_FUNC_FORWARD → AXIS_JOY1_Y(=1) etc., so our
 * g_axisValues[1] must contain the left-stick Y value, not LX. */
#define XBOX_AXIS_TURN      0   /* AXIS_JOY1_X — left stick horizontal */
#define XBOX_AXIS_FORWARD   1   /* AXIS_JOY1_Y — left stick vertical   */
#define XBOX_AXIS_LOOK_LR   2   /* AXIS_JOY1_Z — right stick horizontal*/
#define XBOX_AXIS_LOOK_UD   3   /* AXIS_JOY1_R — right stick vertical  */
#define XBOX_NUM_AXES       8
#define XBOX_NUM_KEYS       512
#define XBOX_MAX_CONTROLLERS 4
#define XBOX_CONTROLLER_OPEN_RETRY_MS 10000

typedef struct XboxControllerState
{
    float axisValues[XBOX_NUM_AXES];
    BYTE  prevAnalog[8];
    WORD  prevButtons;
    int   crouchToggle;
    int   sprintToggle;
    int   walkToggle;       /* L3 toggle: 1 = walk (DIK_CAPITAL held) */
    int   connected;
    unsigned int nextOpenAttemptMs; /* deferred XInputOpen retry throttle */
    HANDLE hController;
    unsigned char keyDown[XBOX_NUM_KEYS];      /* current held state */
    unsigned int  keyTime[XBOX_NUM_KEYS];
    unsigned int  keyPress[XBOX_NUM_KEYS];
} XboxControllerState;

static XboxControllerState g_pads[XBOX_MAX_CONTROLLERS];
static int g_activeController = 0;
static int g_pollController = 0;

#define g_axisValues    (g_pads[g_pollController].axisValues)
#define g_prevAnalog    (g_pads[g_pollController].prevAnalog)
#define g_prevButtons   (g_pads[g_pollController].prevButtons)
#define g_crouchToggle  (g_pads[g_pollController].crouchToggle)
#define g_sprintToggle  (g_pads[g_pollController].sprintToggle)
#define g_walkToggle    (g_pads[g_pollController].walkToggle)
#define g_connected     (g_pads[g_pollController].connected)
#define g_nextOpenAttemptMs (g_pads[g_pollController].nextOpenAttemptMs)
#define g_hController   (g_pads[g_pollController].hController)
#define g_keyDown       (g_pads[g_pollController].keyDown)
#define g_keyTime       (g_pads[g_pollController].keyTime)
#define g_keyPress      (g_pads[g_pollController].keyPress)
/* Right-stick sensitivity multipliers.  These compound with the engine's
 * own binaryAxisVal (1.5 for TURN, 1.25 for PITCH set in
 * sithControl_MapDefaultsJoystick).  Halved from the previous 2.5/2.0
 * which felt too twitchy on hardware. */
static float g_lookSensX = 1.25f;
static float g_lookSensY = 1.2f;
static int   g_lookSensitivity = 50;
static int   g_invertLookY = 0;
static int   g_vibrationEnabled = 1;
static int   g_deadzonePercent = 12;
static int   g_stickDeadzone = STICK_DEADZONE;

/* Key state array — indexed by DIK_ value OR engine-extended joy/mouse
 * key index.  Must be >= JK_NUM_KEYS = 0x100 + JK_NUM_EXTENDED_KEYS
 * (types_enums.h:236).  Engine extended keys (JOY1_B1..B17, etc.) live
 * above 0x100, so a 512-slot array safely covers all DIK + joystick +
 * mouse buttons.  Smaller arrays silently drop joy-button writes —
 * the symptom is "right trigger does nothing because KEY_JOY1_B17 is
 * out of range". */
/* Per-frame press-edge counter.  Mirrors stdControl_aInput2 in PC's
 * Common/stdControl.c — increments on each off→on transition,
 * accumulated into the caller's pOut by ReadKey, then reset at the
 * top of each ReadControls poll (same point SDL2/stdControl.c:597
 * zeroes aInput2).  This is what the engine's per-frame
 * "while(readInput--)" style consumers (NEXTWEAPON cycle, etc.) read
 * — without it, a held key would be read as N presses where N =
 * frames-held. */
void stdControl_ReadControls(void);
void stdControl_XboxSetLookOptions(int sensitivity, int invertLook, int vibration);
void stdControl_XboxSetLookOptionsEx(int sensitivity, int invertLook, int vibration, int deadzonePercent);

void stdControl_SetKeydown(int keyNum, int bDown, unsigned int readTime)
{
    if (keyNum < 0 || keyNum >= XBOX_NUM_KEYS) return;
    /* Detect off→on transition for press-counter latch (matches the
     * PC SetKeydown's ++aInput2[keyNum] in Common/stdControl.c:539,
     * which fires only when bDown && !aKeyInfo[keyNum]). */
    if (bDown && !g_keyDown[keyNum])
        g_keyPress[keyNum]++;
    g_keyDown[keyNum] = (unsigned char)(bDown ? 1 : 0);
    g_keyTime[keyNum] = readTime;
}

void stdControl_SetSDLKeydown(int keyNum, int bDown, unsigned int readTime)
{
    stdControl_SetKeydown(keyNum, bDown, readTime);
}

static float xbox_NormalizeStick(SHORT raw)
{
    if (raw > g_stickDeadzone)  return (float)(raw - g_stickDeadzone)  / (float)(32767 - g_stickDeadzone);
    if (raw < -g_stickDeadzone) return (float)(raw + g_stickDeadzone)  / (float)(32767 - g_stickDeadzone);
    return 0.0f;
}

/* Apply a response curve to the right (look) stick.  Linear mapping
 * makes small flicks feel twitchy and large pushes feel weak — squaring
 * the magnitude (signed) gives a flatter low end for precise aim and a
 * faster top end for fast turns.  Exponent 2.0 = quadratic; higher
 * values feel even more "deadzoned" in the middle.  Applied AFTER
 * deadzone+normalize so v is already in [-1, 1]. */
static float xbox_LookCurve(float v, float exponent)
{
    float mag = (v < 0.0f) ? -v : v;
    float curved;
    if (mag <= 0.0f) return 0.0f;
    /* powf is overkill for a fixed exponent of 2; just square */
    if (exponent == 2.0f) curved = mag * mag;
    else                  curved = (float)pow((double)mag, (double)exponent);
    return (v < 0.0f) ? -curved : curved;
}

/* Bridge to xbox_world_helper.cpp — populates engine's
 * stdControl_aJoysticks[idx] table so MapAxisFunc accepts our axes.
 * stdControl_xbox.c is compiled with /Tp (as C++) per build_xbox.bat,
 * so the extern decl must be wrapped to match the C-linked impl. */
#ifdef __cplusplus
extern "C" {
#endif
extern void xbox_init_joystick_axis(int index, int stickMin, int stickMax);
#ifdef __cplusplus
}
#endif

int stdControl_Startup(void)
{
    /* XInitDevices is called in main() before D3D init — not here.
     * XInputOpen is deferred to the first ReadControls call: USB device
     * enumeration is asynchronous and may not complete by the time
     * stdControl_Startup runs (right after Main_Startup).  By the first
     * ReadControls call the game loop is running and USB is ready. */
    memset(g_pads, 0, sizeof(g_pads));
    g_activeController = 0;
    g_pollController = 0;
    stdControl_XboxSetLookOptions(
        wuRegistry_GetInt("xboxLookSensitivity", 50),
        wuRegistry_GetBool("xboxInvertLook", 0),
        wuRegistry_GetBool("xboxVibration", 1));
    stdControl_XboxSetLookOptionsEx(
        g_lookSensitivity,
        g_invertLookY,
        g_vibrationEnabled,
        wuRegistry_GetInt("xboxDeadzone", 12));

    /* Mark our 4 joystick axes as enabled in stdControl_aJoysticks[].
     * Without this, sithControl_MapAxisFunc silently rejects every
     * binding (returns 0 at line 428 because flags & 1 is unset).
     * XInput stick raw range is int16 [-32768..32767]. */
    xbox_init_joystick_axis(XBOX_AXIS_TURN,    -32767, 32767);
    xbox_init_joystick_axis(XBOX_AXIS_FORWARD, -32767, 32767);
    xbox_init_joystick_axis(XBOX_AXIS_LOOK_LR, -32767, 32767);
    xbox_init_joystick_axis(XBOX_AXIS_LOOK_UD, -32767, 32767);

    XDBG("stdControl_Startup: joystick axes 0..3 marked enabled, deferred XInputOpen x4\n");
    return 1;
}

void stdControl_Shutdown(void)
{
    int i;
    for (i = 0; i < XBOX_MAX_CONTROLLERS; i++)
    {
        if (g_pads[i].hController)
        {
            XInputClose(g_pads[i].hController);
            g_pads[i].hController = NULL;
        }
    }
    XDBG("stdControl_Shutdown\n");
}
int  stdControl_Open(void)     { return 1; }
int  stdControl_Close(void)    { return 1; }

void stdControl_Flush(void)
{
    int i;
    for (i = 0; i < XBOX_MAX_CONTROLLERS; i++)
    {
        memset(g_pads[i].axisValues, 0, sizeof(g_pads[i].axisValues));
        memset(g_pads[i].keyDown,    0, sizeof(g_pads[i].keyDown));
        memset(g_pads[i].keyPress,   0, sizeof(g_pads[i].keyPress));
        memset(g_pads[i].prevAnalog, 0, sizeof(g_pads[i].prevAnalog));
        g_pads[i].prevButtons = 0;
    }
    stdControl_ReadControls();
}

static void stdControl_ReadController(int port)
{
    XINPUT_STATE state;
    XINPUT_GAMEPAD *pad;
    WORD buttons, changed;
    unsigned int tick;
    int i, cur, prev;
    int gameplay;
    int wheelOpen;

    /* Reset the press-edge accumulator at the top of every poll, matching
     * SDL2/stdControl.c:597 which `_memset(stdControl_aInput2, 0, ...)`
     * just before ReadControls.  Held keys keep g_keyDown set; the per-
     * frame press count is rebuilt from SetKeydown's off→on edge
     * detection during the analog/digital button pass below. */
    memset(g_keyPress, 0, sizeof(g_keyPress));

    tick = (unsigned int)GetTickCount();

    /* Lazy-open controller, but keep retrying slowly.  Retail Xbox code
     * checks the present-device mask before XInputOpen and handles removals
     * before insertions in the outer ReadControls loop. */
    if (g_hController == NULL && tick >= g_nextOpenAttemptMs)
    {
        DWORD mask = XGetDevices(XDEVICE_TYPE_GAMEPAD);
        g_nextOpenAttemptMs = tick + XBOX_CONTROLLER_OPEN_RETRY_MS;
        XDBGF("stdControl: XInputOpen try port=%d mask=0x%08X\n", port, (unsigned int)mask);
        if (mask & (1u << port))
        {
            g_hController = XInputOpen(XDEVICE_TYPE_GAMEPAD, XDEVICE_PORT0 + port,
                                       XDEVICE_NO_SLOT, NULL);
            g_connected = (g_hController != NULL) ? 1 : 0;
            XDBGF("stdControl: controller %s (handle=%p err=%lu)\n",
                  g_connected ? "OK" : "not found", (void*)g_hController,
                  g_connected ? 0ul : (unsigned long)GetLastError());
            if (g_connected && !g_pads[g_activeController].connected)
                g_activeController = port;
        }
    }

    if (g_hController == NULL)
        return;  /* controller not present */

    if (XInputGetState(g_hController, &state) != ERROR_SUCCESS)
    {
        if (g_connected) { g_connected = 0; XDBG("stdControl: disconnected\n"); }
        XInputClose(g_hController);
        g_hController = NULL;
        g_nextOpenAttemptMs = tick + XBOX_CONTROLLER_OPEN_RETRY_MS;
        return;
    }
    if (!g_connected) { g_connected = 1; XDBG("stdControl: connected\n"); }
    if (g_activeController == port || !g_pads[g_activeController].connected)
        g_activeController = port;

    pad  = &state.Gamepad;
    /* Digital buttons */
    buttons = pad->wButtons;
    changed = buttons ^ g_prevButtons;
    gameplay = (jkSmack_GetCurrentGuiState() == JK_GAMEMODE_GAMEPLAY);
    wheelOpen = xbox_wheels_IsOpenForPort(port);

    if (gameplay)
    {
        /* D-pad shortcuts: Up=field light, Left=IR goggles,
         * Right=bacta, Down=toggle third-person camera. */
        if ((changed & XINPUT_GAMEPAD_DPAD_UP) && (buttons & XINPUT_GAMEPAD_DPAD_UP))
            xbox_wheels_ActivateInventoryBin(SITHBIN_FIELDLIGHT);
        if ((changed & XINPUT_GAMEPAD_DPAD_LEFT) && (buttons & XINPUT_GAMEPAD_DPAD_LEFT))
            xbox_wheels_ActivateInventoryBin(SITHBIN_IRGOGGLES);
        if ((changed & XINPUT_GAMEPAD_DPAD_RIGHT) && (buttons & XINPUT_GAMEPAD_DPAD_RIGHT))
            xbox_wheels_ActivateInventoryBin(SITHBIN_BACTATANK);
        if (changed & XINPUT_GAMEPAD_DPAD_DOWN)
            stdControl_SetKeydown(DIK_F1, (buttons & XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : 0, tick);
    }
    else
    {
        if (changed & XINPUT_GAMEPAD_DPAD_UP)
            stdControl_SetKeydown(KEY_JOY1_HUP, (buttons & XINPUT_GAMEPAD_DPAD_UP) ? 1 : 0, tick);
        if (changed & XINPUT_GAMEPAD_DPAD_DOWN)
            stdControl_SetKeydown(KEY_JOY1_HDOWN, (buttons & XINPUT_GAMEPAD_DPAD_DOWN) ? 1 : 0, tick);
        if (changed & XINPUT_GAMEPAD_DPAD_LEFT)
            stdControl_SetKeydown(KEY_JOY1_HLEFT, (buttons & XINPUT_GAMEPAD_DPAD_LEFT) ? 1 : 0, tick);
        if (changed & XINPUT_GAMEPAD_DPAD_RIGHT)
            stdControl_SetKeydown(KEY_JOY1_HRIGHT, (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : 0, tick);
    }
    /* Start → toggle pause (sithTime_Pause/Resume).  No menu — just freeze
     * the world clock.  When the menu is wired later this will move to a
     * proper "open pause overlay" call, but for now: press Start, sithTime
     * stops; press again, it resumes.  Avoids the DIK_ESCAPE → menu route
     * that we don't have a working menu for yet. */
    if (!wheelOpen && (changed & XINPUT_GAMEPAD_START))
    {
        int down = (buttons & XINPUT_GAMEPAD_START) ? 1 : 0;
        if (gameplay && down && jkSmack_currentGuiState != JK_GAMEMODE_ESCAPE)
        {
            XDBGF("StartPause: state=%d stop=%d -> escape\n", jkSmack_currentGuiState, jkSmack_stopTick);
            jkSmack_nextGuiState = JK_GAMEMODE_ESCAPE;
            jkSmack_stopTick = 1;
        }
        if (gameplay)
            stdControl_SetKeydown(DIK_ESCAPE, down, tick);
        else
            stdControl_SetKeydown(KEY_JOY1_B7, down, tick);
    }
    if (!wheelOpen && (changed & XINPUT_GAMEPAD_BACK))
        stdControl_SetKeydown(DIK_TAB, (buttons & XINPUT_GAMEPAD_BACK) ? 1 : 0, tick);

    /* L3 - sprint toggle (DIK_LSHIFT = INPUT_FUNC_FAST) */
    if ((changed & XINPUT_GAMEPAD_LEFT_THUMB) && (buttons & XINPUT_GAMEPAD_LEFT_THUMB))
    {
        g_sprintToggle = !g_sprintToggle;
        stdControl_SetKeydown(DIK_LSHIFT, g_sprintToggle, tick);
    }
    /* R3 - crouch toggle. */
    if ((changed & XINPUT_GAMEPAD_RIGHT_THUMB) && (buttons & XINPUT_GAMEPAD_RIGHT_THUMB))
    {
        g_crouchToggle = !g_crouchToggle;
        stdControl_SetKeydown(DIK_C, g_crouchToggle, tick);
    }
    g_prevButtons = buttons;

    /* Analog buttons — face */
    cur = (pad->bAnalogButtons[XB_BTN_A] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_A] > ANALOG_THRESHOLD);
    if (cur != prev) {
        if (jkSmack_GetCurrentGuiState() != JK_GAMEMODE_GAMEPLAY)
            stdControl_SetKeydown(KEY_JOY1_B1, cur, tick); /* GUI confirm */
        stdControl_SetKeydown(DIK_X, cur, tick);  /* A = Jump (DIK_X = 0x2D bound to INPUT_FUNC_JUMP at sithControl.c:1916) */
    }
    cur = (pad->bAnalogButtons[XB_BTN_X] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_X] > ANALOG_THRESHOLD); if (cur != prev) { stdControl_SetKeydown(KEY_JOY1_B3, cur, tick); stdControl_SetKeydown(DIK_SPACE, cur, tick); }  /* X = GUI OK shortcut / Activate */
    cur = (pad->bAnalogButtons[XB_BTN_Y] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_Y] > ANALOG_THRESHOLD); if (cur != prev && !gameplay) stdControl_SetKeydown(KEY_JOY1_B4, cur, tick);
    cur = (pad->bAnalogButtons[XB_BTN_WHITE] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_WHITE] > ANALOG_THRESHOLD); if (cur != prev && !gameplay) stdControl_SetKeydown(KEY_JOY1_B10, cur, tick);
    cur = (pad->bAnalogButtons[XB_BTN_BLACK] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_BLACK] > ANALOG_THRESHOLD); if (cur != prev && !gameplay) stdControl_SetKeydown(KEY_JOY1_B11, cur, tick);
    /* Black/White and Y/B are handled by xbox_wheels_UpdateInput below:
     * taps cycle, holds open the weapon/Force wheel. */
    /* Triggers → joystick fire buttons.  Engine's MapDefaultsJoystick
     * (sithControl.c:2399-2400) binds INPUT_FUNC_FIRE1 to KEY_JOY1_B17
     * and INPUT_FUNC_FIRE2 to KEY_JOY1_B16, so we write directly into
     * those slots — no DIK round-trip.  RT=fire1 (rtrig), LT=fire2 (ltrig). */
    cur = (pad->bAnalogButtons[XB_BTN_RT]    > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_RT]    > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(KEY_JOY1_B17, cur, tick);
    cur = (pad->bAnalogButtons[XB_BTN_LT]    > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_LT]    > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(KEY_JOY1_B16, cur, tick);

    /* B stays GUI cancel outside gameplay.  In gameplay it belongs to
     * Force cycling/wheel; R3 owns crouch. */
    cur  = (pad->bAnalogButtons[XB_BTN_B] > ANALOG_THRESHOLD);
    prev = (g_prevAnalog[XB_BTN_B]         > ANALOG_THRESHOLD);
    if (!gameplay && cur != prev) stdControl_SetKeydown(KEY_JOY1_B2, cur, tick); /* GUI cancel */

    for (i = 0; i < 8; i++) g_prevAnalog[i] = pad->bAnalogButtons[i];

    /* Sticks — match engine's AXIS_JOY1_* index layout.
     *
     * Polarity (verified on hardware):
     *  - sThumbLY: pushing UP yields a NEGATIVE short, so we negate
     *    to get the engine's "forward is positive" convention.
     *  - sThumbLX: strafe out-of-the-box, no negation.
     *  - sThumbRX: turn-right matches engine TURN polarity directly,
     *    no negation. (Earlier negation flipped it the wrong way.)
     *  - sThumbRY: was already negated for "look-up = positive PITCH"
     *    convention; keep as-is. */
    /* Left stick: DIGITAL only (snap to {-1, 0, +1}).
     * Console-FPS feel — forward/back and strafe run at full speed or not
     * at all.  Sprint/walk modulate that speed via the FAST/SLOW keys.
     * Analog left-stick magnitude makes pacing fights for fine movement
     * which doesn't fit how JK plays.  Only the right (look) stick stays
     * analog — that's what needs the continuous range for aim precision.
     * Threshold 0.35 ≈ ~30% stick travel beyond the deadzone. */
    {
        float lx = xbox_NormalizeStick(pad->sThumbLX);
        float ly = -xbox_NormalizeStick(pad->sThumbLY);
        const float DIGITAL_THRESHOLD = 0.35f;
        g_axisValues[XBOX_AXIS_TURN]    = (lx >  DIGITAL_THRESHOLD) ?  1.0f
                                        : (lx < -DIGITAL_THRESHOLD) ? -1.0f
                                        :  0.0f;
        g_axisValues[XBOX_AXIS_FORWARD] = (ly >  DIGITAL_THRESHOLD) ?  1.0f
                                        : (ly < -DIGITAL_THRESHOLD) ? -1.0f
                                        :  0.0f;
    }
    /* Look (right stick): apply quadratic response curve before sensitivity
     * multiplier.  Small inputs become much smaller (precise aim), large
     * inputs stay near full speed.  Feels much less twitchy than linear. */
    xbox_wheels_UpdateInput(port, tick, gameplay,
                            pad->bAnalogButtons[XB_BTN_BLACK] > ANALOG_THRESHOLD,
                            pad->bAnalogButtons[XB_BTN_WHITE] > ANALOG_THRESHOLD,
                            pad->bAnalogButtons[XB_BTN_Y] > ANALOG_THRESHOLD,
                            pad->bAnalogButtons[XB_BTN_B] > ANALOG_THRESHOLD,
                            xbox_NormalizeStick(pad->sThumbRX),
                            -xbox_NormalizeStick(pad->sThumbRY));

    if (xbox_wheels_ShouldSuppressLook(port))
    {
        g_axisValues[XBOX_AXIS_LOOK_LR] = 0.0f;
        g_axisValues[XBOX_AXIS_LOOK_UD] = 0.0f;
    }
    else
    {
        g_axisValues[XBOX_AXIS_LOOK_LR] =  xbox_LookCurve(xbox_NormalizeStick(pad->sThumbRX), 2.0f) * g_lookSensX;
        g_axisValues[XBOX_AXIS_LOOK_UD] = -xbox_LookCurve(xbox_NormalizeStick(pad->sThumbRY), 2.0f) * g_lookSensY;
    }
    if (g_invertLookY)
        g_axisValues[XBOX_AXIS_LOOK_UD] = -g_axisValues[XBOX_AXIS_LOOK_UD];

    /* Per-call axis log: first 3 calls only, just for boot sanity.  No
     * periodic spam — it floods D:\debug_openjkdf2.txt over time. */
    { static int _r = 0;
      if (0) {
        XDBGF("ReadCtl: btns=%04X LX=%d LY=%d RX=%d RY=%d -> ax[%f %f %f %f]\n",
              (unsigned)pad->wButtons,
              (int)pad->sThumbLX, (int)pad->sThumbLY,
              (int)pad->sThumbRX, (int)pad->sThumbRY,
              (float)g_axisValues[0], (float)g_axisValues[1],
              (float)g_axisValues[2], (float)g_axisValues[3]);
        _r++;
      }
    }
}

float stdControl_ReadAxis(int n)        { if(n<0||n>=XBOX_NUM_AXES) return 0.0f; return g_pads[g_activeController].axisValues[n]; }
int   stdControl_ReadAxisRaw(int n)     { if(n<0||n>=XBOX_NUM_AXES) return 0; return (int)(g_pads[g_activeController].axisValues[n]*32767.0f); }
float stdControl_ReadKeyAsAxis(int k)   { (void)k; return 0.0f; }
int   stdControl_ReadAxisAsKey(int n)   { if(n<0||n>=XBOX_NUM_AXES) return 0; return (g_pads[g_activeController].axisValues[n]>0.5f||g_pads[g_activeController].axisValues[n]<-0.5f)?1:0; }
int   stdControl_ReadKey(int keyNum, int *pOut) {
    /* Mirror PC's Common/stdControl.c:455:
     *   *pOut += stdControl_aInput2[keyNum];   (press-edge count)
     *   result = stdControl_aKeyInfo[keyNum];   (held state)
     *
     * sithControl_ReadFunctionMap:740 iterates every bound key and
     * accumulates *pOut across the whole loop.  Earlier impl
     * OVERWROTE pOut, which the XBOX_NUM_KEYS bump (256→512) made
     * actually destructive — JUMP / ACTIVATE / FIRE final iteration
     * lands on an unbound mouse-button slot and zeroes pOut.
     *
     * Accumulate the PRESS counter (g_keyPress), not the held state
     * (g_keyDown), so per-frame consumers like
     * `while (readInput--) cycle_weapon();` see one press per
     * physical button-down event, not one per held frame. */
    if (keyNum < 0 || keyNum >= XBOX_NUM_KEYS) return 0;
    if (pOut) *pOut += g_pads[g_activeController].keyPress[keyNum];
    return g_pads[g_activeController].keyDown[keyNum];
}
void  stdControl_FinishRead(void)       { }

void stdControl_ToggleCursor(int a)     { (void)a; }
int  stdControl_ShowCursor(int a)
{
    /* Match Win32 ShowCursor count semantics closely enough for GUI code:
     * visible is non-negative, hidden is negative. jkGuiRend_UpdateCursor()
     * loops until it sees those signs, so returning 0 for hide hangs. */
    return a ? 0 : -1;
}
void stdControl_ToggleMouse(void)       { }
void stdControl_ReadMouse(void)         { }
void stdControl_SetMouseSensitivity(float x, float y) { g_lookSensX=x*2.5f; g_lookSensY=y*2.0f; }

void stdControl_InitAxis(int idx, int mn, int mx, float mult) { (void)mn;(void)mx;(void)mult; if(idx>=0&&idx<XBOX_NUM_AXES) g_pads[g_activeController].axisValues[idx]=0.0f; }
int  stdControl_EnableAxis(unsigned int idx) { (void)idx; return 1; }
void stdControl_Reset(void)                  { int i; for (i = 0; i < XBOX_MAX_CONTROLLERS; i++) memset(g_pads[i].axisValues,0,sizeof(g_pads[i].axisValues)); }

void stdControl_ShowSystemKeyboard(void)      { }
void stdControl_HideSystemKeyboard(void)      { }
int  stdControl_IsSystemKeyboardShowing(void) { return 0; }

void stdControl_XboxSetLookOptions(int sensitivity, int invertLook, int vibration)
{
    stdControl_XboxSetLookOptionsEx(sensitivity, invertLook, vibration, g_deadzonePercent);
}

void stdControl_XboxSetLookOptionsEx(int sensitivity, int invertLook, int vibration, int deadzonePercent)
{
    float scale;
    if (sensitivity < 1) sensitivity = 1;
    if (sensitivity > 100) sensitivity = 100;
    if (deadzonePercent < 0) deadzonePercent = 0;
    if (deadzonePercent > 30) deadzonePercent = 30;
    g_lookSensitivity = sensitivity;
    g_invertLookY = invertLook ? 1 : 0;
    g_vibrationEnabled = vibration ? 1 : 0;
    g_deadzonePercent = deadzonePercent;
    g_stickDeadzone = (32767 * g_deadzonePercent) / 100;

    scale = (float)g_lookSensitivity / 50.0f;
    g_lookSensX = 1.25f * scale;
    g_lookSensY = 1.2f * scale;
}

void stdControl_ReadControls(void)
{
    int port;
    int oldPoll = g_pollController;
    int splitContext = xboxSplitScreen_IsEnabled();
    DWORD insertions = 0;
    DWORD removals = 0;

    XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals);

    for (port = 0; port < XBOX_MAX_CONTROLLERS; port++)
    {
        g_pollController = port;
        if (removals & (1u << port))
        {
            if (g_hController)
            {
                XInputClose(g_hController);
                g_hController = NULL;
            }
            g_connected = 0;
            g_nextOpenAttemptMs = 0;
            memset(g_axisValues, 0, sizeof(g_pads[port].axisValues));
            memset(g_keyDown, 0, sizeof(g_pads[port].keyDown));
            memset(g_keyPress, 0, sizeof(g_pads[port].keyPress));
            XDBGF("stdControl: controller removed port=%d\n", port);
        }
        if (insertions & (1u << port))
        {
            g_nextOpenAttemptMs = 0;
            XDBGF("stdControl: controller inserted port=%d\n", port);
        }
        if (splitContext)
            xboxSplitScreen_SetContextForControllerPort(port);
        stdControl_ReadController(port);
    }

    g_pollController = oldPoll;
    if (splitContext)
        xboxSplitScreen_RestoreContext();
}

int stdControl_XboxGetLookSensitivity(void) { return g_lookSensitivity; }
int stdControl_XboxGetInvertLook(void) { return g_invertLookY; }
int stdControl_XboxGetVibration(void) { return g_vibrationEnabled; }
int stdControl_XboxGetDeadzone(void) { return g_deadzonePercent; }

void stdControl_XboxSetActiveController(int port)
{
    if (port < 0 || port >= XBOX_MAX_CONTROLLERS)
        port = 0;
    g_activeController = port;
}

int stdControl_XboxGetConnectedMask(void)
{
    int i;
    int mask = 0;
    for (i = 0; i < XBOX_MAX_CONTROLLERS; i++)
    {
        if (g_pads[i].connected)
            mask |= (1 << i);
    }
    return mask;
}

#ifdef __cplusplus
extern "C"
#endif
int stdControl_XboxMovieSkipRequested(int *outPort, const char **outReason)
{
    static const char *kAnalogNames[8] = {
        "A", "B", "X", "Y", "black", "white", "left trigger", "right trigger"
    };
    DWORD insertions = 0;
    DWORD removals = 0;
    DWORD mask;
    unsigned int tick = (unsigned int)GetTickCount();
    int port;

    if (outPort)
        *outPort = -1;
    if (outReason)
        *outReason = "unknown";

    XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &insertions, &removals);
    mask = XGetDevices(XDEVICE_TYPE_GAMEPAD);

    for (port = 0; port < XBOX_MAX_CONTROLLERS; port++)
    {
        XboxControllerState *padState = &g_pads[port];
        if (removals & (1u << port))
        {
            if (padState->hController)
            {
                XInputClose(padState->hController);
                padState->hController = NULL;
            }
            padState->connected = 0;
            padState->nextOpenAttemptMs = 0;
            XDBGF("stdControl: movie poll removed port=%d\n", port);
        }
        if (insertions & (1u << port))
            padState->nextOpenAttemptMs = 0;

        if (!padState->hController && (mask & (1u << port)) &&
            tick >= padState->nextOpenAttemptMs)
        {
            padState->nextOpenAttemptMs = tick + XBOX_CONTROLLER_OPEN_RETRY_MS;
            padState->hController = XInputOpen(XDEVICE_TYPE_GAMEPAD,
                                               XDEVICE_PORT0 + port,
                                               XDEVICE_NO_SLOT, NULL);
            padState->connected = padState->hController ? 1 : 0;
            XDBGF("stdControl: movie poll open port=%d handle=%p err=%lu\n",
                  port, (void*)padState->hController,
                  padState->hController ? 0ul : (unsigned long)GetLastError());
            if (padState->connected && !g_pads[g_activeController].connected)
                g_activeController = port;
        }

        if (padState->hController)
        {
            XINPUT_STATE state;
            XINPUT_GAMEPAD *pad;
            int i;

            if (XInputGetState(padState->hController, &state) != ERROR_SUCCESS)
            {
                XInputClose(padState->hController);
                padState->hController = NULL;
                padState->connected = 0;
                padState->nextOpenAttemptMs = tick + XBOX_CONTROLLER_OPEN_RETRY_MS;
                XDBGF("stdControl: movie poll lost port=%d\n", port);
                continue;
            }

            padState->connected = 1;
            if (g_activeController == port || !g_pads[g_activeController].connected)
                g_activeController = port;

            pad = &state.Gamepad;
            if (pad->bAnalogButtons[XB_BTN_A] > 4)
            {
                if (outPort)
                    *outPort = port;
                if (outReason)
                    *outReason = "A";
                return 1;
            }
            if (pad->bAnalogButtons[XB_BTN_B] > 4)
            {
                if (outPort)
                    *outPort = port;
                if (outReason)
                    *outReason = "B";
                return 1;
            }
            if (pad->bAnalogButtons[XB_BTN_X] > 4)
            {
                if (outPort)
                    *outPort = port;
                if (outReason)
                    *outReason = "X";
                return 1;
            }
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
    }

    return 0;
}

int stdControl_XboxGetControllerKeyPress(int port, int keyNum)
{
    if (port < 0 || port >= XBOX_MAX_CONTROLLERS)
        return 0;
    if (keyNum < 0 || keyNum >= XBOX_NUM_KEYS)
        return 0;
    return (int)g_pads[port].keyPress[keyNum];
}

int stdControl_XboxGetControllerKeyDown(int port, int keyNum)
{
    if (port < 0 || port >= XBOX_MAX_CONTROLLERS)
        return 0;
    if (keyNum < 0 || keyNum >= XBOX_NUM_KEYS)
        return 0;
    return (int)g_pads[port].keyDown[keyNum];
}
