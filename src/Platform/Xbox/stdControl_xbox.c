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

#define DIK_ESCAPE      0x01
#define DIK_TAB         0x0F
#define DIK_Q           0x10
#define DIK_E           0x12    /* Next Force Power */
#define DIK_F           0x21    /* Use Force */
#define DIK_X           0x2D    /* Jump */
#define DIK_Z           0x2C
#define DIK_LBRACKET    0x1A    /* Prev Weapon */
#define DIK_RBRACKET    0x1B    /* Next Weapon */
#define DIK_RETURN      0x1C    /* Use Inventory */
#define DIK_LCONTROL    0x1D    /* Crouch */
#define DIK_LSHIFT      0x2A    /* Sprint */
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

#define XBOX_AXIS_MOVE_FB   0
#define XBOX_AXIS_STRAFE    1
#define XBOX_AXIS_LOOK_LR   2
#define XBOX_AXIS_LOOK_UD   3
#define XBOX_NUM_AXES       8

static float g_axisValues[XBOX_NUM_AXES];
static BYTE  g_prevAnalog[8];
static WORD  g_prevButtons;
static int   g_crouchToggle;
static int   g_sprintToggle;
static int   g_connected;
static HANDLE g_hController;
static float g_lookSensX = 2.5f;
static float g_lookSensY = 2.0f;
static int   g_openAttempted;  /* deferred XInputOpen: tried once from ReadControls */

/* Key state array — indexed by DIK_ value */
#define XBOX_NUM_KEYS  256
static unsigned char g_keyDown[XBOX_NUM_KEYS];
static unsigned int  g_keyTime[XBOX_NUM_KEYS];

void stdControl_SetKeydown(int keyNum, int bDown, unsigned int readTime)
{
    if (keyNum < 0 || keyNum >= XBOX_NUM_KEYS) return;
    g_keyDown[keyNum] = (unsigned char)(bDown ? 1 : 0);
    g_keyTime[keyNum] = readTime;
}

void stdControl_SetSDLKeydown(int keyNum, int bDown, unsigned int readTime)
{
    stdControl_SetKeydown(keyNum, bDown, readTime);
}

static float xbox_NormalizeStick(SHORT raw)
{
    if (raw > STICK_DEADZONE)  return (float)(raw - STICK_DEADZONE)  / (float)(32767 - STICK_DEADZONE);
    if (raw < -STICK_DEADZONE) return (float)(raw + STICK_DEADZONE)  / (float)(32767 - STICK_DEADZONE);
    return 0.0f;
}

int stdControl_Startup(void)
{
    /* XInitDevices is called in main() before D3D init — not here.
     * XInputOpen is deferred to the first ReadControls call: USB device
     * enumeration is asynchronous and may not complete by the time
     * stdControl_Startup runs (right after Main_Startup).  By the first
     * ReadControls call the game loop is running and USB is ready. */
    memset(g_axisValues, 0, sizeof(g_axisValues));
    memset(g_prevAnalog, 0, sizeof(g_prevAnalog));
    g_prevButtons = 0; g_crouchToggle = 0; g_sprintToggle = 0;
    g_hController = NULL; g_connected = 0; g_openAttempted = 0;
    XDBG("stdControl_Startup: deferred XInputOpen to first ReadControls\n");
    return 1;
}

void stdControl_Shutdown(void)
{
    if (g_hController) { XInputClose(g_hController); g_hController = NULL; }
    XDBG("stdControl_Shutdown\n");
}
int  stdControl_Open(void)     { return 1; }
int  stdControl_Close(void)    { return 1; }

void stdControl_Flush(void)
{
    memset(g_axisValues, 0, sizeof(g_axisValues));
    memset(g_prevAnalog, 0, sizeof(g_prevAnalog));
    memset(g_keyDown,    0, sizeof(g_keyDown));
    g_prevButtons = 0;
}

void stdControl_ReadControls(void)
{
    XINPUT_STATE state;
    XINPUT_GAMEPAD *pad;
    WORD buttons, changed;
    unsigned int tick;
    int i, cur, prev;

    /* Lazy-open controller on first call.  XInputOpen may block if USB
     * enumeration is not yet complete; deferring to the game loop (after
     * level load) guarantees it is. Attempt only once — if it returns NULL
     * (no controller present) we run forever without input rather than
     * retrying every tick. */
    if (g_hController == NULL && !g_openAttempted)
    {
        g_openAttempted = 1;
        XDBG("stdControl: lazy XInputOpen\n");
        g_hController = XInputOpen(XDEVICE_TYPE_GAMEPAD, XDEVICE_PORT0,
                                   XDEVICE_NO_SLOT, NULL);
        g_connected = (g_hController != NULL) ? 1 : 0;
        XDBGF("stdControl: controller %s (handle=%p)\n",
              g_connected ? "OK" : "not found", (void*)g_hController);
    }

    if (g_hController == NULL)
        return;  /* controller not present */

    if (XInputGetState(g_hController, &state) != ERROR_SUCCESS)
    {
        if (g_connected) { g_connected = 0; XDBG("stdControl: disconnected\n"); }
        XInputClose(g_hController);
        g_hController = NULL;
        return;
    }
    if (!g_connected) { g_connected = 1; XDBG("stdControl: connected\n"); }

    pad  = &state.Gamepad;
    tick = (unsigned int)GetTickCount();

    /* Digital buttons */
    buttons = pad->wButtons;
    changed = buttons ^ g_prevButtons;

    if (changed & XINPUT_GAMEPAD_DPAD_LEFT)  stdControl_SetKeydown(DIK_LBRACKET, (buttons & XINPUT_GAMEPAD_DPAD_LEFT)  ? 1 : 0, tick);
    if (changed & XINPUT_GAMEPAD_DPAD_RIGHT) stdControl_SetKeydown(DIK_RBRACKET, (buttons & XINPUT_GAMEPAD_DPAD_RIGHT) ? 1 : 0, tick);
    if (changed & XINPUT_GAMEPAD_DPAD_UP)    stdControl_SetKeydown(DIK_E,        (buttons & XINPUT_GAMEPAD_DPAD_UP)    ? 1 : 0, tick);
    if (changed & XINPUT_GAMEPAD_DPAD_DOWN)  stdControl_SetKeydown(DIK_Q,        (buttons & XINPUT_GAMEPAD_DPAD_DOWN)  ? 1 : 0, tick);
    if (changed & XINPUT_GAMEPAD_START)      stdControl_SetKeydown(DIK_ESCAPE,   (buttons & XINPUT_GAMEPAD_START)      ? 1 : 0, tick);
    if (changed & XINPUT_GAMEPAD_BACK)       stdControl_SetKeydown(DIK_TAB,      (buttons & XINPUT_GAMEPAD_BACK)       ? 1 : 0, tick);

    /* R3 - sprint toggle */
    if ((changed & XINPUT_GAMEPAD_RIGHT_THUMB) && (buttons & XINPUT_GAMEPAD_RIGHT_THUMB))
    {
        g_sprintToggle = !g_sprintToggle;
        stdControl_SetKeydown(DIK_LSHIFT, g_sprintToggle, tick);
    }
    g_prevButtons = buttons;

    /* Analog buttons — face */
    cur = (pad->bAnalogButtons[XB_BTN_A] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_A] > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(DIK_X,        cur, tick);  /* A = Jump */
    cur = (pad->bAnalogButtons[XB_BTN_X] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_X] > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(DIK_SPACE,    cur, tick);  /* X = Activate */
    cur = (pad->bAnalogButtons[XB_BTN_Y] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_Y] > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(DIK_RETURN,   cur, tick);  /* Y = Use Inventory */
    /* Analog buttons — shoulders */
    cur = (pad->bAnalogButtons[XB_BTN_WHITE] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_WHITE] > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(DIK_F,       cur, tick);  /* White/RB = Use Force */
    cur = (pad->bAnalogButtons[XB_BTN_BLACK] > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_BLACK] > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(DIK_E,       cur, tick);  /* Black/LB = Next Force Power */
    cur = (pad->bAnalogButtons[XB_BTN_RT]    > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_RT]    > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(DIK_RCONTROL, cur, tick);
    cur = (pad->bAnalogButtons[XB_BTN_LT]    > ANALOG_THRESHOLD); prev = (g_prevAnalog[XB_BTN_LT]    > ANALOG_THRESHOLD); if (cur != prev) stdControl_SetKeydown(DIK_Z,        cur, tick);

    /* B - crouch toggle */
    cur  = (pad->bAnalogButtons[XB_BTN_B] > ANALOG_THRESHOLD);
    prev = (g_prevAnalog[XB_BTN_B]         > ANALOG_THRESHOLD);
    if (cur && !prev) { g_crouchToggle = !g_crouchToggle; stdControl_SetKeydown(DIK_LCONTROL, g_crouchToggle, tick); }

    for (i = 0; i < 8; i++) g_prevAnalog[i] = pad->bAnalogButtons[i];

    /* Sticks */
    g_axisValues[XBOX_AXIS_MOVE_FB] =  xbox_NormalizeStick(pad->sThumbLY);
    g_axisValues[XBOX_AXIS_STRAFE]  =  xbox_NormalizeStick(pad->sThumbLX);
    g_axisValues[XBOX_AXIS_LOOK_LR] =  xbox_NormalizeStick(pad->sThumbRX) * g_lookSensX;
    g_axisValues[XBOX_AXIS_LOOK_UD] = -xbox_NormalizeStick(pad->sThumbRY) * g_lookSensY;
}

float stdControl_ReadAxis(int n)        { if(n<0||n>=XBOX_NUM_AXES) return 0.0f; return g_axisValues[n]; }
int   stdControl_ReadAxisRaw(int n)     { if(n<0||n>=XBOX_NUM_AXES) return 0; return (int)(g_axisValues[n]*32767.0f); }
float stdControl_ReadKeyAsAxis(int k)   { (void)k; return 0.0f; }
int   stdControl_ReadAxisAsKey(int n)   { if(n<0||n>=XBOX_NUM_AXES) return 0; return (g_axisValues[n]>0.5f||g_axisValues[n]<-0.5f)?1:0; }
int   stdControl_ReadKey(int keyNum, int *pOut) {
    if (keyNum < 0 || keyNum >= XBOX_NUM_KEYS) return 0;
    if (pOut) *pOut = g_keyDown[keyNum];
    return g_keyDown[keyNum];
}
void  stdControl_FinishRead(void)       { }

void stdControl_ToggleCursor(int a)     { (void)a; }
int  stdControl_ShowCursor(int a)       { (void)a; return 0; }
void stdControl_ToggleMouse(void)       { }
void stdControl_ReadMouse(void)         { }
void stdControl_SetMouseSensitivity(float x, float y) { g_lookSensX=x*2.5f; g_lookSensY=y*2.0f; }

void stdControl_InitAxis(int idx, int mn, int mx, float mult) { (void)mn;(void)mx;(void)mult; if(idx>=0&&idx<XBOX_NUM_AXES) g_axisValues[idx]=0.0f; }
int  stdControl_EnableAxis(unsigned int idx) { (void)idx; return 1; }
void stdControl_Reset(void)                  { memset(g_axisValues,0,sizeof(g_axisValues)); }

void stdControl_ShowSystemKeyboard(void)      { }
void stdControl_HideSystemKeyboard(void)      { }
int  stdControl_IsSystemKeyboardShowing(void) { return 0; }