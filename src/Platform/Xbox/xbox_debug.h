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

/* Convenience macros */
#define XDBG(msg)        xbox_debug_Print(msg)
/* VC71 C89 mode doesn't support __VA_ARGS__. Use XDBGF as a direct
   function call instead of a variadic macro. */
#define XDBGF  xbox_debug_Printf


#ifdef __cplusplus
} /* extern "C" */
#endif

#endif /* XBOX_DEBUG_H */