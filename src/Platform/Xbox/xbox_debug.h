/*
 * xbox_debug.h  —  OpenJKDF2 Xbox debug logging
 *
 * Location:  src/Platform/Xbox/xbox_debug.h
 *
 * Usage:
 *   XDBG("std3D_Startup: device ready\n");
 *   XDBGF("std3D: texture %d x %d uploaded\n", w, h);
 *
 * Output goes to:
 *   - OutputDebugStringA  (visible in CXBX-Reloaded debug output + xemu via GDB)
 *   - D:\debug_openjkdf2.txt on the Xbox HDD (survives crashes, readable after)
 *
 * Call xbox_debug_Startup() once before any XDBG usage.
 * Call xbox_debug_Shutdown() on exit (flushes and closes the file).
 */

#ifndef XBOX_DEBUG_H
#define XBOX_DEBUG_H

#ifdef __cplusplus
extern "C" {
#endif

void xbox_debug_Startup(void);
void xbox_debug_Shutdown(void);
void xbox_debug_Print(const char *msg);
void xbox_debug_Printf(const char *fmt, ...);

/* On-screen debug HUD — text overlay drawn after StartScene's clear.
 * Each glyph is rendered as solid quads (3x5 bitmap font), no textures
 * needed — uses only the proven-working glBegin/glColor/glVertex path.
 *
 *   std3D_DebugLine(idx, "text")        — write fixed text to slot idx
 *   std3D_DebugLineKV(idx, "KEY", val)  — formatted "KEY value"
 *
 * idx ∈ [0, 23].  Lines persist until overwritten.  Pass NULL to blank.
 * Lowercase folds to upper; punctuation: space : - = / .
 *
 * Old flag/counter API kept for compatibility — flags write to slots 0..7,
 * counters to slots 8..11, as "F<idx> ON/.." / "C<idx> <val>". */
void std3D_DebugLine(int idx, const char *text);
void std3D_DebugLineKV(int idx, const char *key, int value);
void std3D_DebugFlag(int idx, int on);
void std3D_DebugCounter(int idx, int val, int max);

/* Returns a pointer to the current world's colormap colors (rdColor24[256])
 * or NULL if no world/colormap loaded yet.  Used as a fallback palette by
 * std3D_AddToTextureCache when the engine hasn't yet called SetCurrentPalette. */
void *xbox_get_world_palette(void);

/* Convenience macros */
#define XDBG(msg)        xbox_debug_Print(msg)
/* VC71 C89 mode doesn't support __VA_ARGS__. Use XDBGF as a direct
   function call instead of a variadic macro. */
#define XDBGF  xbox_debug_Printf


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XBOX_DEBUG_H */