# Xbox Homebrew D3D8 Research Notes

**Purpose:** reference material extracted from XDK CHM, XboxDev wiki, Cxbx-Reloaded source, nxdk source, and web searches. Serves the Swap/CreateDevice hang investigation and future work (sithRender wiring, textures, audio, input). For current test state, see `CLAUDE.md`'s "Swap hang investigation: current test & hypothesis" section — this file is reference, not status.

---

## 1. D3DPRESENT_PARAMETERS — Xbox layout is 68 bytes, not 52

**This was discovered during the investigation and has been partially addressed, but it was not the complete cause of the hang. See CLAUDE.md for current empirical state.** What follows is the reference material that establishes the struct layout.

**Primary source:** `C:\XDK\doc\XboxSDK.chm` → `_dx_d3dpresent_parameters_graphics.htm` (Microsoft's own XDK documentation)

**Corroboration #1:** Cxbx-Reloaded's `src/core/hle/D3D8/XbD3D8Types.h` — same struct, same fields, with the verbatim comment *"This check guarantees identical layout, compared to Direct3D8._D3DPRESENT_PARAMETERS_"*, with the assertion noting that `offsetof(X_D3DPRESENT_PARAMETERS, BufferSurfaces) == sizeof(_D3DPRESENT_PARAMETERS_)` — i.e. BufferSurfaces starts exactly at byte 52 (where the PC struct ends).

**The Xbox struct:**
```c
typedef struct _D3DPRESENT_PARAMETERS_
{
    /* --- first 52 bytes: identical to PC DirectX 8 --- */
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
    /* --- XBOX EXTENSIONS (16 bytes) --- */
    D3DSurface         *BufferSurfaces[3];   /* 12 bytes */
    D3DSurface         *DepthStencilSurface; /*  4 bytes */
} D3DPRESENT_PARAMETERS;   /* total: 68 bytes */
```

Microsoft's doc on the surface fields:
> "If non-NULL, Direct3D will not allocate its own color and Z surfaces, but rather will use the specified surfaces. **BufferSurfaces[0]** will be used as the back-buffer, and **BufferSurfaces[1]** (and **BufferSurfaces[2]** if BackBufferCount is 2) will be used as the front-buffers."

### What we know empirically (updated after testing)

- With the 52-byte PC struct: `CreateDevice` returns hr=0; Swap hangs inside `BlockOnFence`.
- With the 68-byte Xbox struct: `CreateDevice` itself hangs before returning.
- Both behaviors together prove the lib really does read 68 bytes (otherwise the 16 bytes beyond our struct wouldn't be able to flip the behavior).
- The 68-byte struct is necessary but not sufficient. With bytes 52–67 now deliberately NULL, D3D's internal surface allocator tries to run and that code path hangs on our hardware for reasons not yet identified.
- **"Softmod quirk" is not a valid explanation** — softmods don't replace the kernel; retail games' D3D allocator works fine on softmodded Xboxes. The hang is about our parameters or call pattern, not the runtime environment.

### Stale-hypothesis notes (what I thought initially; partially wrong)

The original theory was: "52-byte struct leaves 16 bytes of stack garbage, lib reads it as non-NULL surface pointers, Swap flips to invalid addresses and hangs." This explained the 52-byte behavior but was incomplete — fixing the struct should then have worked. It didn't. What appears to have been happening is:

- The stack bytes past `d3dpp` (at offset 52–67) were likely **zero** in the original runs (fresh stack frame, no prior writes) — so the lib actually saw NULL surface pointers and *did* allocate its own. CreateDevice completed successfully because allocation succeeded in that context.
- Swap then hung for a different reason — possibly a GPU state issue we haven't yet identified — and we assumed the surface pointers were the cause.
- With the explicit 68-byte struct, CreateDevice enters the same allocator path but now hangs there. Why the allocator behaves differently between the two cases is unclear; could be timing, stack alignment effects, or something about our parameter combination interacting with the allocator.

### Surface-allocation path forward

Microsoft's XDK CHM (`_dx_d3dpresent_parameters_graphics.htm`) explicitly supports app-allocated surfaces:

> "If non-NULL, Direct3D will not allocate its own color and Z surfaces, but rather will use the specified surfaces. BufferSurfaces[0] will be used as the back-buffer, and BufferSurfaces[1] (and BufferSurfaces[2] if BackBufferCount is 2) will be used as the front-buffers."

> "D3D_AllocContiguousMemory, SetTile, XGSetSurfaceHeader, and Register are the preferred ways to create the surfaces in this case, but CreateImageSurface can also be used before calling CreateDevice by using a NULL device interface pointer: `((D3DDevice*) NULL)->CreateImageSurface(...)`"

If the current isolation test (BackBufferCount=0, SwapEffect=COPY with 68-byte struct) still hangs, this is the documented next path — provide our own back buffer + front buffer + depth surface, bypassing D3D's internal allocator entirely.

---

## 2. D3D8 reference info (for current + future work)

### IDirect3DDevice8::Swap pseudo-code (from XDK whitepaper `prog_wp_API_Insight_Present.htm`)

Authoritative pseudo-code of what `D3DDevice_Swap(0)` actually does internally. Summary:

- `Swap(0)` expands to `Swap(D3DSWAP_COPY | D3DSWAP_FINISH)`.
- For `D3DSWAPEFFECT_DISCARD` / `FLIP` (the default path): calls `SwapStart` → `SwapFlip` → `SwapFinish`.
- `SwapStart` will `BlockOnTime` if more than 2 swaps are already pending. → This is the indefinite stall we were hitting.
- `SwapFlip` emits GPU commands: `WAIT_FOR_IDLE`, `SET_NEW_FRONTBUFFER`, `FLIP_STALL`, `SET_NEW_BACKBUFFER`.
- `D3DSWAPEFFECT_FLIP` is treated identically to `D3DSWAPEFFECT_DISCARD`.
- `BackBufferCount == 0` is accepted and treated as 1.
- Max 2 swaps queued simultaneously; more → `BlockOnFence`.
- `Present(NULL,NULL,NULL,NULL)` is exactly `Swap(0)`. Confirmed in Xbox's `D3D8.h` inline: `D3DINLINE HRESULT WINAPI IDirect3DDevice8_Present(...) { D3DDevice_Swap(0); return S_OK; }`.

### Swap flags (`_dx_idirect3ddevice8_swap_graphics.htm`)

- `D3DSWAP_DEFAULT` (0) — equivalent to `Present`
- `D3DSWAP_COPY` — for FSAA / back-buffer scaling custom paths
- `D3DSWAP_BYPASSCOPY` — title implements its own back→front blit
- `D3DSWAP_FINISH` — finalize custom swap path

### IDirect3DDevice8::Reset (`_dx_idirect3ddevice8_reset_graphics.htm`)

> "Unlike DX8 on PC, Reset does NOT unload textures or lose state. On Xbox, Reset only changes the video mode and recreates the frame buffers."

**Important for future:** If we ever need to change resolution at runtime, `Reset` is cheap on Xbox. Useful for switching to a different back-buffer size.

### IDirect3D8::CreateDevice (`_dx_idirect3d8_createdevice_graphics.htm`)

- Adapter: use `D3DADAPTER_DEFAULT`
- DeviceType: **must be `D3DDEVTYPE_HAL` on Xbox**
- Third param (`hFocusWindow` on PC) is `void*` on Xbox, always NULL — *"Xbox Optimization"*
- BehaviorFlags: `D3DCREATE_HARDWARE_VERTEXPROCESSING`, optional `D3DCREATE_PUREDEVICE` (must also `#define D3DCOMPILE_PUREDEVICE 1` before xtl.h)
- Remarks: *"returns a fully working device interface, set to the required display mode, and allocated with the appropriate back buffers. The application only needs to create and set a depth buffer, if desired, to begin rendering."* → hence EnableAutoDepthStencil=TRUE is sufficient.

### D3D_SDK_VERSION = 120

From `C:\XDK\xbox\include\D3D8.h` line 26. Our minimal shim had `0`; now fixed to `120`. Direct3DCreate8 is documented to *"fail if the version doesn't match"* but in practice (per our testing) returns a non-NULL sentinel either way. Still, use the real value for correctness.

### IDirect3DDevice8::CreateTexture (`_dx_idirect3ddevice8_createtexture_graphics.htm`)

- Levels: 0 = auto-generate full mip chain
- Usage: 0 or `D3DUSAGE_BORDERSOURCE_TEXTURE` (Xbox-specific; disables border color)
- **Pool: IGNORED on Xbox** — doesn't matter what you pass
- Auto-mips halve and clamp to 1×1 minimum

### ⚠️ Xbox enum values differ from PC for many types (from CHM)

- `D3DFMT_*` — almost all values redefined to match NVIDIA internal format codes (see below)
- `D3DMULTISAMPLE_TYPE` — "The constants for this type have been redefined" (`_dx_d3dmultisample_type_graphics.htm`). On PC these were `2_SAMPLES=2`, etc.; on Xbox they're named like `D3DMULTISAMPLE_2_SAMPLES_MULTISAMPLE_LINEAR`, `..._QUINCUNX`, etc. with filter info in the name. `D3DMULTISAMPLE_NONE = 0` is stable across both.
- `D3DSWAPEFFECT` — unchanged from PC: DISCARD=1, FLIP=2, COPY=3. On Xbox FLIP is treated as DISCARD.
- `D3DCREATE_HARDWARE_VERTEXPROCESSING = 0x40` — unchanged.
- `D3DCLEAR_*`, `D3DCULL_*`, `D3DBLEND_*` — unchanged (verified against our code using them fine).

**Bottom line:** Pulling Xbox-specific headers rather than defining our own values avoids guessing. Our d3d8_xbox_minimal.h pulls the generic D3D8Types.h (PC values); safe for enums where values match, dangerous for D3DFORMAT / D3DMULTISAMPLE_TYPE constants other than _NONE.

### D3DFORMAT (`_dx_d3dformat_graphics.htm`)

Key values:
- `D3DFMT_UNKNOWN = 0xFFFFFFFF` (**NOT zero** — Xbox changed this from PC DX8)
- `D3DFMT_A8R8G8B8 = 0x06`, `D3DFMT_X8R8G8B8 = 0x07`
- `D3DFMT_R5G6B5 = 0x05`, `D3DFMT_A1R5G5B5 = 0x02`, `D3DFMT_A4R4G4B4 = 0x04`
- `D3DFMT_P8 = 0x0B` (paletted 8-bit)
- `D3DFMT_DXT1 = 0x0C`, `D3DFMT_DXT3 = 0x0E`, `D3DFMT_DXT5 = 0x0F`
- `D3DFMT_D24S8 = 0x2A`, `D3DFMT_F24S8 = 0x2B` (floating-point depth)
- `D3DFMT_LIN_*` variants (linear, non-swizzled) have distinct codes — e.g. `D3DFMT_LIN_A8R8G8B8 = 0x12`

**Render-target-only formats** (enforced by D3D):
- A8R8G8B8, X8R8G8B8, R5G6B5, X1R5G5B5 (swizzled)
- Their LIN_ equivalents
- LIN_L8 / LIN_G8B8 (no Z/stencil/visibility test support)

All other formats are texture/surface-only, not usable as render targets.

### D3DPRESENTFLAG_* (from `_dx_d3dpresent_parameters_graphics.htm`)

- `D3DPRESENTFLAG_WIDESCREEN = 0x10` — affects field of view, not back-buffer size
- `D3DPRESENTFLAG_PROGRESSIVE = 0x04` — required for 480p/720p
- `D3DPRESENTFLAG_INTERLACED = 0x02` — for interlaced displays
- `D3DPRESENTFLAG_FIELD = 0x40` — field rendering
- `D3DPRESENTFLAG_10X11PIXELASPECTRATIO` — centers frame buffer on display (704px wide with 32px black borders)
- `D3DPRESENTFLAG_EMULATE_REFRESH_RATE` — allows non-native refresh rates; does NOT work with 1080i/overlays

### Video modes / back-buffer resolution (`_dx_back_buffer_and_video_modes.htm`)

- 640×480 back buffer always works (scales to native)
- Native resolutions: NTSC 640×480/720×480, PAL 640×576/720×576, 480p 720×480, 720p 1280×720, 1080i 1920×1080
- To avoid scaling: match the native for your target mode.

---

## 3. Pushbuffer / GPU details (for future rendering optimizations)

### From XDK docs (`_dx_pushbuffer_overview_graphics.htm` + cross-refs)

- Xbox D3D uses a GPU command ring (the push-buffer). CPU appends commands; GPU reads ahead.
- `D3DDevice_BeginPush(count)` / `EndPush` — write raw pushbuffer DWORDs
- `D3DDevice_KickPushBuffer()` — force GPU to fetch queued commands
- `D3DDevice_GetPushBufferOffset()`, `GetPushDistance()` — measure
- `Direct3D_SetPushBufferSize(DWORD size, DWORD kickOffSize)` — Xbox-only, called **before** CreateDevice to tune pushbuffer
- `CreatePushBuffer` / `RunPushBuffer` — persistent recorded pushbuffers (pre-recorded draw state)
- `D3DDevice_BlockUntilIdle()`, `IsBusy()` — GPU sync primitives
- `InsertFence()` + `BlockOnFence()` / `IsFencePending()` — fine-grained CPU/GPU sync

### From pbkit source (nxdk, `lib/pbkit/pbkit.c`)

This is how homebrew bypasses MS's D3D entirely by driving the NV2A directly. Not our path right now, but good to know exists:

- `pb_init()` allocates contiguous memory via `MmAllocateContiguousMemoryEx` (default 512KB pushbuffer)
- Claims PRAMIN via `MmClaimGpuInstanceMemory()` for graphics contexts and DMA descriptors
- Creates DMA contexts on channels 2–12 via `pb_create_dma_ctx`
- Initializes two 3D graphics contexts via `pb_3D_init`
- Writes directly to memory-mapped NV2A registers (`VIDEOREG`): `NV_PFIFO_CACHE1_DMA_PUT`, `NV_PGRAPH_*`, `PCRTC_START` (for flip)
- Triple-buffered flip: `pb_finished()` raises a GPU interrupt; ISR sets `PCRTC_START = frame_buffer_addr`
- `pb_wait_for_vbl()` blocks on `NtWaitForSingleObject(pb_VBlankEvent)`; VBlank ISR pulses it
- Works on softmodded retail Xboxes. Proof-of-concept fallback if MS D3D ever proves fundamentally incompatible.

---

## 4. XBE format details (for future patchxbe.py issues)

**Sources:** XboxDev wiki `Xbe` page + `imagebld.exe /?` + `imagebld.exe /DUMP default.xbe`

### Library version table flags (`Flags` = 2 bytes per entry)

- `QFEVersion` = `0x1FFF` (13-bit: low bits)
- `Approved` = `0x6000` (2-bit: 0=unapproved, 1=possible-approval, 2=approved, 3=?)
- `Debug Build` = `0x8000` (1-bit)

Our XBE currently shows `flags=0x4001` for every library → approved (value 2), QFE version 1. Correct for retail.

### Kernel thunk XOR keys (obfuscation for identification)

- Beta:   `0x46437DCD`
- Debug:  `0xEFB1F152`
- Retail: `0x5B6D40B6`

### Entry point XOR keys

- Beta:   `0xE682F45B`
- Debug:  `0x94859D4B`
- Retail: `0xA8FC57AB`

The kernel tries debug-decode first, then retail. Using the wrong XOR = crash on boot.

**Our XBE:** kernel thunk address shown in dump is `EFB40152`. XOR with debug key (`0xEFB1F152`) → `0x0005F000`, a plausible low-address RVA. So our XBE is marked as **debug-keyed** (imagebld default). Should still work on softmod retail kernels (they handle both paths), but on an unmodified retail kernel this would be rejected. Softmod Xboxes explicitly accept debug-keyed XBEs.

### Certificate structure

- Title ID (2 ASCII letters + u16) — ours is `4C410001`
- Title name (Unicode, up to 40 chars)
- Allowed media types (bitmap, `0xFFFFFFFF` = any)
- Game region (`0x80000007` = global+debug)
- Ratings (`0xFFFFFFFF` = all)
- LAN key (16 bytes; `"TEST"` pattern = test key)
- Signature key (16 bytes; `"TEST"` pattern = test key)

### imagebld.exe options (relevant ones)

- `/IN:` / `/OUT:` — input PE / output XBE
- `/TESTID:0xNNNNNNNN` — title ID for test builds
- `/TESTNAME:"..."` — title name
- `/STACK:N` — stack size (default 0x10000; ours is 0x40000 = 256KB)
- `/TESTMEDIATYPES:0xFFFFFFFF` — all media types
- `/INITFLAGS:N` — init flags, default 0x00000001 (mount utility drive). Others: `0x02` format utility, `0x04` Limit64MB, `0x08` no HDD setup
- `/LIMITMEM` — limit to 64 MB (equivalent to INITFLAGS bit 2)
- `/TESTXBEVERSION:N`, `/TESTVERSION:N`, `/TESTREGION:N`, `/TESTRATINGS:N`, `/TESTSIGNKEY:`, `/TESTLANKEY:`, `/TESTSERVICE:{PART|PROD|GSRV}`, `/TESTALTID:`
- `/DONTMODIFYHD` — no T:/U: drive writes
- `/DONTMOUNTUD`, `/FORMATUD`, `/UDCLUSTER:N` — utility drive control
- `/NOLIBWARN` — disable unapproved-library warnings
- `/NOPRELOAD:sec` — don't auto-load a section
- `/INSERTFILE:file,sec,[R][N]` — embed a file as a section (the `$$XTIMAGE` section in our XBE uses this for the title image)
- `/DEBUG` — embed symbol path for kernel debugger
- `/DUMP` — show the XBE's structure (what we used above)

### Initialization flags (XBE header `InitFlags`)

- `0x01` MountUtilityDrive (Z: drive)
- `0x02` FormatUtilityDrive
- `0x04` Limit64Megabytes (default retail — ignored in 128MB debug mode)
- `0x08` DontSetupHarddisk

### Media type flags (certificate `AllowedMediaTypes`)

- `0x01` HARD_DISK
- `0x02` DVD_X2
- `0x04` DVD_CD
- `0x08` CD
- `0x10` DVD_5_RO, `0x20` DVD_9_RO, `0x40` DVD_5_RW, `0x80` DVD_9_RW
- `0x100` DONGLE, `0x200` MEDIA_BOARD
- `0x40000000` NONSECURE_HARD_DISK
- `0x80000000` NONSECURE_MODE

Our `0xFFFFFFFF` is the "anything goes" softmod-friendly default.

### Section flags (XBE section header)

- `0x01` Writeable
- `0x02` Preload
- `0x04` Executable
- `0x08` Inserted file
- `0x10` Head page read-only
- `0x20` Tail page read-only
- Common combos: `0x07` = W+PL+X (for .data, DSOUND, D3D, XPP), `0x36` = PL+X+HRO+TRO (for .text, .rdata), `0x38` = inserted+HRO+TRO (for $$XTIMAGE)

---

## 5. Subsystem quick reference (for next milestones)

### DirectSound (when wiring `stdSound_xbox.c`)

- Entry point: `HRESULT DirectSoundCreate(NULL, &pDS, NULL)` — device GUID and outer are both reserved, must be NULL.
- **CRITICAL GOTCHA:** *"DirectSoundCreate cannot be called prior to main() (i.e. from a constructor on a global object) as internal state information has not been fully initialized yet."* — must be invoked from main() or later.
- Return codes: `DS_OK`, `DSERR_NOAGGREGATION`, `DSERR_NODRIVER`, `DSERR_OUTOFMEMORY`.
- The DirectSound object *"represents the Xbox Media Communications Processor (MCP)"* (NVIDIA MCPX APU).
- Xbox-specific: 3D HRTF, mix bins, dolby, XMA/ADPCM compression, 256 voices max.

### XInput / controller (when wiring `stdControl_xbox.c`)

- `XInitDevices(numTypes, PXDEVICE_PREALLOC_TYPE)` must be called once before any XInputOpen. Normal pattern:
  ```c
  XDEVICE_PREALLOC_TYPE xdpt[] = {
      {XDEVICE_TYPE_GAMEPAD, 4},
      {XDEVICE_TYPE_MEMORY_UNIT, 8}
  };
  XInitDevices(sizeof(xdpt)/sizeof(*xdpt), xdpt);
  ```
- `XInputOpen(XDEVICE_TYPE_GAMEPAD, XDEVICE_PORT0, XDEVICE_NO_SLOT, NULL)` — returns HANDLE (or NULL on failure; call GetLastError).
- `XGetDeviceChanges(XDEVICE_TYPE_GAMEPAD, &ins, &rem)` — poll for hot-swap. Always process removals before insertions.
- `XInputGetState(hDevice, &XINPUT_STATE)` to read, `XInputSetState(hDevice, &XINPUT_FEEDBACK)` to set rumble.
- All game controllers (wheels, joysticks, lightguns) report as `XDEVICE_TYPE_GAMEPAD`. Port count via `XGetPortCount()`.
- Debug keyboard/mouse types exist (`XDEVICE_TYPE_DEBUG_KEYBOARD`, `XDEVICE_TYPE_DEBUG_MOUSE`) but are devkit-only; `#define DEBUG_KEYBOARD` before `xtl.h` to enable.
- Handles remain valid until the device disconnects; close with `XInputClose`.
- Each opened device consumes 260–320 bytes + bus bandwidth.

### File I/O / path conventions

- Drive letters on Xbox: **C: utility**, **D: DVD / XBE directory** (symlink'd by dashboard), **E: game data / persistent per-title**, **T: persistent per-title data**, **U: user data / saves**, **Z: scratch/utility**
- Kernel NT paths (bypass the drive-letter layer): `\Device\CdRom0\`, `\Device\Harddisk0\Partition1\` (= E:\), `Partition6` (= F:\), `Partition7` (= G:\)
- FAT/FATX path separator: backslash. Max path: 250 chars.
- `NtCreateFile` / `NtReadFile` / `NtWriteFile` work on both CXBX-R and real hardware. `CreateFileA` works on real HW but NOT in CXBX-R.
- Write-through flag: NtCreate options bit `0x02` (`FILE_WRITE_THROUGH`). Use when debug-logging so writes flush before a hang kills the console.
- `XGetDiskFreeSpaceEx("U:\\", ...)` for free-space checks.
- `XGetDiskClusterSize()` — cluster size of a device. A "block" in UI = 16 KB = 1 cluster.

### Memory

- 64 MB RAM in retail Xbox (128 MB in debug kits, but our target is retail).
- Unified Memory Architecture — CPU/GPU/APU/DMA all share it.
- 4 memory partitions × 16 MB, interleaved every 16 B. Auto-parallelized by mem controller.
- CPU cache line: 32 B. CPU accesses read/write in 32-B aligned blocks.
- Three memory types:
  - **Cached** — default CPU heap memory
  - **Write-combining** — non-cached, batches writes; use for pushbuffer, vertex buffers, textures (anything GPU reads but CPU doesn't read back)
  - **Uncached** — only for memory-mapped control registers; never use for normal memory (no Xbox scenario calls for it)
- `MmAllocateContiguousMemoryEx(size, minAddr, maxAddr, alignment, protectFlags)` — kernel-level alloc of physically-contiguous memory. Required for GPU-visible buffers like vertex/index/pushbuffer data when bypassing D3D's managed allocator.

### Stack size

- imagebld default stack: 0x10000 (64KB). Our XBE uses 0x40000 (256KB). Plenty.
- imagebld uses `SizeOfStackReserve` from the PE header, **NOT** `SizeOfStackCommit` — make sure linker sets `/STACK:reserve,commit` correctly.

---

## 6. Common softmod-Xbox homebrew pitfalls

- **Wrong kernel thunk / entry point XOR keys**: the #1 cause of "XBE crashes on boot" in homebrew. imagebld chooses debug keys by default; retail kernels accept them on softmod but reject them on unmodified retail. Ours is debug-keyed and that's fine for softmod use.
- **Linking debug-variant libs** (`d3d8d.lib`, `xapilibd.lib`, `libcd.lib`) on a retail kernel. These call `xbdm.lib` internally which depends on debug-kit services → hang/crash. Our build correctly uses retail libs (`d3d8ltcg.lib` + LTCG pair).
- **Calling `DirectSoundCreate` from C++ global ctors** — explicitly documented as unsupported (MCP not initialized yet). Must be from main() or later.
- **OutputDebugString in "retail" builds is banned by TCR C11-10** — safe on softmod because there's no certification, but don't leave debug logging in a "release" build if later distributing.
- **Dashboard re-entry**: titles are expected to return to the dashboard via `XLaunchNewImage(NULL, NULL)`. On softmods, UnleashX reclaims control automatically on exit.
- **Save-game signing**: Xbox enforces signed save games on retail kernels (even softmod) for files on U:\. Use `XCalculateSignature*` family. Zero-signature "test" signing key pattern (`TESTTESTTESTTEST`) is fine for homebrew.
- **TCRs (Technical Certification Requirements)**: not relevant to homebrew, but worth knowing the rules Xbox games were tested against — informs expected subsystem behavior (controller hot-swap, MU handling, game-load time budgets).
- **MSAA + Present**: Present with MSAA on implicitly forces `D3DSWAPEFFECT_COPY` and does a filtered blit through the GPU. That resets several render states (lists in XDK whitepaper). Save/restore if you use FSAA.
- **"Pure device"** (`D3DCREATE_PUREDEVICE`) disables simple-state save/restore during Swap. If you ever use it, also `#define D3DCOMPILE_PUREDEVICE 1` before including xtl.h.

---

## 7. nxdk / pbkit as fallback (future contingency)

If MS D3D8 ever proves unsuited for us:

- **nxdk** (`github.com/XboxDev/nxdk`) is a maintained open-source SDK targeting softmod Xboxes. It implements its own graphics stack (`pbkit`) that drives the NV2A directly via `MmAllocateContiguousMemoryEx` + memory-mapped register writes, bypassing MS D3D.
- pbkit is documented enough to reimplement a minimal D3D-like API on top of it if we need to.
- nxdk builds with `nxdk-pgraph-tests`, `nxdk-automated-tester`, and many homebrew games — proven to work on the exact same hardware class as our target.

We don't need this right now. Logged for future reference.

---

## 8. Resources I've extracted / cached locally

- `C:\XDK\doc\XboxSDK.chm` → decompiled to `build\xbox\chm_extract\` (5034 files). Directly greppable.
- Key pages I've read:
  - `_dx_idirect3ddevice8_swap_graphics.htm`
  - `_dx_idirect3ddevice8_present_graphics.htm`
  - `_dx_d3dpresent_parameters_graphics.htm` ← **the root-cause doc**
  - `prog_wp_API_Insight_Present.htm` (Swap pseudo-code)
  - `_dx_idirect3d8_createdevice_graphics.htm`
  - `_dx_idirect3ddevice8_reset_graphics.htm`
  - `_dx_idirect3ddevice8_createtexture_graphics.htm`
  - `_dx_idirect3ddevice8_createimagesurface_graphics.htm`
  - `_dx_idirect3ddevice8_blockonfence_graphics.htm`
  - `_dx_d3dformat_graphics.htm`
  - `_dx_back_buffer_and_video_modes.htm`
  - `xbox_jc_directsoundcreate_dxaudio.htm`
  - `x_jsk_xinputopen.htm`
  - `x_mt_input_ov_init.htm`
  - `wp_memory.htm` (Mike Abrash's Xbox Memory Architecture whitepaper — entire thing; useful later)
  - `whitepaper_TCR_TopIssues.htm` (save-game signing, MU handling, launch info, controller discovery, debug-string rules)
- `C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release\d3d8_syms.txt`, `xg_syms.txt` — lib symbol dumps proving d3d8.lib is a 3-symbol import stub.
- `C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release\xbe_dump.txt` — imagebld `/DUMP` of our current XBE.

## External references worth revisiting

- `github.com/Cxbx-Reloaded/Cxbx-Reloaded` — HLE implementations of every D3D function. If we ever need to reverse-engineer exact behavior of any D3D call, read theirs.
- `github.com/XboxDev/nxdk/lib/pbkit/pbkit.c` — direct NV2A programming.
- `xboxdevwiki.net/Xbe` — XBE format reference.
- `xboxdevwiki.net/Kernel` — kernel export list.
