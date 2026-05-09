/*
 * std3D_draw.cpp  --  C++ wrappers for Xbox D3D inline draw methods
 *
 * DrawPrimitive and DrawPrimitiveUP are inline C++ methods defined in
 * D3D8-Xbox.h (they write pushbuffer commands directly).  They are NOT
 * exported from d3d8ltcg.lib — only the Xbox-extension DrawVertices* variants
 * are in the lib, and those hang NV2A on retail hardware.
 *
 * This file must be compiled as C++ (/Tp) to access the inline methods.
 * platform_xbox.h is force-included via /FI before this file's own includes.
 * xtl.h is then included here to pull in D3D8-Xbox.h + the Xbox D3D inline API.
 *
 * The extern "C" wrappers are called from std3D.c (C89) without mangling.
 */

/*
 * xtl.h includes D3D8-Xbox.h → d3d8types-xbox.h (in XDK include path).
 * On our install d3d8types-xbox.h is the replacement file that provides
 * Xbox-correct type layouts.  D3D8-Xbox.h then defines IDirect3DDevice8
 * with all inline draw methods (DrawPrimitive, DrawPrimitiveUP, etc.)
 */
#include <xtl.h>

extern "C" {

/*
 * xbox_DrawPrimitiveUP
 *
 * Wraps IDirect3DDevice8::DrawPrimitiveUP (inline C++ method).
 * Parameters match the standard D3D8 DrawPrimitiveUP signature:
 *   primType    — D3DPRIMITIVETYPE (pass as int for C linkage)
 *   primCount   — number of primitives (triangles), NOT vertex count
 *   pData       — CPU-side vertex data pointer
 *   stride      — bytes per vertex
 *
 * Called from std3D.c for all draw calls.  FakeGLx (D3DQuakeX) uses
 * this path exclusively and is the only confirmed-working D3D renderer
 * on retail Xbox hardware outside of retail games.
 */
void xbox_DrawPrimitiveUP(void *pDev, int primType, unsigned int primCount,
                          const void *pData, unsigned int stride)
{
    IDirect3DDevice8 *dev = (IDirect3DDevice8 *)pDev;
    dev->DrawPrimitiveUP((D3DPRIMITIVETYPE)primType, primCount, pData, stride);
}

} /* extern "C" */
