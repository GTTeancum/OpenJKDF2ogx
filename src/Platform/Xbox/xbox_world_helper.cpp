/*
 * xbox_world_helper.cpp  —  C++ → C linkage bridge for engine globals.
 *
 * The Xbox build compiles every source file with /Tp (force C++) so that
 * COM-based D3D8 headers compile cleanly.  The engine's globals (e.g.
 * sithWorld_pCurrentWorld in src/globals.c) end up with C++ name mangling.
 *
 * std3D.c is structured as a pure C adapter — its function bodies live
 * inside `extern "C"` blocks for symmetric ABI with the engine API.
 * From inside those C-linked bodies, references to C++-mangled engine
 * globals fail to link.
 *
 * This file resolves the mismatch by including globals.h (so it sees the
 * C++-mangled symbols) and exposing tiny `extern "C"` accessor functions
 * that the C-linked adapter can call.
 */

#include "../../globals.h"
#include "xbox_debug.h"

extern "C" void *xbox_get_world_palette(void)
{
    if (!sithWorld_pCurrentWorld) return 0;
    if (!sithWorld_pCurrentWorld->colormaps) return 0;
    return (void *)sithWorld_pCurrentWorld->colormaps->colors;
}

/* Populate stdControl_aJoysticks[idx] so the engine treats the axis as
 * connected.  Mirrors the canonical PC stdControl_InitAxis impl in
 * src/Platform/Common/stdControl.c:545 — sets flags|=1 (axis exists),
 * stores min/max range, computes range-conversion factor.
 *
 * Without this, sithControl_MapAxisFunc bails out at line 428
 * (`if ((stdControl_aJoysticks[dxKeyNum].flags & 1) == 0) return 0;`)
 * and joystick→input-function bindings silently fail to register, so
 * AXIS_JOY1_X/Y/etc. read zero forever even though XInput is producing
 * values per frame. */
extern "C" void xbox_init_joystick_axis(int index, int stickMin, int stickMax)
{
    int center;
    if (index < 0 || index >= JK_NUM_AXES) return;
    center = stickMin + (stickMax - stickMin + 1) / 2;
    stdControl_aJoysticks[index].flags        |= 1;
    stdControl_aJoysticks[index].uMinVal       = stickMin;
    stdControl_aJoysticks[index].uMaxVal       = stickMax;
    stdControl_aJoysticks[index].dwXoffs       = center;
    stdControl_aJoysticks[index].dwYoffs       = 0;
    stdControl_aJoysticks[index].fRangeConversion =
        (stickMax - center) ? (1.0f / (float)(stickMax - center)) : 1.0f;
}
