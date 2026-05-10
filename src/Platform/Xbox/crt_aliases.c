/*
 * crt_aliases.c - CRT symbol aliases and missing symbol definitions
 *
 * The engine code uses underscore-prefixed CRT names (_memcpy, _sprintf, etc.).
 * jk.c provides these on PC but compiles as C++ on Xbox, producing mangled
 * names. This file provides C-linkage versions that call the real CRT.
 *
 * Also provides missing globals and stub functions needed for linking.
 */

#include "platform_xbox.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <math.h>
#include <stdarg.h>
#include <ctype.h>
#include <wchar.h>

/* ================================================================
   CRT string/memory aliases
   ================================================================ */
/* CRT aliases needed by C translation units.
   jk.c provides C++ mangled versions; these are C linkage. */
void* __cdecl _memcpy(void* d, const void* s, size_t n)    { return memcpy(d,s,n); }
char* __cdecl _strtok(char* s, const char* d)               { return strtok(s,d); }
char* __cdecl _strchr(char* s, char c)                       { return strchr(s,(int)c); }
char* __cdecl _strrchr(char* s, char c)                      { return strrchr(s,(int)c); }
char* __cdecl _strncat(char* d, const char* s, size_t n)    { return strncat(d,s,n); }
char* __cdecl _strncpy(char* d, const char* s, size_t n)   { return strncpy(d,s,n); }
int   __cdecl __tolower(int c)                               { return tolower(c); }
char* __cdecl _strpbrk(const char* a, const char* b)        { return strpbrk(a,b); }
size_t __cdecl _strspn(const char* a, const char* b)        { return strspn(a,b); }

/* ================================================================
   CRT wchar aliases
   ================================================================ */
wchar_t* __cdecl _wcsncpy(wchar_t* d, const wchar_t* s, size_t n) { return wcsncpy(d,s,n); }
size_t   __cdecl _wcslen(const wchar_t* s)                  { return wcslen(s); }
wchar_t* __cdecl __wcsncpy(wchar_t* d, const wchar_t* s, size_t n) { return wcsncpy(d,s,n); }
wchar_t* __cdecl __wcschr(const wchar_t* s, wchar_t c)     { return wcschr(s,c); }

/* ================================================================
   CRT conversion/formatting aliases
   ================================================================ */
int   __cdecl _atoi(const char* s)                          { return atoi(s); }
int   __cdecl _rand(void)                                    { return rand(); }
void  __cdecl _qsort(void* b, size_t n, size_t s, int(__cdecl*c)(const void*,const void*)) { qsort(b,n,s,c); }

int   __cdecl _sprintf(char* b, const char* f, ...)         { va_list a; int r; va_start(a,f); r=vsprintf(b,f,a); va_end(a); return r; }
int   __cdecl __vsnprintf(char* b, size_t n, const char* f, va_list a) { return _vsnprintf(b,n,f,a); }
/* _sscanf is now #defined to sscanf in platform_xbox.h.
   The old va_list wrapper was broken (XDK lacks vsscanf). */

/* ================================================================
   CRT case/character aliases
   ================================================================ */
/* _tolower and __tolower provided by libc.lib — do not redefine */
/* __tolower is provided by libc.lib — do not redefine */

int   __cdecl __strcmpi(const char* a, const char* b)       { return _stricmp(a,b); }

void  __cdecl _strtolower(char* s) {
    if (!s) return;
    while (*s) { *s = (char)tolower((unsigned char)*s); s++; }
}

/* ================================================================
   Math
   ================================================================ */
/* ceilf — provided by math lib or rdCache */

/* ================================================================
   printf / exit
   ================================================================ */
int __cdecl jk_printf(const char* fmt, ...) {
    char buf[512];
    va_list a;
    va_start(a, fmt);
    _vsnprintf(buf, sizeof(buf)-1, fmt, a);
    buf[sizeof(buf)-1] = 0;
    va_end(a);
    OutputDebugStringA(buf);
    return 0;
}

int __cdecl _printf(const char* fmt, ...) {
    char buf[512];
    va_list a;
    va_start(a, fmt);
    _vsnprintf(buf, sizeof(buf)-1, fmt, a);
    buf[sizeof(buf)-1] = 0;
    va_end(a);
    OutputDebugStringA(buf);
    return 0;
}

void __cdecl jk_exit(int code) { (void)code; for(;;) Sleep(1000); }

int __cdecl jk_snwprintf(wchar_t* b, size_t n, const wchar_t* f, ...) {
    va_list a;
    int r;
    va_start(a, f);
    r = _vsnwprintf(b, n, f, a);
    va_end(a);
    return r;
}

int __cdecl jk_vsnwprintf(wchar_t* b, size_t n, const wchar_t* f, va_list a) {
    return _vsnwprintf(b, n, f, a);
}

int __cdecl msvc_sub_512D30(int a, int b) { (void)a; (void)b; return 0; }

/* ================================================================
   Win32 stubs
   ================================================================ */
void __cdecl SetCurrentDirectoryA(const char* p) { (void)p; }

/* stdHashTable C-linkage versions are provided by stdGob_xbox.c
   when STDGOB_STANDALONE is defined. */

/* std_pHS is defined with extern "C" in xbox_stubs.c */

/* jkHud — C linkage versions for C callers (jkMain.c) */
int  __cdecl jkHud_Open(void) { return 0; }
void __cdecl jkHud_Close(void) { }

/* HeapValidate is provided by winbase.h / xapilib.lib */
void __cdecl RtlUnwind(void) { }

/* ================================================================
   C-linkage stubs for engine functions declared with extern "C"
   in their headers. These are called from C++ code through
   extern "C" header declarations, so they need C linkage.
   ================================================================ */

/* std3D — real implementation is in std3D.c with extern "C" wrapping.
   These are only needed if std3D.c is NOT in the build (stub mode). */

/* sithMulti — networking, stubbed */
int  sithMulti_Startup(void) { return 0; }
int  sithMulti_Shutdown(void) { return 0; }
void sithMulti_LobbyMessage(void* a) { (void)a; }
void sithMulti_SendWelcome(void) { }
int  sithMulti_FreeThing(void* a) { (void)a; return 0; }
int  sithMulti_GetSpawnIdx(void) { return 0; }
void sithMulti_HandleDeath(void* a, void* b) { (void)a; (void)b; }
void sithMulti_HandleTimeLimit(void) { }

/* jkStrings — string table */
int   jkStrings_Startup(void) { return 1; }
int   jkStrings_Shutdown(void) { return 1; }
void* jkStrings_GetUniStringWithFallback(const char* k) { (void)k; return 0; }

/* stdComm — communications, stubbed */
int  stdComm_Startup(void) { return 1; }
int  stdComm_Shutdown(void) { return 1; }

/* stdHttp — networking, stubbed */
int  stdHttp_Startup(void) { return 1; }
int  stdHttp_Shutdown(void) { return 1; }

/* stdPalEffects — palette, stubbed */
/* stdPalEffects — real impls in src/General/stdPalEffects.c (now in build).
 * Previous stubs here had the wrong signatures (e.g. GetEffectPointer was
 * declared void(void) returning void*, real signature is int(int) returning
 * stdPalEffect*).  Calling code dereferenced the NULL return and hung. */

/* stdJSON — config, stubbed */
void stdJSON_SetString(void* a, const char* b, const char* c) { (void)a; (void)b; (void)c; }
void stdJSON_SaveFloat(void* a, const char* b, float c) { (void)a; (void)b; (void)c; }
void stdJSON_SaveInt(void* a, const char* b, int c) { (void)a; (void)b; (void)c; }
void stdJSON_SaveBool(void* a, const char* b, int c) { (void)a; (void)b; (void)c; }
const char* stdJSON_GetString(void* a, const char* b) { (void)a; (void)b; return ""; }
float stdJSON_GetFloat(void* a, const char* b) { (void)a; (void)b; return 0.0f; }
int   stdJSON_GetInt(void* a, const char* b) { (void)a; (void)b; return 0; }
int   stdJSON_GetBool(void* a, const char* b) { (void)a; (void)b; return 0; }
void  stdJSON_EraseAll(void) { }

/* stdUpdater */
void stdUpdater_StartupCvars(void) { }

/* stdFileUtil — real implementation in stdFile_xbox.c */

/* DirectPlay — stubbed */
void DirectPlay_SetSessionFlagidk(int a) { (void)a; }
void DirectPlay_SetSessionDesc(void* a) { (void)a; }

/* Misc globals (C linkage) */
int jkGuiNetHost_bIsDedicated = 0;

/* CRT */
int __cdecl __strnicmp(const char* a, const char* b, size_t n) { return _strnicmp(a, b, n); }
double __cdecl _atof(const char* s) { return atof(s); }

