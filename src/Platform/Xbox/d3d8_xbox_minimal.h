/*
 * d3d8_xbox_minimal.h  —  Minimal Xbox D3D8 declarations for std3D.c
 *
 * The XDK's D3D8-Xbox.h requires d3d8types-xbox.h which is incomplete
 * in our XDK install. Instead of shimming hundreds of missing types,
 * this header declares only the flat C functions and types that
 * std3D.c actually calls.
 *
 * Xbox D3D8 uses flat structs (D3DDevice, D3DTexture) with C-linkage
 * helper functions, not COM vtable interfaces like PC D3D8.
 */

#ifndef D3D8_XBOX_MINIMAL_H
#define D3D8_XBOX_MINIMAL_H

/* =====================================================================
 * D3DPRESENT_PARAMETERS: Xbox layout is 68 bytes, NOT the 52-byte PC
 * layout that D3D8Types.h ships with. The XDK 5849 install is missing
 * the real d3d8types-xbox.h that would have overridden this.
 *
 * Microsoft's own XDK CHM doc (_dx_d3dpresent_parameters_graphics.htm)
 * and Cxbx-Reloaded source (src/core/hle/D3D8/XbD3D8Types.h) both
 * confirm the Xbox struct appends:
 *     D3DSurface *BufferSurfaces[3];
 *     D3DSurface *DepthStencilSurface;
 * giving 52 + 16 = 68 bytes. d3d8ltcg.lib's Direct3D_CreateDevice
 * reads all 68 bytes. Passing a 52-byte PC struct causes it to read
 * 16 bytes of stack garbage as surface pointers — D3D then tells the
 * GPU scanout engine to flip to invalid addresses, and Swap hangs
 * forever inside BlockOnFence waiting for a swap that never completes.
 *
 * Fix: rename D3D8Types.h's PC typedef at include time, then define
 * our own struct with the proper Xbox layout. ZeroMemory(d3dpp) now
 * zeros all 68 bytes, leaving the surface pointers NULL — which tells
 * D3D to allocate its own surfaces (the common case).
 * =================================================================== */

/* Build is now against XDK 5558, whose D3D8Types.h already contains the
 * Xbox-correct enum values for D3DPRIMITIVETYPE, D3DRENDERSTATETYPE,
 * D3DTEXTURESTAGESTATETYPE, D3DFORMAT, etc. (verified: D3DPT_TRIANGLELIST=5,
 * D3DRS_ZENABLE=143).  No fabricated d3d8types-xbox.h needed.
 *
 * Forward-declare D3DVertexBuffer so D3DSTREAM_INPUT (referenced inside
 * D3D8Types.h chain) can name it.
 *
 * Rename D3D8Types.h's D3DPRESENT_PARAMETERS so our own (with
 * BufferSurfaces[3] + DepthStencilSurface for the auto-allocated path)
 * can use the canonical name below. */
typedef struct D3DVertexBuffer D3DVertexBuffer;
typedef struct D3DSurface      D3DSurface;

#define _D3DPRESENT_PARAMETERS_ _D3DPRESENT_PARAMETERS_XBHDR_
#define D3DPRESENT_PARAMETERS   D3DPRESENT_PARAMETERS_XBHDR

#include "D3D8Types.h"
#include "D3D8Caps.h"

#undef _D3DPRESENT_PARAMETERS_
#undef D3DPRESENT_PARAMETERS

/* Xbox-correct D3DPRESENT_PARAMETERS (68 bytes) */
typedef struct _D3DPRESENT_PARAMETERS_
{
    UINT                BackBufferWidth;
    UINT                BackBufferHeight;
    D3DFORMAT           BackBufferFormat;
    UINT                BackBufferCount;

    D3DMULTISAMPLE_TYPE MultiSampleType;

    D3DSWAPEFFECT       SwapEffect;
    HWND                hDeviceWindow;
    BOOL                Windowed;
    BOOL                EnableAutoDepthStencil;
    D3DFORMAT           AutoDepthStencilFormat;
    DWORD               Flags;

    UINT                FullScreen_RefreshRateInHz;
    UINT                FullScreen_PresentationInterval;

    /* ---- Xbox extensions below this line; PC D3D8 ends here ---- */
    void               *BufferSurfaces[3];    /* D3DSurface* — if non-NULL, app-provided */
    void               *DepthStencilSurface;  /* D3DSurface* — if non-NULL, app-provided */

} D3DPRESENT_PARAMETERS;

/* =====================================================================
 * Xbox-specific constants
 * =================================================================== */

/* Present flags — from d3d8types-xbox.h */
#ifndef D3DPRESENTFLAG_INTERLACED
#define D3DPRESENTFLAG_INTERLACED    0x00000002
#endif
#ifndef D3DPRESENTFLAG_PROGRESSIVE
#define D3DPRESENTFLAG_PROGRESSIVE   0x00000004
#endif

/* D3DFMT_* values are now correct via d3d8types-xbox.h — no overrides needed.
 * D3DFMT_X8R8G8B8 = 0x07, D3DFMT_D16 = 0x2C, D3DFMT_LIN_X8R8G8B8 = 0x1E, etc. */

/* Video standard / flags constants — from Xbox.h */
#ifndef XC_VIDEO_STANDARD_NTSC_M
#define XC_VIDEO_STANDARD_NTSC_M    1
#define XC_VIDEO_STANDARD_NTSC_J    2
#define XC_VIDEO_STANDARD_PAL_I     3
#endif
#ifndef XC_VIDEO_FLAGS_PAL_60Hz
#define XC_VIDEO_FLAGS_HDTV_480p    0x00000008
#define XC_VIDEO_FLAGS_PAL_60Hz     0x00000040
#endif

#ifndef D3D_SDK_VERSION
/* Must match XDK d3d8.h: Direct3DCreate8 validates this and returns NULL
 * on mismatch. XDK 5849's d3d8.h defines it as 120. */
#define D3D_SDK_VERSION              120
#endif

#ifndef D3DADAPTER_DEFAULT
#define D3DADAPTER_DEFAULT           0
#endif

#ifndef D3DCREATE_HARDWARE_VERTEXPROCESSING
#define D3DCREATE_HARDWARE_VERTEXPROCESSING  0x00000040L
#endif
#ifndef D3DCREATE_SOFTWARE_VERTEXPROCESSING
#define D3DCREATE_SOFTWARE_VERTEXPROCESSING  0x00000020L
#endif
#ifndef D3DCREATE_PUREDEVICE
#define D3DCREATE_PUREDEVICE                 0x00000010L
#endif
#ifndef D3DPRESENT_INTERVAL_IMMEDIATE
#define D3DPRESENT_INTERVAL_IMMEDIATE        0x80000000L
#endif

/* =====================================================================
 * Xbox flat struct forward declarations
 * On Xbox, D3D objects are plain structs — no COM vtables.
 * We only need the pointer types; the actual struct layouts are
 * opaque to us (we never dereference fields directly).
 * =================================================================== */

typedef struct Direct3D          Direct3D;
typedef struct D3DDevice         D3DDevice;
typedef struct D3DResource       D3DResource;
typedef struct D3DBaseTexture    D3DBaseTexture;
typedef struct D3DTexture        D3DTexture;
typedef struct D3DIndexBuffer    D3DIndexBuffer;
typedef struct D3DPushBuffer     D3DPushBuffer;
typedef struct D3DFixup          D3DFixup;
/* D3DVertexBuffer and D3DSurface are forward-declared at the top of this
 * file (before the d3d8types-xbox.h include) so the included header can
 * reference them.  Don't redeclare them here. */

/* =====================================================================
 * Direct3D functions  (d3d8-xbox.lib)
 * =================================================================== */
#ifdef __cplusplus
extern "C" {
#endif

/* Video system queries — from xapilib.lib (already linked) */
DWORD WINAPI XGetVideoStandard(VOID);
DWORD WINAPI XGetVideoFlags(VOID);

/* Create the D3D8 root object */
Direct3D * WINAPI Direct3DCreate8(UINT SDKVersion);

/* Pushbuffer tuning — call BEFORE CreateDevice.
 * FakeGLx (D3DQuakeX) uses (768*1024, 128*1024); XDK default is smaller.
 * size = GPU command FIFO size in bytes; kickOffSize = run-ahead threshold. */
HRESULT WINAPI Direct3D_SetPushBufferSize(DWORD dwPushBufferSize, DWORD dwKickOffSize);

/* Direct3D object functions */
HRESULT WINAPI Direct3D_CreateDevice(
    UINT Adapter, D3DDEVTYPE DeviceType, void *pUnused,
    DWORD BehaviorFlags, D3DPRESENT_PARAMETERS *pPresentationParameters,
    D3DDevice **ppReturnedDeviceInterface);
/* Direct3D_Release is inline — returns 1, no-op on Xbox */

/* Device lifecycle */
ULONG   WINAPI D3DDevice_Release(void);
HRESULT WINAPI D3DDevice_Reset(D3DPRESENT_PARAMETERS *pPresentationParameters);

/* Video encoder — SDLx calls SetFlickerFilter(5) after Reset to enable
 * flicker reduction on interlaced displays. The video encoder must be
 * configured before Swap's FLIP_STALL can complete. */
void    WINAPI D3DDevice_SetFlickerFilter(DWORD Filter);

/* Soft display filter — enables soft-edge filter for interlaced output.
 * Half-LifeX (working Xbox renderer) calls SetSoftDisplayFilter(true)
 * during D3D init.  Possibly required for proper video encoder setup. */
void    WINAPI D3DDevice_SetSoftDisplayFilter(BOOL Enable);

/* Scene management */
void    WINAPI D3DDevice_Clear(DWORD Count, const D3DRECT *pRects,
            DWORD Flags, D3DCOLOR Color, float Z, DWORD Stencil);
/* BeginScene/EndScene are inline no-ops on Xbox (from D3D8-Xbox.h) */
static __inline void WINAPI D3DDevice_BeginScene(void) { }
static __inline void WINAPI D3DDevice_EndScene(void) { }

/* Swap/Present */
DWORD   WINAPI D3DDevice_Swap(DWORD Flags);
void    WINAPI D3DDevice_BlockUntilVerticalBlank(void);
void    WINAPI D3DDevice_KickPushBuffer(void);
/* Stalls CPU until GPU has finished processing all queued commands.
 * Use before Swap to distinguish GPU-hang from Swap-hang. */
void    WINAPI D3DDevice_BlockUntilIdle(void);

/* Render states — use the "not inline" version which handles all
 * render state dispatch internally (simple, deferred, and special).
 * The inline version requires lookup tables we don't have. */
void    WINAPI D3DDevice_SetPixelShader(DWORD Handle);
void    WINAPI D3DDevice_SetRenderStateNotInline(D3DRENDERSTATETYPE State, DWORD Value);

/* Texture stage states */
void    WINAPI D3DDevice_SetTextureStageStateNotInline(
            DWORD Stage, D3DTEXTURESTAGESTATETYPE Type, DWORD Value);

/* Vertex shader (FVF) */
void    WINAPI D3DDevice_SetVertexShader(DWORD Handle);

/* Texture binding */
void    WINAPI D3DDevice_SetTexture(DWORD Stage, D3DBaseTexture *pTexture);

/* Stream source */
void    WINAPI D3DDevice_SetStreamSource(UINT StreamNumber,
            D3DVertexBuffer *pStreamData, UINT Stride);

/* Transform matrices — map screen-space vertices to NDC via projection */
void    WINAPI D3DDevice_SetTransform(D3DTRANSFORMSTATETYPE State,
            const D3DMATRIX *pMatrix);

/* Viewport — REQUIRED before any draw call.
 *
 * Without a valid viewport, NV2A's rasterizer/ROP unit writes pixels to
 * invalid memory addresses → GPU DMA fault → Swap fence-wait loops forever.
 * The Clear engine bypasses the rasterizer, which is why Clear+Swap works
 * even without a viewport.
 *
 * FakeGLx (D3DQuakeX) always calls SetViewport before its first draw.
 * D3DVIEWPORT8 is defined in D3D8Types.h. */
void    WINAPI D3DDevice_SetViewport(const D3DVIEWPORT8 *pViewport);

/* GetBackBuffer — Half-LifeX calls this immediately after CreateDevice
 * (GetBackBuffer + GetDesc + Release).  May be required to fully
 * initialize the device's render-target binding on Xbox. */
HRESULT WINAPI D3DDevice_GetBackBuffer(INT BackBuffer,
            DWORD Type, D3DSurface **ppBackBuffer);

/* Push buffer recording — Half-LifeX (verified working on retail Xbox)
 * wraps every draw with BeginPushBuffer/<draw>/EndPushBuffer/RunPushBuffer.
 * The draw is recorded into a pre-allocated push buffer object and then
 * "run" by the GPU as a separate command stream.  Verified exported from
 * XDK 5558 d3d8.lib (CreatePushBuffer@12, BeginPushBuffer@4,
 * EndPushBuffer@0, RunPushBuffer@8). */
HRESULT WINAPI D3DDevice_CreatePushBuffer(UINT Size, BOOL RunUsingCpuCopy,
            D3DPushBuffer **ppPushBuffer);
void    WINAPI D3DDevice_BeginPushBuffer(D3DPushBuffer *pPushBuffer);
void    WINAPI D3DDevice_EndPushBuffer(void);
void    WINAPI D3DDevice_RunPushBuffer(D3DPushBuffer *pPushBuffer,
            D3DFixup *pFixup);

/* Drawing — Xbox-specific extensions (VertexCount, NOT PrimitiveCount).
 *
 * DrawVertices / DrawVerticesUP / DrawIndexedVerticesUP are Xbox-specific
 * extensions that take VertexCount instead of PrimitiveCount.  All confirmed
 * to hang NV2A on retail hardware under XDK 5849 (extensive hw tests).
 * Declared here for reference only; do NOT use for rendering.
 *
 * NOTE: DrawPrimitive and DrawPrimitiveUP are NOT declared here.
 * They are inline-only C++ methods in D3D8-Xbox.h (not exported from
 * d3d8ltcg.lib).  Use xbox_DrawPrimitiveUP() from std3D_draw.cpp instead. */
void    WINAPI D3DDevice_DrawVertices(D3DPRIMITIVETYPE PrimitiveType,
            UINT StartVertex, UINT VertexCount);
void    WINAPI D3DDevice_DrawVerticesUP(D3DPRIMITIVETYPE PrimitiveType,
            UINT VertexCount, const void *pVertexStreamZeroData,
            UINT VertexStreamZeroStride);
void    WINAPI D3DDevice_DrawIndexedVerticesUP(D3DPRIMITIVETYPE PrimitiveType,
            UINT VertexCount, const void *pIndexData,
            const void *pVertexStreamZeroData, UINT VertexStreamZeroStride);

/* Texture creation */
D3DTexture * WINAPI D3DDevice_CreateTexture2(
            DWORD Width, DWORD Height, DWORD Depth, DWORD Levels,
            DWORD Usage, D3DFORMAT Format, D3DRESOURCETYPE D3DType);

/* Surface creation. Usage: D3DUSAGE_RENDERTARGET, D3DUSAGE_DEPTHSTENCIL, or 0
 * (plain image surface). Per XDK CHM, can be called with NULL global device
 * (i.e. before Direct3D_CreateDevice) to pre-allocate frame buffers, which
 * are then passed to CreateDevice via D3DPRESENT_PARAMETERS.BufferSurfaces[]
 * and .DepthStencilSurface. */
D3DSurface * WINAPI D3DDevice_CreateSurface2(
            DWORD Width, DWORD Height, DWORD Usage, D3DFORMAT Format);

/* Vertex buffer creation */
D3DVertexBuffer * WINAPI D3DDevice_CreateVertexBuffer2(UINT Length);

/* Resource management */
ULONG   WINAPI D3DResource_Release(D3DResource *pThis);

/* Texture lock/unlock — LockRect is in the lib, UnlockRect is a no-op */
void    WINAPI D3DTexture_LockRect(D3DTexture *pThis, UINT Level,
            D3DLOCKED_RECT *pLockedRect, const RECT *pRect, DWORD Flags);
static __inline void WINAPI D3DTexture_UnlockRect(D3DTexture *pThis, UINT Level)
    { (void)pThis; (void)Level; /* no-op on Xbox UMA */ }

/* Vertex buffer lock/unlock — Lock2 is in the lib, Unlock is a no-op */
BYTE *  WINAPI D3DVertexBuffer_Lock2(D3DVertexBuffer *pThis, DWORD Flags);
static __inline void WINAPI D3DVertexBuffer_Unlock(D3DVertexBuffer *pThis)
    { (void)pThis; /* no-op on Xbox UMA */ }

#ifdef __cplusplus
}
#endif

/* =====================================================================
 * Inline convenience wrappers (matching the C++ method call syntax
 * used in std3D.c without needing the full D3D8-Xbox.h)
 *
 * These let std3D.c keep using  g_pDevice->BeginScene()  etc.
 * by defining minimal C++ struct methods that call the flat C API.
 * =================================================================== */

#ifdef __cplusplus

/* Minimal Direct3D struct with methods we use */
struct Direct3D {
    HRESULT WINAPI CreateDevice(UINT Adapter, D3DDEVTYPE DeviceType,
        void *pUnused, DWORD BehaviorFlags,
        D3DPRESENT_PARAMETERS *pp, D3DDevice **ppDev)
    {
        return Direct3D_CreateDevice(Adapter, DeviceType, pUnused,
            BehaviorFlags, pp, ppDev);
    }
    ULONG WINAPI Release() { return 1; /* no-op on Xbox */ }
};

/* Minimal D3DResource with Release */
struct D3DResource {
    ULONG WINAPI Release() { return D3DResource_Release(this); }
};

struct D3DBaseTexture : public D3DResource {};

struct D3DTexture : public D3DBaseTexture {
    HRESULT WINAPI LockRect(UINT Level, D3DLOCKED_RECT *pLR,
        const RECT *pRect, DWORD Flags)
    {
        D3DTexture_LockRect(this, Level, pLR, pRect, Flags);
        return S_OK;
    }
    HRESULT WINAPI UnlockRect(UINT Level)
    {
        D3DTexture_UnlockRect(this, Level);
        return S_OK;
    }
};

struct D3DVertexBuffer : public D3DResource {
    HRESULT WINAPI Lock(UINT Offset, UINT Size, BYTE **ppData, DWORD Flags)
    {
        *ppData = D3DVertexBuffer_Lock2(this, Flags) + Offset;
        return S_OK;
    }
    HRESULT WINAPI Unlock()
    {
        D3DVertexBuffer_Unlock(this);
        return S_OK;
    }
};

struct D3DDevice {
    ULONG WINAPI Release() { return D3DDevice_Release(); }

    void WINAPI BeginScene() { /* inline no-op on Xbox */ }
    void WINAPI EndScene()   { /* inline no-op on Xbox */ }

    HRESULT WINAPI Clear(DWORD Count, const D3DRECT *pRects, DWORD Flags,
        D3DCOLOR Color, float Z, DWORD Stencil)
    {
        D3DDevice_Clear(Count, pRects, Flags, Color, Z, Stencil);
        return S_OK;
    }

    HRESULT WINAPI Present(const RECT*, const RECT*, void*, void*)
    {
        D3DDevice_Swap(0);
        return S_OK;
    }

    HRESULT WINAPI SetTransform(D3DTRANSFORMSTATETYPE State,
        const D3DMATRIX *pMatrix)
    {
        D3DDevice_SetTransform(State, pMatrix);
        return S_OK;
    }

    HRESULT WINAPI SetRenderState(D3DRENDERSTATETYPE State, DWORD Value)
    {
        D3DDevice_SetRenderStateNotInline(State, Value);
        return S_OK;
    }

    HRESULT WINAPI SetTextureStageState(DWORD Stage,
        D3DTEXTURESTAGESTATETYPE Type, DWORD Value)
    {
        D3DDevice_SetTextureStageStateNotInline(Stage, Type, Value);
        return S_OK;
    }

    HRESULT WINAPI SetVertexShader(DWORD Handle)
    {
        D3DDevice_SetVertexShader(Handle);
        return S_OK;
    }

    HRESULT WINAPI SetTexture(DWORD Stage, D3DBaseTexture *pTex)
    {
        D3DDevice_SetTexture(Stage, pTex);
        return S_OK;
    }

    HRESULT WINAPI SetStreamSource(UINT Stream, D3DVertexBuffer *pVB,
        UINT Stride)
    {
        D3DDevice_SetStreamSource(Stream, pVB, Stride);
        return S_OK;
    }

    HRESULT WINAPI CreateVertexBuffer(UINT Length, DWORD Usage, DWORD FVF,
        D3DPOOL Pool, D3DVertexBuffer **ppVB)
    {
        *ppVB = D3DDevice_CreateVertexBuffer2(Length);
        return (*ppVB != 0) ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT WINAPI CreateTexture(UINT W, UINT H, UINT Levels, DWORD Usage,
        D3DFORMAT Fmt, D3DPOOL Pool, D3DTexture **ppTex)
    {
        *ppTex = (D3DTexture*)D3DDevice_CreateTexture2(
            W, H, 1, Levels, Usage, Fmt, (D3DRESOURCETYPE)3/*D3DRTYPE_TEXTURE*/);
        return (*ppTex != 0) ? S_OK : E_OUTOFMEMORY;
    }

    HRESULT WINAPI DrawIndexedPrimitiveUP(D3DPRIMITIVETYPE PrimType,
        UINT MinIdx, UINT NumVerts, UINT PrimCount,
        const void *pIdx, D3DFORMAT IdxFmt,
        const void *pVerts, UINT Stride)
    {
        /* Xbox DrawIndexedVerticesUP takes vertex count, not prim count */
        UINT vtxCount;
        switch (PrimType) {
            case D3DPT_TRIANGLELIST: vtxCount = PrimCount * 3; break;
            case D3DPT_TRIANGLESTRIP: vtxCount = PrimCount + 2; break;
            case D3DPT_LINELIST: vtxCount = PrimCount * 2; break;
            default: vtxCount = PrimCount; break;
        }
        D3DDevice_DrawIndexedVerticesUP(PrimType, vtxCount, pIdx, pVerts, Stride);
        return S_OK;
    }
};

#endif /* __cplusplus */

#endif /* D3D8_XBOX_MINIMAL_H */
