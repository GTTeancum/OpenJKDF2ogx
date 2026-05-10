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
