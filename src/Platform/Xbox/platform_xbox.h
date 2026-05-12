#ifndef _PLATFORM_XBOX_H
#define _PLATFORM_XBOX_H

/*
 * OpenJKDF2 - Original Xbox Port
 * Platform defines for TARGET_XBOX
 */

/* ── Target identity ───────────────────────────────────────────── */
#define TARGET_XBOX          1
#define SDL2_RENDER          1
#define PLATFORM_NOSOCKETS   1
#define QOL_IMPROVEMENTS     1

/* ── Suppress unavailable features ────────────────────────────── */
#define TARGET_NO_MULTIPLAYER_MENUS  1
#define LINUX_TMP                    1

/* ── Sound backend selection ───────────────────────────────────── */
/* Must be defined before any header that includes types.h          */
#define STDSOUND_XBOX 1

/* ── Xbox CRT compatibility ────────────────────────────────────── */
#define _CRT_SECURE_NO_WARNINGS      1
#define _CRT_NONSTDC_NO_WARNINGS     1

/* ── C99 keyword shims for VS2005 C89 mode ─────────────────────── */
#ifndef inline
#define inline __inline
#endif
#ifndef __func__
#define __func__ __FUNCTION__
#endif

/* ── stdint shim for VS2005 on Xbox (no stdint.h in XDK) ───────── */
#ifndef _STDINT_H_
#define _STDINT_H_
typedef unsigned char      uint8_t;
typedef unsigned short     uint16_t;
typedef unsigned int       uint32_t;
typedef unsigned __int64   uint64_t;
typedef signed char        int8_t;
typedef signed short       int16_t;
typedef signed int         int32_t;
typedef signed __int64     int64_t;
typedef uint32_t           size_t;
typedef int32_t            intptr_t;
typedef uint32_t           uintptr_t;
typedef int64_t            intmax_t;
#endif /* _STDINT_H_ */

/* ── bool shim ─────────────────────────────────────────────────── */
#ifndef __cplusplus
typedef int BOOL;
#ifndef true
#define true  1
#define false 0
#endif
#endif

/* ── Engine config overrides ───────────────────────────────────── */
#ifndef STD3D_MAX_TEXTURES
#define STD3D_MAX_TEXTURES  512
#endif

/* COG VM: use dynamic (heap) stacks instead of embedding 64K entries
   per sithCog struct. Without this, sizeof(sithCog) ~1MB and 91 cogs
   would need ~95MB, exceeding Xbox's 64MB RAM. */
#define COG_DYNAMIC_STACKS
#define COG_DYNAMIC_STACKS_INCREMENT (32)

/* ── Missing POSIX types (satisfy struct declarations in headers) ── */
struct dirent { char d_name[260]; unsigned char d_type; };

/* ── Missing C99/POSIX defines ─────────────────────────────────── */
#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif
#define snprintf  _snprintf

/* XDK has no vsscanf, so the va_list wrapper in crt_aliases.c can't work.
   Map _sscanf directly to sscanf so callsites compile to real sscanf()
   with correct variadic args.  This must precede jk.h inclusion. */
#define _sscanf   sscanf

/* VC71 lacks round() — provide as macro (can't use inline before math.h) */
#define round(x) ((x) >= 0.0 ? (double)(int)((x) + 0.5) : (double)(int)((x) - 0.5))
#define NO_JK_MMAP 1
typedef unsigned short char16_t;

/* ── Xbox SDK headers ──────────────────────────────────────────── */
/* Do NOT include <xtl.h> here — it pulls in d3d8.h/d3dx8.h which have
   COM interfaces that fail in C89 mode. C++ files that need D3D should
   include <xtl.h> directly. For all files we include just the base
   Win32-compat and kernel headers from the XDK.
   These defines mirror what xtl.h sets before its includes. */
#ifndef _X86_
#define _X86_
#endif
#ifndef WINVER
#define WINVER 0x0500
#endif
#include <excpt.h>
#include <stdarg.h>
#include <windef.h>
#include <winbase.h>
#include <xbox.h>
#include <wchar.h>
#include <assert.h>

/* ── Stub out things the Xbox doesn't have ─────────────────────── */
#ifndef HKEY
#define HKEY        void*
#endif

/* ── LSTATUS (registry return type, not in XTL) ─────────────────── */
#ifndef _LSTATUS_DEFINED
typedef LONG LSTATUS;
#define _LSTATUS_DEFINED
#endif

/* ── MCIDEVICEID stub (not in XTL) ──────────────────────────────── */
#ifndef MCIDEVICEID
typedef UINT MCIDEVICEID;
#endif

/* ── Win32 console types not in XTL (needed by stdconsole.h) ───── */
#ifndef _WINCON_
#define _WINCON_
typedef struct _COORD { SHORT X; SHORT Y; } COORD;
typedef struct _SMALL_RECT { SHORT Left; SHORT Top; SHORT Right; SHORT Bottom; } SMALL_RECT;
typedef struct _CONSOLE_SCREEN_BUFFER_INFO {
    COORD      dwSize;
    COORD      dwCursorPosition;
    WORD       wAttributes;
    SMALL_RECT srWindow;
    COORD      dwMaximumWindowSize;
} CONSOLE_SCREEN_BUFFER_INFO;
typedef struct _CONSOLE_CURSOR_INFO { DWORD dwSize; BOOL bVisible; } CONSOLE_CURSOR_INFO;
#define STD_OUTPUT_HANDLE   ((DWORD)-11)
#define STD_INPUT_HANDLE    ((DWORD)-10)
#define STD_ERROR_HANDLE    ((DWORD)-12)
/* Stub console API - Xbox has no Win32 console */
#define GetStdHandle(n)                       ((HANDLE)0)
#define GetConsoleScreenBufferInfo(h,p)       (0)
#define SetConsoleCursorPosition(h,c)         (0)
#define SetConsoleCursorInfo(h,p)             (0)
#define SetConsoleTitle(s)                    (0)
#define WriteConsole(h,b,n,w,r)               (0)
#define FillConsoleOutputCharacter(h,c,n,p,w) (0)
#endif /* _WINCON_ */

/* std_pHS and pHS are defined in xbox_stubs.c with C++ linkage.
   The globals.h declarations (guarded by TARGET_XBOX) are suppressed,
   so all TUs see a consistent declaration from this header. */
#ifdef __cplusplus
struct HostServices;
extern HostServices* std_pHS;
extern HostServices* pHS;
#endif

#endif /* _PLATFORM_XBOX_H */
