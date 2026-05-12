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

/* Camera-projection parameters bridge for std3D's perspective-matrix
   setup.  Returns 0 if the camera isn't ready (e.g. before
   sithCamera_Open completes).  Used by std3D_DrawRenderList to call
   glFrustum() each frame so the GPU does the perspective projection
   (instead of CPU pre-projecting vertices into screen space, which
   strips W and forces affine UV interpolation). */
extern "C" int xbox_get_camera_params(float* fov_out, float* aspect_out,
                                       float* znear_out, float* zfar_out)
{
    if (!rdCamera_pCurCamera || !rdCamera_pCurCamera->pClipFrustum)
        return 0;
    *fov_out   = (float)rdCamera_pCurCamera->fov;
    *aspect_out= (float)rdCamera_pCurCamera->screenAspectRatio;
    *znear_out = (float)rdCamera_pCurCamera->pClipFrustum->zNear;
    *zfar_out  = (float)rdCamera_pCurCamera->pClipFrustum->zFar;
    return 1;
}

/* Diagnostic accessor: copy the current camera view_matrix into a flat
 * 12-float buffer (rvec[3], lvec[3], uvec[3], scale[3]).  Returns 0 if
 * the camera isn't ready.  Used by std3D bolt diagnostics. */
extern "C" int xbox_get_view_matrix(float* out12)
{
    if (!rdCamera_pCurCamera || !out12) return 0;
    rdMatrix34* vm = &rdCamera_pCurCamera->view_matrix;
    out12[ 0] = (float)vm->rvec.x; out12[ 1] = (float)vm->rvec.y; out12[ 2] = (float)vm->rvec.z;
    out12[ 3] = (float)vm->lvec.x; out12[ 4] = (float)vm->lvec.y; out12[ 5] = (float)vm->lvec.z;
    out12[ 6] = (float)vm->uvec.x; out12[ 7] = (float)vm->uvec.y; out12[ 8] = (float)vm->uvec.z;
    out12[ 9] = (float)vm->scale.x; out12[10] = (float)vm->scale.y; out12[11] = (float)vm->scale.z;
    return 1;
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
