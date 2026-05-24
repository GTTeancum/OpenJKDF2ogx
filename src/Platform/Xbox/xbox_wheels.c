#ifdef TARGET_XBOX

#include "xbox_wheels.h"
#include "xbox_debug.h"
#include "../../globals.h"
#include "../../types_enums.h"
#include "../../Gameplay/sithInventory.h"
#include "../../General/stdBitmap.h"
#include "../../General/stdString.h"
#include "../../General/util.h"
#include "../../Platform/std3D.h"
#include "../../World/sithWeapon.h"
#include "gl/gl.h"
#include <math.h>
#include <string.h>

#define XBOX_WHEEL_MAX_PORTS 4
#define XBOX_WHEEL_HOLD_MS 260

typedef enum XboxWheelKind
{
    XBOX_WHEEL_NONE = 0,
    XBOX_WHEEL_WEAPON,
    XBOX_WHEEL_FORCE
} XboxWheelKind;

typedef struct XboxWheelButton
{
    int down;
    int opened;
    unsigned int startTick;
} XboxWheelButton;

typedef struct XboxWheelState
{
    XboxWheelKind openKind;
    int selectedWeaponSlot;
    int selectedForceSlot;
    XboxWheelButton weaponPrev;
    XboxWheelButton weaponNext;
    XboxWheelButton forcePrev;
    XboxWheelButton forceNext;
} XboxWheelState;

typedef struct XboxWheelWeaponSlot
{
    int bin;
    const char *name;
    const char *iconPath;
    float iconW;
    float iconH;
} XboxWheelWeaponSlot;

typedef struct XboxWheelForceSlot
{
    int bin;
    const char *name;
    int wheelPos;
} XboxWheelForceSlot;

static XboxWheelState s_wheels[XBOX_WHEEL_MAX_PORTS];
static stdBitmap *s_weaponIcons[10];
static int s_weaponIconTried[10];
static int s_weaponIconLogBudget = 16;

static const XboxWheelWeaponSlot s_weaponSlots[10] =
{
    { SITHBIN_FISTS,              "Fists",              "ui\\bm\\xw_fists.bm",    92.0f, 68.0f },
    { SITHBIN_BRYARPISTOL,        "Bryar Pistol",       "ui\\bm\\xw_bryar.bm",    54.0f, 28.0f },
    { SITHBIN_STORMTROOPER_RIFLE, "Stormtrooper Rifle", "ui\\bm\\xw_strifle.bm",  58.0f, 26.0f },
    { SITHBIN_THERMAL_DETONATOR,  "Thermal Detonator",  "ui\\bm\\xw_thermal.bm",  88.0f, 68.0f },
    { SITHBIN_TUSKEN_PROD,        "Tusken Prod",        "ui\\bm\\xw_tusken.bm",   116.0f, 64.0f },
    { SITHBIN_REPEATER,           "Repeater",           "ui\\bm\\xw_repeater.bm", 58.0f, 28.0f },
    { SITHBIN_RAIL_DETONATOR,     "Rail Detonator",     "ui\\bm\\xw_rail.bm",     87.0f, 45.0f },
    { SITHBIN_SEQUENCER_CHARGE,   "Sequencer Charge",   "ui\\bm\\xw_seqchg.bm",   84.0f, 72.0f },
    { SITHBIN_CONCUSSION_RIFLE,   "Concussion Rifle",   "ui\\bm\\xw_conc.bm",     90.0f, 42.0f },
    { SITHBIN_LIGHTSABER,         "Lightsaber",         "ui\\bm\\xw_saber.bm",    56.0f, 24.0f }
};

static const XboxWheelForceSlot s_forceSlots[14] =
{
    { SITHBIN_F_SEEING,      "Force Seeing",       1 },
    { SITHBIN_F_PULL,        "Force Pull",         2 },
    { SITHBIN_F_THROW,       "Force Throw",        3 },
    { SITHBIN_F_GRIP,        "Force Grip",         4 },
    { SITHBIN_F_LIGHTNING,   "Force Lightning",    5 },
    { SITHBIN_F_DESTRUCTION, "Force Destruction",  6 },
    { SITHBIN_F_DEADLYSIGHT, "Deadly Sight",       7 },
    { SITHBIN_F_PROTECTION,  "Force Protection",   9 },
    { SITHBIN_F_ABSORB,      "Force Absorb",       10 },
    { SITHBIN_F_BLINDING,    "Force Blind",        11 },
    { SITHBIN_F_PERSUASION,  "Force Persuasion",   12 },
    { SITHBIN_F_HEALING,     "Force Heal",         13 },
    { SITHBIN_F_JUMP,        "Force Jump",         14 },
    { SITHBIN_F_SPEED,       "Force Speed",        15 }
};

static sithThing* xbox_wheels_Player(void)
{
    return sithPlayer_pLocalPlayerThing;
}

static int xbox_wheels_ScaleX(float x)
{
    int w = Video_format.width ? (int)Video_format.width : 640;
    return (int)(x * (float)w / 640.0f);
}

static int xbox_wheels_ScaleY(float y)
{
    int h = Video_format.height ? (int)Video_format.height : 480;
    return (int)(y * (float)h / 480.0f);
}

static int xbox_wheels_WeaponSlotForBin(int bin)
{
    int i;
    for (i = 0; i < 10; i++)
        if (s_weaponSlots[i].bin == bin)
            return i;
    return 0;
}

static int xbox_wheels_ForceSlotForBin(int bin)
{
    int i;
    for (i = 0; i < 14; i++)
        if (s_forceSlots[i].bin == bin)
            return i;
    return 0;
}

static float xbox_wheels_ForcePosAngle(int pos)
{
    if (pos < 0 || pos >= 16)
        return 90.0f;
    return 90.0f - ((float)pos * 22.5f);
}

static float xbox_wheels_UIScale(void)
{
    float sx = (float)(Video_format.width ? Video_format.width : 640) / 640.0f;
    float sy = (float)(Video_format.height ? Video_format.height : 480) / 480.0f;
    return (sx < sy) ? sx : sy;
}

static void xbox_wheels_ForcePosUnit(int pos, float *outX, float *outY)
{
    float angle = xbox_wheels_ForcePosAngle(pos) * 0.01745329252f;
    *outX = (float)cos((double)angle);
    *outY = (float)sin((double)angle);
}

static int xbox_wheels_IsWeaponAvailable(sithThing *player, int bin)
{
    if (!player) return 0;
    if (bin == SITHBIN_FISTS) return 1;
    return sithInventory_GetAvailable(player, bin) ||
           sithInventory_GetCarries(player, bin) ||
           sithInventory_GetBinAmount(player, bin) > 0.0f;
}

static int xbox_wheels_IsForceAvailable(sithThing *player, int bin)
{
    if (!player) return 0;
    return sithInventory_GetAvailable(player, bin) ||
           sithInventory_GetCarries(player, bin) ||
           sithInventory_GetBinAmount(player, bin) > 0.0f;
}

static int xbox_wheels_AmmoForWeapon(sithThing *player, int weaponBin)
{
    int ammoBin = -1;
    sithItemInfo *item;
    if (!player) return 0;

    switch (weaponBin)
    {
        case SITHBIN_BRYARPISTOL:
        case SITHBIN_STORMTROOPER_RIFLE:
            ammoBin = SITHBIN_ENERGY;
            break;
        case SITHBIN_THERMAL_DETONATOR:
            ammoBin = SITHBIN_THERMAL_DETONATOR;
            break;
        case SITHBIN_TUSKEN_PROD:
        case SITHBIN_REPEATER:
        case SITHBIN_CONCUSSION_RIFLE:
            ammoBin = SITHBIN_POWER;
            break;
        case SITHBIN_RAIL_DETONATOR:
            ammoBin = SITHBIN_RAILCHARGES;
            break;
        case SITHBIN_SEQUENCER_CHARGE:
            ammoBin = SITHBIN_SEQUENCER_CHARGE;
            break;
        default:
            return 0;
    }

    item = sithInventory_GetBin(player, ammoBin);
    if (!item || item->ammoAmt < 0.0f)
        return 0;
    return (int)item->ammoAmt;
}

static void xbox_wheels_Open(int port, XboxWheelKind kind)
{
    sithThing *player;
    if (port < 0 || port >= XBOX_WHEEL_MAX_PORTS)
        return;

    player = xbox_wheels_Player();
    s_wheels[port].openKind = kind;
    if (player)
    {
        if (kind == XBOX_WHEEL_WEAPON)
            s_wheels[port].selectedWeaponSlot = xbox_wheels_WeaponSlotForBin(sithInventory_GetCurWeapon(player));
        else if (kind == XBOX_WHEEL_FORCE)
            s_wheels[port].selectedForceSlot = xbox_wheels_ForceSlotForBin(sithInventory_GetCurPower(player));
    }
}

static void xbox_wheels_Close(int port, int commit)
{
    sithThing *player;
    XboxWheelState *state;
    if (port < 0 || port >= XBOX_WHEEL_MAX_PORTS)
        return;

    state = &s_wheels[port];
    player = xbox_wheels_Player();
    if (commit && player)
    {
        if (state->openKind == XBOX_WHEEL_WEAPON)
        {
            int slot = state->selectedWeaponSlot;
            int bin = (slot >= 0 && slot < 10) ? s_weaponSlots[slot].bin : -1;
            if (bin >= 0 && xbox_wheels_IsWeaponAvailable(player, bin))
                sithWeapon_SelectWeapon(player, bin, 0);
        }
        else if (state->openKind == XBOX_WHEEL_FORCE)
        {
            int slot = state->selectedForceSlot;
            int bin = (slot >= 0 && slot < 14) ? s_forceSlots[slot].bin : -1;
            if (bin >= 0 && xbox_wheels_IsForceAvailable(player, bin))
                sithInventory_SelectPower(player, bin);
        }
    }
    state->openKind = XBOX_WHEEL_NONE;
}

static void xbox_wheels_UpdateWeaponSelection(int port, float rx, float ry)
{
    int side, row;
    if ((rx * rx + ry * ry) < 0.10f)
        return;
    side = (rx >= 0.0f) ? 1 : 0;
    row = (int)((1.0f - ry) * 2.5f);
    if (row < 0) row = 0;
    if (row > 4) row = 4;
    s_wheels[port].selectedWeaponSlot = side ? (5 + row) : row;
}

static void xbox_wheels_UpdateForceSelection(int port, float rx, float ry)
{
    int i, best = s_wheels[port].selectedForceSlot;
    float bestDist = 9999.0f;
    if ((rx * rx + ry * ry) < 0.10f)
        return;
    for (i = 0; i < 14; i++)
    {
        float sx, sy;
        float dx, dy;
        xbox_wheels_ForcePosUnit(s_forceSlots[i].wheelPos, &sx, &sy);
        dx = rx - sx;
        dy = ry - sy;
        float d = dx * dx + dy * dy;
        if (d < bestDist)
        {
            bestDist = d;
            best = i;
        }
    }
    s_wheels[port].selectedForceSlot = best;
}

static void xbox_wheels_HandleButton(int port, XboxWheelButton *btn, int down,
                                     unsigned int tick, XboxWheelKind kind, int cycleForward)
{
    sithThing *player = xbox_wheels_Player();
    if (down && !btn->down)
    {
        btn->startTick = tick;
        btn->opened = 0;
    }
    if (down && !btn->opened && (tick - btn->startTick) >= XBOX_WHEEL_HOLD_MS)
    {
        btn->opened = 1;
        xbox_wheels_Open(port, kind);
    }
    if (!down && btn->down)
    {
        if (btn->opened)
        {
            xbox_wheels_Close(port, 1);
        }
        else if (player)
        {
            if (kind == XBOX_WHEEL_WEAPON)
            {
                if (cycleForward)
                    sithWeapon_Syncunused1(player);
                else
                    sithWeapon_Syncunused2(player);
            }
            else
            {
                if (cycleForward)
                    sithInventory_SelectPowerFollowing(player);
                else
                    sithInventory_SelectPowerPrior(player);
            }
        }
    }
    btn->down = down ? 1 : 0;
}

void xbox_wheels_UpdateInput(int port, unsigned int tick, int gameplay,
                             int blackDown, int whiteDown, int yDown, int bDown,
                             float rightX, float rightY)
{
    XboxWheelState *state;
    if (port < 0 || port >= XBOX_WHEEL_MAX_PORTS)
        return;

    state = &s_wheels[port];
    if (!gameplay)
    {
        memset(state, 0, sizeof(*state));
        return;
    }

    xbox_wheels_HandleButton(port, &state->weaponPrev, blackDown, tick, XBOX_WHEEL_WEAPON, 0);
    xbox_wheels_HandleButton(port, &state->weaponNext, whiteDown, tick, XBOX_WHEEL_WEAPON, 1);
    xbox_wheels_HandleButton(port, &state->forcePrev, yDown, tick, XBOX_WHEEL_FORCE, 0);
    xbox_wheels_HandleButton(port, &state->forceNext, bDown, tick, XBOX_WHEEL_FORCE, 1);

    if (state->openKind == XBOX_WHEEL_WEAPON)
        xbox_wheels_UpdateWeaponSelection(port, rightX, rightY);
    else if (state->openKind == XBOX_WHEEL_FORCE)
        xbox_wheels_UpdateForceSelection(port, rightX, rightY);
}

int xbox_wheels_IsOpenForPort(int port)
{
    if (port < 0 || port >= XBOX_WHEEL_MAX_PORTS)
        return 0;
    return s_wheels[port].openKind != XBOX_WHEEL_NONE;
}

int xbox_wheels_IsAnyOpen(void)
{
    int i;
    for (i = 0; i < XBOX_WHEEL_MAX_PORTS; i++)
        if (s_wheels[i].openKind != XBOX_WHEEL_NONE)
            return 1;
    return 0;
}

int xbox_wheels_ShouldSuppressLook(int port)
{
    return xbox_wheels_IsOpenForPort(port);
}

int xbox_wheels_ActivateInventoryBin(int binIdx)
{
    sithThing *player = xbox_wheels_Player();
    if (!player)
        return 0;
    return sithInventory_BinSendActivate(player, binIdx);
}

static void xbox_wheels_Rect(float x, float y, float w, float h,
                             unsigned char r, unsigned char g, unsigned char b, unsigned char a)
{
    rdRect rect;
    rect.x = xbox_wheels_ScaleX(x);
    rect.y = xbox_wheels_ScaleY(y);
    rect.width = xbox_wheels_ScaleX(w);
    rect.height = xbox_wheels_ScaleY(h);
    std3D_DrawUIClearedRectRGBA(r, g, b, a, &rect);
}

static void xbox_wheels_AnnularSegment(float cx, float cy, float innerR, float outerR,
                                       float startDeg, float endDeg,
                                       unsigned char r, unsigned char g,
                                       unsigned char b, unsigned char a)
{
    int i;
    const int steps = 10;
    float sx = (float)(Video_format.width ? Video_format.width : 640) / 640.0f;
    float sy = (float)(Video_format.height ? Video_format.height : 480) / 480.0f;
    float x = cx * sx;
    float y = cy * sy;
    float inX = innerR * sx;
    float inY = innerR * sy;
    float outX = outerR * sx;
    float outY = outerR * sy;
    float a0 = startDeg * 0.01745329252f;
    float a1 = endDeg * 0.01745329252f;

    glEnable(GL_BLEND);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    glDisable(GL_TEXTURE_2D);
    glBegin(GL_TRIANGLES);
    glColor4f((float)r / 255.0f, (float)g / 255.0f, (float)b / 255.0f, (float)a / 255.0f);
    for (i = 0; i < steps; i++)
    {
        float t0 = a0 + (a1 - a0) * (float)i / (float)steps;
        float t1 = a0 + (a1 - a0) * (float)(i + 1) / (float)steps;
        float c0 = (float)cos((double)t0);
        float s0 = (float)sin((double)t0);
        float c1 = (float)cos((double)t1);
        float s1 = (float)sin((double)t1);

        glVertex3f(x + c0 * inX,  y - s0 * inY,  0.0f);
        glVertex3f(x + c0 * outX, y - s0 * outY, 0.0f);
        glVertex3f(x + c1 * outX, y - s1 * outY, 0.0f);

        glVertex3f(x + c0 * inX,  y - s0 * inY,  0.0f);
        glVertex3f(x + c1 * outX, y - s1 * outY, 0.0f);
        glVertex3f(x + c1 * inX,  y - s1 * inY,  0.0f);
    }
    glEnd();
}

static void xbox_wheels_DrawWedge(float cx, float cy, float innerR, float outerR,
                                  float startDeg, float endDeg, int selected, int available)
{
    unsigned char fill = available ? 162 : 92;
    unsigned char alpha = selected ? 150 : 102;

    xbox_wheels_AnnularSegment(cx, cy, innerR - 2.5f, outerR + 2.5f,
                               startDeg - 0.8f, endDeg + 0.8f,
                               54, 54, 54, selected ? 190 : 140);
    xbox_wheels_AnnularSegment(cx, cy, innerR, outerR, startDeg, endDeg,
                               selected ? 205 : fill,
                               selected ? 205 : fill,
                               selected ? 205 : fill,
                               alpha);
    xbox_wheels_AnnularSegment(cx, cy, innerR + 4.0f, outerR - 4.0f,
                               startDeg + 1.0f, endDeg - 1.0f,
                               selected ? 238 : 210,
                               selected ? 238 : 210,
                               selected ? 238 : 210,
                               selected ? 68 : 32);
}

static void xbox_wheels_SegmentCenter(float cx, float cy, float radius,
                                      float startDeg, float endDeg,
                                      float *outX, float *outY)
{
    float mid = ((startDeg + endDeg) * 0.5f) * 0.01745329252f;
    *outX = cx + (float)cos((double)mid) * radius;
    *outY = cy - (float)sin((double)mid) * radius;
}

static void xbox_wheels_DrawBitmapCentered(stdBitmap *bitmap, float cx, float cy,
                                           float maxW, float maxH, int available)
{
    stdVBuffer *vbuf;
    float w, h, scaleX, scaleY, scale;
    if (!bitmap || !bitmap->mipSurfaces || !bitmap->mipSurfaces[0])
        return;

    vbuf = bitmap->mipSurfaces[0];
    w = (float)vbuf->format.width_in_pixels;
    h = (float)vbuf->format.height;
    if (w <= 0.0f || h <= 0.0f)
        return;

    scaleX = (float)xbox_wheels_ScaleX(maxW) / w;
    scaleY = (float)xbox_wheels_ScaleY(maxH) / h;
    scale = (scaleX < scaleY) ? scaleX : scaleY;
    if (scale <= 0.0f)
        scale = 1.0f;

    std3D_DrawUIBitmapRGBA(bitmap, 0,
                           (float)xbox_wheels_ScaleX(cx) - (w * scale * 0.5f),
                           (float)xbox_wheels_ScaleY(cy) - (h * scale * 0.5f),
                           NULL, scale, scale, 1,
                           available ? 255 : 95,
                           available ? 255 : 95,
                           available ? 255 : 95,
                           available ? 245 : 155);
}

static void xbox_wheels_DrawWeaponIcon(int slot, float cx, float cy, int available)
{
    if (slot < 0 || slot >= 10)
        return;
    if (!s_weaponIconTried[slot])
    {
        s_weaponIconTried[slot] = 1;
        s_weaponIcons[slot] = stdBitmap_LoadPartial((char*)s_weaponSlots[slot].iconPath, 1, 0);
        if (s_weaponIconLogBudget > 0)
        {
            XPERF("Smoke: WheelIcon load slot=%d path='%s' bitmap=%p\n",
                  slot, s_weaponSlots[slot].iconPath, (void*)s_weaponIcons[slot]);
            s_weaponIconLogBudget--;
        }
    }

    if (s_weaponIcons[slot])
        xbox_wheels_DrawBitmapCentered(s_weaponIcons[slot], cx, cy,
                                       s_weaponSlots[slot].iconW,
                                       s_weaponSlots[slot].iconH,
                                       available);

    if (!available)
        xbox_wheels_Rect(cx - 28.0f, cy - 20.0f, 56.0f, 40.0f, 92, 92, 92, 120);
}

static void xbox_wheels_DrawForceIcon(int bin, float cx, float cy, int available)
{
    sithThing *player = xbox_wheels_Player();
    sithItemDescriptor *desc;
    if (!player)
        return;
    desc = sithInventory_GetItemDesc(player, bin);
    if (desc && desc->hudBitmap)
        xbox_wheels_DrawBitmapCentered(desc->hudBitmap, cx, cy, 23.0f, 23.0f, available);
}

static void xbox_wheels_DrawText(stdFont *font, const char *text, int greyed)
{
    int width, x;
    float scale;
    if (!font || !text)
        return;
    scale = xbox_wheels_UIScale();
    width = (int)stdFont_DrawAsciiWidth(font, 0, 0, 999, text, 1, scale);
    x = xbox_wheels_ScaleX(320.0f) - (width / 2);
    stdFont_DrawAsciiGPU(font, (unsigned int)x, xbox_wheels_ScaleY(420.0f), 999, text, greyed ? 0 : 1, scale);
}

static void xbox_wheels_DrawWeapon(stdFont *font, XboxWheelState *state)
{
    sithThing *player = xbox_wheels_Player();
    int i;
    char text[96];
    const float leftStarts[5] = { 133.0f, 157.0f, 181.0f, 205.0f, 229.0f };
    const float rightStarts[5] = { 47.0f, 23.0f, -1.0f, -25.0f, -49.0f };
    if (!player)
        return;

    for (i = 0; i < 10; i++)
    {
        float startDeg = (i < 5) ? leftStarts[i] : rightStarts[i - 5];
        float endDeg = (i < 5) ? (startDeg + 19.0f) : (startDeg - 19.0f);
        int available = xbox_wheels_IsWeaponAvailable(player, s_weaponSlots[i].bin);
        int selected = (i == state->selectedWeaponSlot);
        if (i >= 5)
        {
            float tmp = startDeg;
            startDeg = endDeg;
            endDeg = tmp;
        }
        xbox_wheels_DrawWedge(320.0f, 238.0f, 86.0f, 153.0f, startDeg, endDeg, selected, available);
    }
    for (i = 0; i < 10; i++)
    {
        float startDeg = (i < 5) ? leftStarts[i] : rightStarts[i - 5];
        float endDeg = (i < 5) ? (startDeg + 19.0f) : (startDeg - 19.0f);
        float iconX, iconY;
        int available = xbox_wheels_IsWeaponAvailable(player, s_weaponSlots[i].bin);
        if (i >= 5)
        {
            float tmp = startDeg;
            startDeg = endDeg;
            endDeg = tmp;
        }
        xbox_wheels_SegmentCenter(320.0f, 238.0f, 122.0f, startDeg, endDeg, &iconX, &iconY);
        xbox_wheels_DrawWeaponIcon(i, iconX, iconY, available);
    }

    if (state->selectedWeaponSlot < 0 || state->selectedWeaponSlot >= 10)
        state->selectedWeaponSlot = 0;
    stdString_snprintf(text, sizeof(text), "%s - (%d)",
                       s_weaponSlots[state->selectedWeaponSlot].name,
                       xbox_wheels_AmmoForWeapon(player, s_weaponSlots[state->selectedWeaponSlot].bin));
    xbox_wheels_DrawText(font, text,
                         !xbox_wheels_IsWeaponAvailable(player, s_weaponSlots[state->selectedWeaponSlot].bin));
}

static void xbox_wheels_DrawForce(stdFont *font, XboxWheelState *state)
{
    sithThing *player = xbox_wheels_Player();
    int i;
    char text[96];
    if (!player)
        return;

    for (i = 0; i < 14; i++)
    {
        float centerDeg = xbox_wheels_ForcePosAngle(s_forceSlots[i].wheelPos);
        float startDeg = centerDeg - 8.2f;
        float endDeg = centerDeg + 8.2f;
        int available = xbox_wheels_IsForceAvailable(player, s_forceSlots[i].bin);
        int selected = (i == state->selectedForceSlot);
        xbox_wheels_DrawWedge(320.0f, 238.0f, 86.0f, 153.0f, startDeg, endDeg, selected, available);
    }
    for (i = 0; i < 14; i++)
    {
        float centerDeg = xbox_wheels_ForcePosAngle(s_forceSlots[i].wheelPos);
        float startDeg = centerDeg - 8.2f;
        float endDeg = centerDeg + 8.2f;
        float iconX, iconY;
        int available = xbox_wheels_IsForceAvailable(player, s_forceSlots[i].bin);
        xbox_wheels_SegmentCenter(320.0f, 238.0f, 122.0f, startDeg, endDeg, &iconX, &iconY);
        xbox_wheels_DrawForceIcon(s_forceSlots[i].bin, iconX, iconY, available);
    }

    if (state->selectedForceSlot < 0 || state->selectedForceSlot >= 14)
        state->selectedForceSlot = 0;
    stdString_snprintf(text, sizeof(text), "%s - (level %d)",
                       s_forceSlots[state->selectedForceSlot].name,
                       (int)sithInventory_GetBinAmount(player, s_forceSlots[state->selectedForceSlot].bin));
    xbox_wheels_DrawText(font, text,
                         !xbox_wheels_IsForceAvailable(player, s_forceSlots[state->selectedForceSlot].bin));
}

void xbox_wheels_Draw(stdFont *font)
{
    int i;
    for (i = 0; i < XBOX_WHEEL_MAX_PORTS; i++)
    {
        if (s_wheels[i].openKind == XBOX_WHEEL_WEAPON)
        {
            xbox_wheels_DrawWeapon(font, &s_wheels[i]);
            return;
        }
        if (s_wheels[i].openKind == XBOX_WHEEL_FORCE)
        {
            xbox_wheels_DrawForce(font, &s_wheels[i]);
            return;
        }
    }
}

#endif
