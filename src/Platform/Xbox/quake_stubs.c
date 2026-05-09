/*
 * quake_stubs.c — Symbols fakeglx.cpp expects from Quake's C side.
 *
 * fakeglx.cpp was lifted from Microsoft's xquake (Quake-on-Xbox port).
 * It calls Con_Printf and references DIBWidth / DIBHeight as externals.
 * We have neither Quake's console nor the DIB-based windowed surfaces,
 * so this file provides minimal stubs that route Con_Printf to our
 * Xbox debug log and supply zero values for the DIB globals (the latter
 * are unused on the Xbox code path inside fakeglx).
 */

#include <stdarg.h>
#include "xbox_debug.h"

int DIBWidth = 0;
int DIBHeight = 0;

void Con_Printf(char *fmt, ...)
{
    char buf[512];
    va_list args;
    va_start(args, fmt);
    _vsnprintf(buf, sizeof(buf) - 1, fmt, args);
    va_end(args);
    buf[sizeof(buf) - 1] = '\0';
    xbox_debug_Print(buf);
}
