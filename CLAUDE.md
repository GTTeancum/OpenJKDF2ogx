# OpenJKDF2 Xbox Port — Claude Code Context

## For a Claude instance picking this up cold

Read these in order before doing anything:

1. **This file in full.** Pay attention to "Current State" and "Swap hang investigation: current test & hypothesis" — the latter contains the live table of what's been tried. The row labeled **CURRENT** is the test awaiting a log from the user.
2. **`build\xbox\research_notes.md`** — reference material (D3D API, XBE format, kernel APIs, enum-value gotchas, etc.). Don't re-derive anything it already documents.
3. **`build\xbox\release\debug_openjkdf2.txt`** — the Xbox-side log. Whatever the user is reacting to is in there.
4. **`build\xbox\release\build_log.txt`** — the last build's output.

### What the user is almost always asking for
- "Log posted" / "Log updated" / "log at usual location" → read `build\xbox\release\debug_openjkdf2.txt` and compare to the last row of the test table.
- A build → use `build_xbox.bat`, never direct cl.exe/link.exe invocations.
- A fix → evidence-based only. No hypothesis is a fix until a log shows it.

### What NOT to do (learned the hard way)
- **Don't guess at lib variants.** d3d8ltcg.lib + xgraphicsltcg.lib is the matched retail LTCG pair. Every other combination has been tried. See "Dead ends" in the Swap hang section.
- **Don't assume "softmod quirk."** Softmods don't modify the kernel. Retail games' D3D works fine on softmod.
- **Don't use C++ vtable dispatch** (`pD3D->CreateDevice(...)`). `Direct3DCreate8` returns sentinel `0x1`, not a real COM object. Flat C API only.
- **Don't change multiple things per test.** The current Swap investigation is structured as one-variable-at-a-time isolation. Preserve that discipline.
- **Don't dereference `g_pDevice` fields directly.** It's opaque per the flat C API contract.
- **Don't touch `patchxbe.py`'s PE subsystem patch or KERNEL32 strip.** Those are required for the XBE to boot at all.

### Keep this file current
After every build/deploy/log cycle, update the test table in "Swap hang investigation: current test & hypothesis" with the new row and outcome. If you change source or build config, update the "Last build" bullet in that same section. Stale docs are worse than no docs — this project's context is now too expensive to lose.

---

## Communication Rules
- **Work silently.** No explanations unless you need something from the user.
- **Always provide full files with filenames.** No partial snippets.
- **Always narrate the path to place files.** e.g. `stdFile_xbox.c → src\Platform\Xbox\stdFile_xbox.c`
- **Serve files as downloads**, not copy/paste.
- **Never hardcode drive letters.** Use relative paths or probe E/F/G/D dynamically.
- **User commits manually at end of day.** Don't assume version control.
- **Always build via `build_xbox.bat`** from the repo root. Never use VS2005's compiler directly.
- **Build command:** `powershell.exe -NoProfile -Command "cmd /c '.\build_xbox.bat' 2>&1 | Out-File -FilePath build\xbox\release\build_log.txt -Encoding utf8 -Force"` — then check `build\xbox\release\build_log.txt` for errors.
- **Output XBE:** `build\xbox\release\default.xbe` (this is what the user FTPs to the Xbox).
- **Xbox-side log:** `E:\debug_openjkdf2.txt` (written by `xbox_debug.c` via NtCreateFile + WRITE_THROUGH — trustworthy even right before a hang). User FTPs it back to `build\xbox\release\debug_openjkdf2.txt` and says "log posted" / "log updated" / similar.

## Project Overview
Porting OpenJKDF2 (Jedi Knight: Dark Forces II reverse-engineered source) to the original Xbox console.

## Build Environment
- **IDE:** Visual Studio 2005 (VS2005)
- **SDK:** Xbox Development Kit (XDK) 5849 at `C:\XDK\`
- **Language:** C89 mode (`CompileAs="1"` globally). Individual files can override to C++ (`CompileAs="2"`) in the vcproj per-file FileConfiguration.
- **Build config:** Release only. No debug kit. Retail Xbox.
- **CRT:** `libc.lib` (release). NOT `libcd.lib` (debug — hangs on retail hardware).
- **D3D:** Real renderer (`src\Platform\Xbox\std3D.c`) is active in the build. `std3D_xbox_stub.c` exists for headless mode but is NOT currently compiled in (see build_xbox.bat's source list).
- **Linker libs (Release):** `d3d8ltcg.lib dsound.lib xboxkrnl.lib xgraphicsltcg.lib xonline.lib libc.lib xapilib.lib` — matched LTCG pair per XDK Release_LTCG canonical config. (Single source of truth: `build_xbox.bat` line ~181. If the build command changes, update this line.)
- **NEVER use `d3d8-xbox.lib` or `d3d8d.lib` or `xgraphicsd.lib`** — those are debug-kit libs that hang on retail hardware.
- **NEVER use `d3d8.lib` standalone** — it's a 3-symbol import stub (only `Direct3DCreate8`, `ValidatePixelShader`, `ValidateVertexShader`). Can't resolve `Direct3D_CreateDevice`, `D3DDevice_Swap`, etc. Links fail.
- **Linker flags:** `/FIXED:NO`, `IgnoreAllDefaultLibraries="true"`, `GenerateManifest="false"`, no manifest embedding
- **Post-build:** `patchxbe.py` patches PE subsystem to 14 (Xbox), strips KERNEL32 imports, runs `imagebld.exe` at `C:\XDK\xbox\bin\imagebld.exe` to produce XBE
- **Source root:** `C:\Programming\GitHub\OpenJKDF2ogx\`
- **Game data (CXBX-R):** `C:\Emulators\CXBX\openJKDF2x\`
- **Game data (Xbox hardware):** `/E/Games/openJKDF2x/` (FTP'd to HDD)

## Testing
- **Primary:** CXBX-Reloaded (build 9454f34). D3D crashes in emulator (LLE NV2A bug, confirmed not our code). File I/O works via NtCreateFile.
- **Secondary:** Retail Xbox via FTP. Log file writes to `E:\debug_openjkdf2.txt` via NtCreateFile.
- **No console testing until in-game with basic features working.** Full stop.

## C89 Constraints (VS2005)
- No mid-block variable declarations. All variables at top of scope.
- No `for (int i = 0; ...)` — declare `int i` before the loop.
- No C99 compound literals (breaks C++ compilation of headers like rdVector.h).
- Use `__inline` instead of `inline` (handled by `platform_xbox.h` define).
- Files needing C99/C++ features must use a per-file `CompileAs="2"` override in the vcproj, but beware: engine headers use C99 compound literals that break under C++. The safest approach is self-contained C89 implementations (see `stdGob_xbox.c`).

## Key Architecture Decisions

### File I/O
- All file I/O goes through `NtCreateFile`/`NtClose` from `xboxkrnl.lib` — NOT `CreateFileA` from xapilib.
- `CreateFileA` works on real hardware but NOT in CXBX-R (returns err=0 for everything).
- `NtCreateFile` works on both CXBX-R and real hardware.
- Xbox kernel paths use `\??\D:` prefix (symlink set by dashboard to XBE directory). This is the primary probe path.
- Device paths: `\Device\CdRom0\` = disc, `\Device\Harddisk0\Partition1\` = E:\, `Partition6` = F:\, `Partition7` = G:\
- HostServices table in `stdFile_xbox.c` provides file I/O callbacks to the engine.
- `std_pHS` is the global HostServices pointer. All engine Startup functions receive it.

### Debug Logging
- `xbox_debug.c` writes to `E:\debug_openjkdf2.txt` via NtCreateFile (probes Partition1/6/7).
- Falls back to `CreateFileA` with `D:\`, `T:\`, `E:\` drive letters.
- `OutputDebugStringA` always works as fallback (shows in CXBX-R console as `DEBUG_PRINT:`).
- `XDBG("msg")` and `XDBGF("fmt", args)` macros in `xbox_debug.h`.
- **Do NOT use XeImageFileName** — causes KeBugCheck on CXBX-R.

### GOB Loading
- `stdGob_xbox.c` is a self-contained C89 reimplementation of stdGob + stdHashTable + stdString + stdFnames.
- Uses real headers (`Win95/stdGob.h`, `General/stdHashTable.h`) for struct layout compatibility.
- Provides its own function implementations to avoid C89/C++ conflicts with the rest of the codebase.
- `jkGob_Startup` stub in `xbox_stubs.c` calls through to real `stdGob_Startup(std_pHS)`.
- `stdFileUtil_NewFind`/`FindNext`/`DisposeFind` are still stubbed (Win32 `_findfirst` won't work on Xbox).

### D3D8 Renderer
- Full implementation exists in `std3D.c` (excluded from Release build).
- Uses Xbox D3D8 API: `Direct3DCreate8`, `CreateDevice`, `CreateTexture`, `DrawIndexedPrimitiveUP`.
- `D3DVERTEX` struct matches the SDL2_RENDER vertex layout from rdCache.c.
- Texture cache converts 8bpp paletted and 16bpp (RGB565/RGB1555) to ARGB32.
- NV2A requires power-of-two textures.
- To re-enable: add `d3d8.lib` (NOT `d3d8-xbox.lib`) and `xgraphics.lib` back to linker, swap stub for real `std3D.c`, remove `#if 0` gate in `std3D_Startup`.

### XBE Generation
- `patchxbe.py` handles PE→XBE conversion:
  1. Patches PE subsystem field from 2 (Win32) to 14 (Xbox)
  2. Strips KERNEL32.dll import descriptor (imagebld rejects it)
  3. Runs `imagebld.exe /IN:patched.exe /OUT:output.xbe`
  4. Injects D3D8/XGRAPHC library version entries into XBE header (CXBX-R uses these for HLE activation)

## Source File Locations (vcproj paths vs disk)

### Correct paths on disk (vcproj paths were wrong for many files):
| vcproj said | Actual disk location |
|---|---|
| `src\std\RTI\stdGob.c` | `src\Win95\stdGob.c` |
| `src\std\General\stdHashTable.c` | `src\General\stdHashTable.c` |
| `src\std\General\stdString.c` | `src\General\stdString.c` |
| `src\std\General\stdFnames.c` | `src\General\stdFnames.c` |
| `src\std\Win95\stdFileUtil.c` | `src\General\stdFileUtil.c` |
| `src\std\General\stdHashKey.c` | Does not exist on disk |

### Xbox platform files (`src\Platform\Xbox\`):
- `main_xbox.c` — Entry point, engine startup sequence, present loop
- `stdFile_xbox.c` — File I/O via NtCreateFile, HostServices table, game root probe
- `stdGob_xbox.c` — Self-contained GOB loader (replaces stdGob.c + dependencies)
- `std3D.c` — Full D3D8 renderer. **Currently active in the build.** If CreateDevice fails, returns early to headless-mode (engine keeps running without rendering).
- `std3D_xbox_stub.c` — D3D stub for headless operation. **Not currently compiled into the build.**
- `d3d8_xbox_minimal.h` — Hand-rolled minimal D3D8 header used by `std3D.c` (instead of XDK's `D3D8-Xbox.h` which requires the missing `d3d8types-xbox.h`). Defines the 68-byte Xbox `D3DPRESENT_PARAMETERS` with preprocessor rename to override PC 52-byte struct from `D3D8Types.h`.
- `test_tut01/tut01_test.cpp` + `build_xbox_test.bat` — standalone Tut01 reproduction. Builds to `build\xbox\test\default.xbe`. Used to isolate toolchain vs project code.
- `Window_xbox.c` — Window/message loop wrapper
- `stdSound_xbox.c` — DirectSound audio (compiled as C with CINTERFACE)
- `stdControl_xbox.c` — Controller input stubs
- `xbox_debug.c` — Debug logging via NtCreateFile + OutputDebugStringA
- `xbox_debug.h` — XDBG/XDBGF macros
- `xbox_stubs.c` — Stub implementations for all unported game modules
- `crt_aliases.c` — CRT symbol aliases (`_memcpy`, `_memset`, `_strcmp`, etc.) + KERNEL32 stubs
- `platform_xbox.h` — Target defines, stdint shim, C89 keyword shims, Win32 console stubs
- `stdPlatform_xbox.c` — stdPlatform_Printf, stdPrintf
- `wuRegistry_xbox.c` — Registry stubs
- `patchxbe.py` — PE→XBE conversion (repo root)

## Current State (snapshot as of last update to this file — May 8 2026)

### What works
- **XBE builds clean.** 0 errors. `build_xbox.bat` from repo root.
- **Boots on real Xbox hardware** (softmod retail via UnleashX → File Explorer).
- **Engine runs end-to-end through to Present.** Log shows:
  - D3D8 device created (xquake-matched d3dpp: X8R8G8B8 / count=1 / D24S8 / MSAA NONE / DISCARD / IMMEDIATE)
  - 60-frame Clear+Swap stability loop completes (visible blue/green flicker on TV)
  - Main_Startup: all subsystems init in order
  - jkRes_Startup: GOB enumeration via Win32 FindFirstFileA, full Res1hi.gob (820 entries) and JK1.gob (296 entries) loaded
  - cog scripts loaded, JKL parsed, level data ready
  - Game tick: jkGame_Update completes; sithRender produces real geometry
  - rdCache builds render list (273 verts / 129 tris of real level surfaces)
  - DrawRenderList → DrawPrimitiveUP returns CPU-side ("DIAG-C: done")
  - EndScene returns
- **`FlushFileBuffers` after every XDBG write** — log accurately reflects last-reached line on hang (without this, buffered writes gave false progress past the actual wedge point).
- **Win32 `FindFirstFileA` for directory enum** — switched from CRT `_findfirst` (flaky); matches Re-Volt and UC2 retail Xbox sources.

### Active wall
- **Present (Swap) hangs after the first real DrawPrimitiveUP.** This is the same wall the prior session described:
  > "Every path tried — DrawIndexedVerticesUP, DrawVerticesUP, DrawVertices (VB), DrawPrimitiveUP via C++ wrapper — has the same signature: CPU-side returns, then Swap (or BlockUntilIdle) hangs forever."
  Confirmed root: `DrawPrimitiveUP` on Xbox is a one-line wrapper around `D3DDevice_DrawVerticesUP` (D3D8.h:2095) — they encode the same NV2A push-buffer commands.
  Frame 0 (engine init only, no real draws) → Present succeeds. Frame 1 (3 real draws queued) → Present hangs. CPU-side draws all return; GPU wedges on command execution; Present blocks waiting for fence that never signals.

### Ruled out today
- **Locked resources as GPU sync barrier.** Both `g_pFallbackTex` (1×1 white) and `g_pDynVB` were Locked but never Unlocked. XDK CHM warns Lock blocks GPU draws using the resource until Unlock. Fixed by adding `D3DTexture_UnlockRect` and switching DrawPrimitiveUP source to a plain CPU heap buffer (`GL_flatVerts`). Same wedge persists.
- **`D3DCREATE_PUREDEVICE`.** xquake uses it; D3DQuakeX (homebrew confirmed working on retail) does not. We removed it (matching D3DQuakeX). Pending hardware test.
- **Recorded push buffer (`BeginPushBuffer`/`RunPushBuffer`).** Replaced with immediate `DrawPrimitiveUP`. Wall persists, so this wasn't the cause; but kept the simpler path.
- **MultiSampleType `2_SAMPLES_MULTISAMPLE_LINEAR`.** Replaced with `D3DMULTISAMPLE_NONE` (xquake match).
- **Buffered log writes hiding the real hang point.** Every prior log was misleading on where execution had actually reached.

### Not yet tested
- Remove PUREDEVICE (build done, awaiting log).
- Explicit `SetPixelShader(0)` to disable pixel-shader path.
- Explicit `SetRenderTarget` / `GetRenderTarget` to verify back buffer + depth surface are bound.
- Match xquake's full per-draw state setup (ALPHAOP, ALPHAARG1/2 for stage 0; explicit DISABLE for stages 1+).

### Not regressed
- Controller input still stubbed in `stdControl_xbox.c` (lazy XInputOpen pattern works).
- CXBX-R: D3D crashes on device creation (known emulator bug, not our code — don't chase).
- sithRender IS wired into the game tick — it produces vertices that reach `std3D_DrawRenderList`. The DIAG-C path discards them and substitutes a hardcoded triangle for the wall investigation; once the wall breaks, switch DIAG-C back to the real engine geometry path.

## Swap hang investigation: current test & hypothesis

> If you are picking this up cold, this section tells you **exactly** what was last tried, what the result was, and what the next experiment is. Update this section after every build/deploy/log cycle so it never drifts.

### Current build state (last write to this file is the source of truth)
- **Last build:** **SetViewport fix.** Root cause of every prior draw hang has now been identified by reading `C:\XDK\xbox\include\D3D8-Xbox.h`:
  1. **`IDirect3DDevice8::DrawPrimitiveUP` is a 1-line wrapper around `D3DDevice_DrawVerticesUP`** (D3D8-Xbox.h line 2282). They are the SAME function. All prior conclusions like "DrawVerticesUP hangs but DrawPrimitiveUP might work" were based on a misconception. FakeGLx's working draws use this exact lib function.
  2. **Missing call: SetViewport.** FakeGLx (`fakeglx.cpp:2487-2499`, dirty-flagged from constructor at line 1230) always calls `m_pD3DDev->SetViewport(...)` before its first draw. We never called it. Without a valid viewport, NV2A's rasterizer/ROP writes pixels to invalid addresses → GPU DMA fault → Swap fence-wait loops forever. This perfectly explains the symptom: Clear+Swap works (Clear engine bypasses the rasterizer), but Draw+Swap always hangs.
  
  Added `D3DDevice_SetViewport` declaration to `d3d8_xbox_minimal.h` (verified present in d3d8ltcg.lib via `dumpbin /linkermember:1`). Added `SetViewport(0,0,640,480,0,1)` call in `std3D_Startup` immediately after `SetFlickerFilter`. The C++ wrapper `xbox_DrawPrimitiveUP` from the previous build is still in place but is now redundant — it calls the same `D3DDevice_DrawVerticesUP`. 0 errors.
- **XBE location:** `build\xbox\release\default.xbe`.
- **Awaiting hardware test.** This is the strongest-confidence fix attempted in the entire investigation. Expected: priming triangle visible (red/green/blue) on cleared blue background; game-loop Swap returns; pixels appear on TV.

### Evidence to date

| Test | d3dpp size | BackBufferCount | SwapEffect | CreateDevice | Swap | Notes |
|---|---|---|---|---|---|---|
| Original | 52 (PC) | 0 | COPY | **hr=0x00000000, dev=0x00056ED0** | **hangs** | Baseline. Log terminated right after "main: -> Present". |
| Canonical-XDK | 52 (PC) | 1 | DISCARD | **hr=0x00000000** | **hangs** | Matched XDK Tut01 d3dpp exactly. Ruled out swap-chain config as cause. |
| + VBlank probe | 52 | 1 | DISCARD | hr=0 | hangs | `BlockUntilVerticalBlank` returned before Swap hung → video encoder is fine. |
| + KickPushBuffer | 52 | 1 | DISCARD | hr=0 | hangs | Explicit pushbuffer kick before Swap didn't help. |
| Swap-only (no Clear) | 52 | 1 | DISCARD | hr=0 | **hangs even with no prior GPU work** | Proves hang is not about what we queue; fundamental to device state. |
| Tut01 test XBE (C++ vtable) | 52 | 1 | DISCARD | **CreateDevice hangs** | — | Via vtable dispatch. Proved `Direct3DCreate8` returns sentinel `0x1`, not a real COM object. Flat C API is the only viable path. |
| d3d8i.lib (Profile variant) | 52 | 1 | DISCARD | hr=0 | hangs | Lib swap didn't help. |
| d3d8ltcg+xgraphicsltcg pair | 52 | 1 | DISCARD | hr=0 | hangs | XDK-canonical LTCG pair didn't help. |
| **Struct fix (68-byte)** | **68 (Xbox)** | **1** | **DISCARD** | **hangs** | — | Hang moved from Swap → CreateDevice. Confirms lib really does read 68 bytes. |
| Isolation (built not tested) | 68 | 0 | COPY | untested | untested | `default_isolation.xbe` preserved but superseded; SDLx match is stronger evidence. |
| App surfaces (pre-CreateDevice) | 68 | 1 | DISCARD | **hangs** | — | Pre-allocated back/front/depth via `CreateSurface2`, set in `BufferSurfaces[]`. `EnableAutoDepthStencil=FALSE`. CreateDevice still hung — surface pre-alloc is not the issue. |
| SDLx match (LIN_X8R8G8B8) | 68 | 1 | DISCARD | **hangs** | — | `BackBufferFormat=D3DFMT_LIN_X8R8G8B8`, `EnableAutoDepthStencil=TRUE`, `Flags=PROGRESSIVE/INTERLACED`, no `PresentationInterval`, NULL surfaces. Matched SDLx exactly — still hangs. `AutoDepthStencilFormat=D3DFMT_D24S8` (PC value 75) still in use. |
| LIN_D24S8 depth (wrong) | 68 | 1 | DISCARD | **hangs** | — | Tried `AutoDepthStencilFormat=D3DFMT_LIN_D24S8` (0x2E). Wrong direction — 0x2E is the linear variant, not what SDLx uses. CreateDevice still hung. |
| Xbox D3DFMT_D24S8=0x2A | 68 | 1 | DISCARD | **hr=0x00000000 dev=00056ED0** | **hangs** | Depth format fix worked — CreateDevice now returns hr=0. Swap still hangs (Phase 2a). |
| Reset+FlickerFilter | 68 | 1 | DISCARD | hr=0 | **hangs** | Reset hr=0, SetFlickerFilter done, but game-loop Swap still hangs. Reset alone does not start PCRTC. |
| Priming Clear(TARGET)+Swap | 68 | 1 | DISCARD | hr=0 | priming **returns**, game-loop **hangs** | Priming Swap in startup returned — GPU works. Game-loop Swap (after StartScene's `Clear(TARGET\|ZBUFFER\|STENCIL)`) still hangs. Confirms ZBUFFER/STENCIL clear is the cause. D3DCLEAR values verified same on Xbox (0x1/0x2/0x4). NV2A hangs on depth-surface write — likely DMA mapping issue with auto-allocated D24S8 tiled surface. |
| TARGET-only Clear | 68 | 1 | DISCARD | hr=0 | **hangs** | Removed ZBUFFER/STENCIL from `std3D_StartScene` Clear. Still hangs — hang is not from the Clear flags. Root cause is elsewhere: see FakeGLx plan. |
| **FakeGLx pattern** | **68** | **1** | **DISCARD** | **hr=0** | tick 0 **returns**, tick 1 **crash after level load** | SetPushBufferSize(768K/128K), D3DFMT_D16, D3DFMT_LIN_A8R8G8B8 textures, no Reset. Tick 0 Swap returned ✓ — FakeGLx fixed the basic Swap hang. Tick 1 (after sithMain_Mode1Init level load) crashes somewhere between "SetVideoMode: done" and "main: -> EndScene". Exact crash location TBD by jkMain.c trace build. |
| jkMain traces only | 68 | 1 | DISCARD | hr=0 | **?** | Dense JKTRACE in GameplayShow/LABEL_35/GameplayTick. NOT YET TESTED — superseded by next row. |
| +sithMain_Tick traces | 68 | 1 | DISCARD | hr=0 | **crash at ReadControls** | sithMain_Tick else branch traced. Last line: `sithTick: dt=0.000000 frames=0 ReadControls`. Crash inside `sithControl_ReadControls()` → `XInputGetState(0, &state)`. Root cause: XDK original Xbox `XInputGetState` takes HANDLE, not port number. Passing 0 = NULL handle → crash. |
| XInput handle fix | 68 | 1 | DISCARD | hr=0 | **loop STABLE — Swap every tick** | XInitDevices+XInputOpen+HANDLE fix + wholeFramesToApply>0 guard. sithTick traces reach return0 ✓. GameplayTick nullsub+jkGame_Update+returning ✓. 489KB log = thousands of Swap pairs, no crash. TV shows cleared/black screen (no sithRender wired yet). |
| sithRender wired (prev) | 68 | 1 | DISCARD | hr=0 | **loop stable, TV unknown** | `jkGame_Update` calls rdAdvanceFrame→sithMain_UpdateCamera→rdFinishFrame. Game runs to tick 800+, no crash. TV output undetermined (no render diagnostics in that build). |
| render diagnostics (prev) | 68 | 1 | DISCARD | hr=0 | **geoMode=4 faces=0, no sithRender output** | `jkGame_Update` logs correctly. `sithRender_Draw` traces used stdPlatform_Printf → OutputDebugStringA only, invisible on hardware. All three ticks showed faces=0 — sithRender_Draw either not called or returning early. Root cause unknown. |
| fixed traces (XDBGF) | 68 | 1 | DISCARD | hr=0 | **crash at 2nd rdCache_DRL** | DRL: first batch (273v,129t) succeeds. rdCache_DRL: verts=725 normTris=303 logged; crash before DRL2: VerticesFinish done. Root cause: `std3D_RenderListVerticesFinish` double-locked `g_pVB` (once per batch, no intervening Unlock). DrawIndexedVerticesUP doesn't use VB — lock was dead code. |
| remove VB lock + ARV trace | 68 | 1 | DISCARD | hr=0 | **both batches draw, Swap hangs** | VB double-lock crash fixed. Both batches (273v+129t, 725v+303t) complete. DrawIndexedVerticesUP called. Swap hangs AFTER all CPU-side draw calls return. GPU receiving real draw work for first time. `std3D_DrawRenderList` returns normally (jkGame_Update done logged), Swap is the hang point. |
| BlockUntilIdle before Swap | 68 | 1 | DISCARD | hr=0 | **GPU stuck on draw commands** | BlockUntilIdle logged but never returned — NV2A deadlocked on DrawIndexedVerticesUP commands. DrawVerticesUP (FakeGLx's proven path) was never tried. |
| DrawVerticesUP (flat expand) | 68 | 1 | DISCARD | hr=0 (expected) | **never hardware tested** | Replaced DrawIndexedVerticesUP with DrawVerticesUP. Superseded before hardware test — render-state fixes applied on top. |
| render-state audit (FakeGLx match) | 68 | 1 | DISCARD | hr=0 | **Swap hangs after game-loop draws** | CULLMODE=NONE, ALPHATESTENABLE, SPECULARENABLE=FALSE, DITHERENABLE=FALSE, BlockUntilIdle removed. DrawVerticesUP CPU-side returns ("done" logged for all 3 batches of DRL#1; DRL#2-4 also return CPU-side), but game-loop Swap hangs. GPU deadlocks — fence-wait in Swap never completes. pal=00000000 for all textures (NULL palette handled by `if(pal)` guard → white texels, not garbage). |
| skip DrawVerticesUP (TEST A) — first run | 68 | 1 | DISCARD | hr=0 | **crash at XInitDevices** | Log only 74 lines — stopped at "LoadCD OK, entering game". XInitDevices hung before logging "done". USB enumeration timing issue: binary layout shift (different code size) changed call timing. Previous builds happened to be fine; this boot wasn't. |
| skip DrawVerticesUP + Sleep(500) — XInitDevices still hangs | 68 | 1 | DISCARD | hr=0 | **XInitDevices hangs (log stops at "calling XInitDevices")** | Sleep(500) didn't help. XInitDevices consistently blocks. Root cause: XDK requires XInitDevices before D3D init — we were calling it after CreateDevice, which worked by luck in prior builds. |
| XInitDevices moved to main() before D3D; skip DrawVerticesUP (TEST A confirmed) | 68 | 1 | DISCARD | hr=0 | **Swap returned ✓ — TEST A confirmed** | Game ran to tick 13700+. Swap returned every frame. v[0] x=191 y=382 z=949(×0.001=0.949) color=0xFF000000. XInitDevices ordering fix: must call before D3D CreateDevice. TEST A result: DrawVerticesUP is the GPU hang source (Swap returns when DVU skipped; SetTexture/SetRenderState alone are safe). |
| TEST B first run — XInputOpen blocked at Startup | 68 | 1 | DISCARD | hr=0 | **XInputOpen hung** | Log stopped at `stdControl_Startup: calling XInputOpen` (line 76). USB enumeration not yet complete when Startup runs. XInputOpen blocks waiting for USB. Fix: defer XInputOpen to first ReadControls call (lazy-open, game-loop level, USB definitely ready). |
| TEST B + deferred XInputOpen | 68 | 1 | DISCARD | hr=0 | **Swap hangs** | DrawVerticesUP done logged at line 295 (CPU-side returned). But Swap hangs at end of tick 1 (after 2 more DRL calls with s_done=1). GPU got stuck executing the DrawVerticesUP. rdTri.flags=0x1833 (opaque, bit 0x800 set). XInputOpen deferred fix: lazy-open in ReadControls via g_openAttempted. |
| TEST B2 — XYZRHW (bypasses vertex shader) | 68 | 1 | DISCARD | hr=0 | **Swap hangs** | XYZRHW also hangs. Vertex shader not the issue. Same pattern: DVU CPU-side done, Swap never returns. Both XYZ and XYZRHW paths hang. |
| TEST B2 + swizzled back buffer | 68 | 1 | DISCARD | hr=0 | **Swap hangs in Startup** | Priming DrawVerticesUP (XYZRHW, zero state, swizzled BB) added to Startup — hangs before level load, before any game state. Confirms DrawVerticesUP is fundamentally broken on NV2A under XDK 5849 regardless of FVF, render state, or back buffer format. DrawVerticesUP is dead. Pivot: explicit vertex buffer + D3DDevice_DrawVertices. |
| DIAG-C — DrawVertices (VB path) first run | 68 | 1 | DISCARD | hr=0 | **Swap hangs in Startup (priming DrawVertices)** | VB created (pVB=004A1010 locked=83BDC000). DrawVertices CPU-side returns. Swap after DrawVertices hangs — same pattern as DrawVerticesUP. VB draw type makes no difference. Root cause: D3DCREATE_HARDWARE_VERTEXPROCESSING. NV2A vertex microcode hangs on ALL draw calls regardless of FVF or draw API. Fix: switch to D3DCREATE_SOFTWARE_VERTEXPROCESSING (FakeGLx's confirmed path — CPU transforms vertices, bypasses NV2A vertex microcode entirely). |
| SOFTWARE_VERTEXPROCESSING | 68 | 1 | DISCARD | hr=0 | **Swap hangs in Startup (priming DrawVertices)** | SOFTWARE_VP made no difference — same hang as HARDWARE_VP. DrawVertices CPU-side returns, Swap hangs. Not a vertex shader / microcode issue. Root cause: NV2A TMU always executes stage-0 texture fetch during rasterization, regardless of COLOROP setting. NULL texture DMA object → GPU DMA fault → Swap fence-wait loops forever. Fix: create 1×1 white fallback texture (g_pFallbackTex) in Startup; bind it for all draw calls instead of NULL. MODULATE(white, diffuse) = diffuse → correct untextured output. |
| fallback texture + SOFTWARE_VP | 68 | 1 | DISCARD | hr=0 | **NOT DEPLOYED** | Superseded before hardware test. SOFTWARE_VP is wrong (FakeGLx uses HARDWARE). DrawVertices is wrong (FakeGLx uses DrawPrimitive). |
| DrawPrimitive linker error (never deployed) | 68 | 1 | DISCARD | hr=0 | **never tested** | `D3DDevice_DrawPrimitive`/`DrawPrimitiveUP` are inline-only in D3D8-Xbox.h — NOT exported from d3d8ltcg.lib. Linker error: unresolved external. Build failed, XBE not produced. |
| DrawPrimitiveUP via C++ wrapper (not deployed) | 68 | 1 | DISCARD | hr=0 (expected) | **never tested** | Created `std3D_draw.cpp` exporting `xbox_DrawPrimitiveUP` (extern "C") that casts `D3DDevice*` to `IDirect3DDevice8*` and calls the inline method. Subsequently invalidated by reading D3D8-Xbox.h directly: `IDirect3DDevice8::DrawPrimitiveUP` is literally `{ D3DDevice_DrawVerticesUP(...); return S_OK; }` — same function we already proved "hangs". Build was correct but would have produced identical hang behaviour. |
| SetViewport fix (deployed) | 68 | 1 | DISCARD | hr=0 | **Swap still hangs** | SetViewport(0,0,640,480,0,1) added after SetFlickerFilter. Did not unblock the wall. Engine continues running past D3D init and reaches game tick — viewport was needed but not the wedge cause. |
| Re-Volt-style d3dpp | 68 | 2 | DISCARD | hr=0 | **Swap hangs** | A8R8G8B8 / BackBufferCount=2 / D24S8 / MSAA NONE — matched against Rayman 4, Mercenaries, Re-Volt, UC2 retail sources. No fix. |
| Stability loop replaces priming draw | 68 | 2 | DISCARD | hr=0 | **stability OK, real-draw Swap still hangs** | 60-frame Clear+Swap loop confirms present chain is solid. As soon as a real DrawPrimitiveUP is queued (engine tick), next Swap wedges. |
| Win32 FindFirstFileA for dir enum | — | — | — | — | **GOB load works reliably** | Replaced CRT `_findfirst` (flaky on retail) with Win32 `FindFirstFileA`. Matches Re-Volt and UC2 patterns. |
| FlushFileBuffers after every XDBG | — | — | — | — | **Log now trustworthy** | Without this, buffered writes survived past Xbox-side hangs and gave false progress in earlier logs. |
| xquake-matched d3dpp + PUREDEVICE | 68 | 1 | DISCARD | hr=0 | **Swap still hangs** | X8R8G8B8 / count=1 / D24S8 / NONE / DISCARD / IMMEDIATE / `D3DCREATE_HARDWARE_VERTEXPROCESSING\|D3DCREATE_PUREDEVICE` — exact xquake gl_fakegl.cpp:1579-1597. Frame 0 Present OK; Frame 1 (3 real DrawPrimitiveUPs of 129/303/129 tris) → Present hangs. |
| Unlock fallback texture, drop locked-VB pattern | 68 | 1 | DISCARD | hr=0 | **Swap still hangs** | XDK CHM (LockRect) says D3DLOCK_NOOVERWRITE keeps Lock from blocking GPU draws → default Lock makes resource unusable until Unlock. Both xquake and D3DQuakeX use ordinary CPU heap buffers as DrawPrimitiveUP source. Fixed: D3DTexture_UnlockRect after writing fallback texture; replaced g_pLockedVB with GL_flatVerts (heap). Wall persists — locked-resource theory disconfirmed as sole cause. |
| **CURRENT: drop PUREDEVICE** | **68** | **1** | **DISCARD** | hr=0 (expected) | **?** | D3DQuakeX (homebrew confirmed working on retail) does NOT use PUREDEVICE; xquake (MS sample) does. PUREDEVICE means D3D doesn't track redundant state — caller fully responsible for re-applying every frame. Our state setup is partial, so PUREDEVICE may leave NV2A registers stale. Removed PUREDEVICE; CreateDevice flags now `D3DCREATE_HARDWARE_VERTEXPROCESSING` only (matches D3DQuakeX). 0 errors. Pending log. |

### Active hypothesis

**Engine is stable through to Present.** Every subsystem from boot through game tick produces correct data and reaches `std3D_Present: calling Swap` reliably. Frame 0 (no real draws) Presents successfully; Frame 1 (real DrawPrimitiveUP commands queued) wedges in Swap.

**The wall is at NV2A command execution, not draw submission.** CPU returns from every draw call. GPU receives queued commands and stalls on execution. Present blocks on a fence that never signals.

**Likely causes still on the table** (in order of cheapness to test):
1. **PUREDEVICE** — currently being tested. If that doesn't fix it, eliminate.
2. **Implicit pixel shader bound at CreateDevice** — Xbox D3D8 has 50+ `D3DRS_PS*` register-combiner states. `SetPixelShader(0)` may need to be called explicitly to force the fixed-function ROP path; without it, NV2A may be running uninitialized combiner microcode.
3. **Render target / depth surface not bound at draw time.** AutoDepthStencil should bind, but verify with `GetRenderTarget` / `GetDepthStencilSurface` calls.
4. **Incomplete texture stage state.** XDK XBR::SetDefaultState (canonical Microsoft default-state function) sets ALPHAOP, ALPHAARG1/2, ADDRESSU/V, MAGFILTER, MINFILTER, MAXANISOTROPY, ALPHAKILL, COLORKEYOP, COLORKEYCOLOR for stages 0-3. We set only COLOROP / COLORARG1 / COLORARG2 for stage 0. Xbox documented defaults *should* cover the rest, but if ANY register is at boot-time garbage from device init, NV2A may wedge.
5. **The wall requires hardware-level NV2A debugging.** If exhaustive code-side fixes don't unblock, the next step is using XDK's debug overlay or capturing the actual push buffer commands with a tool like nv2a_log to see what NV2A sees when it wedges.

### Ordered next steps

- **If CURRENT → `DrawRenderList: verts=N tris=N` in log AND geometry on screen:** rendering works. Next: implement `sithTime_Tick` (real `GetTickCount` delta), wire `jkPlayer_DrawPov` (weapon), HUD.
- **If CURRENT → `DrawRenderList` logged but black/wrong screen:** triangles reaching D3D but invisible. Check vertex transform (rdCamera wired?), texture bind (pTex NULL?), blend state.
- **If CURRENT → `faces=N>0` but no DrawRenderList:** rdCache_Flush sorts/passes faces but std3D_DrawRenderList bails (`GL_numTris==0`). Missing call to std3D_RenderListVerticesBegin or similar — check rdCache_SendFaceListToHardware → std3D flow.
- **If CURRENT → `faces=0` after UpdateCamera AND `sithRender_Draw: proceeding`:** geometry submits nothing. Sector/surface culling issue or `rdCache_numProcFaces` not incrementing. Add trace in sithRender_KindaClip to count surfaces processed.
- **If CURRENT → `sithRender_Draw: geoMode=0`:** `sithRender_SetGeoMode(0)` called from SetVideoMode. Fix: after SetVideoMode, force `sithRender_geoMode = RD_GEOMODE_TEXTURED` (0x3) via stub.
- **If CURRENT → `sithRender_Draw: cam=NULL or sector=NULL`:** camera not set up. Check `sithCamera_SetCurrentCamera` is called after level load (GameplayShow: sithCamera_SetsFocus is logged — trace what that does).
- **If CURRENT → crash at `sithTick: Console`:** `sithConsole_AdvanceLogBuf` crashes. Likely needs `sithConsole_Startup` called. Check what it needs and stub/call it.
- **If CURRENT → crash at any inner-loop trace (SoundMixerTick/EventAdv/etc.):** `wholeFramesToApply` was non-zero (unexpected — sithTime still stubbed). Investigate how.
- **If CURRENT → Swap hangs (game-loop Swap after sithTick return0):** sithMain_Tick side-effects corrupt GPU state. Add `return 1;` at top of sithMain_Tick to verify empty tick is safe; then bisect.
- **If CURRENT → stdControl_Startup: XInitDevices crashes:** XInitDevices not available or wrong signature. Fallback: stub `stdControl_Startup` to no-op (the `g_hController==NULL` guard in ReadControls already prevents the old crash; sithMain guard prevents ReadControls with frames=0).
- **If CURRENT → Swap returns AND pixels appear:** wire sithRender.
- **Dead end archived:** LIN_D24S8 (0x2E) wrong value; app-surface pre-allocation; SDLx match without depth-format fix; D3DFMT_D24S8=0x2A alone; TARGET-only Clear; FLIP=DISCARD; UnlockRect-as-cause; swizzled textures with LockRect(flags=0); XInputGetState(port, state) — wrong API for original Xbox.

## FakeGLx Migration Plan

> **STATUS: APPLIED AND PARTIALLY VERIFIED.** Tick 0 Swap returned ✓. Tick 1 crash pending trace diagnosis. This section is archived reference — do not re-apply.

### Evidence basis

FakeGLx (D3DQuakeX, `github.com/rodolforg/D3DQuakeX`) is the only documented working 3D renderer on original Xbox softmodded hardware outside of retail games. Its D3D8 init sequence is the proven reference:

```cpp
// From fakeglx/fakeglx.cpp (verbatim):
m_pD3D->SetPushBufferSize(768*1024, 128*1024);  // BEFORE CreateDevice
params.AutoDepthStencilFormat = D3DFMT_D16;      // 0x2C on Xbox
params.SwapEffect              = D3DSWAPEFFECT_FLIP;
params.BackBufferFormat        = D3DFMT_X8R8G8B8; // swizzled OK for back buffer
// No Reset() call after CreateDevice
// No SetFlickerFilter (or value 1, not 5)
```

FakeGLx also uses `D3DTexture_UnlockRect` correctly — it calls the real library function.

### Changes applied (this build)

**`d3d8_xbox_minimal.h`:** No change. `D3DTexture_UnlockRect` no-op stub is correct — XDK CHM: *"This function does nothing."* `D3DVertexBuffer_Unlock` is the same.

**`std3D.c` — `std3D_AddToTextureCache`:**

Switch texture format from swizzled to linear:
```c
// OLD: D3DFMT_A8R8G8B8 (0x06) — swizzled/tiled
pTex = D3DDevice_CreateTexture2(potW, potH, 1, 1, 0, D3DFMT_A8R8G8B8,    D3DRTYPE_TEXTURE);
// NEW: D3DFMT_LIN_A8R8G8B8 (0x12) — linear, no tiling
pTex = D3DDevice_CreateTexture2(potW, potH, 1, 1, 0, D3DFMT_LIN_A8R8G8B8, D3DRTYPE_TEXTURE);
```
Rationale: XDK CHM LockRect doc — calling `LockRect(flags=0)` on a tiled surface without `D3DLOCK_TILED` means *"CPU reads and writes [do not] properly respect the tile remapping setting"*. Linear format has no tiling; `LockRect(flags=0)` returns a correct linear pointer.

Fix the LockRect check (previously overwrote `hr` with `S_OK` before checking it):
```c
// OLD (bug — hr always S_OK regardless of LockRect result):
D3DTexture_LockRect(pTex, 0, &lr, NULL, 0);  hr = S_OK;  if (FAILED(hr)) ...
// NEW:
D3DTexture_LockRect(pTex, 0, &lr, NULL, 0);  if (!lr.pBits) ...
```

**`std3D.c` — `std3D_Startup`:**

Add `SetPushBufferSize` before `CreateDevice`:
```c
Direct3D_SetPushBufferSize(768*1024, 128*1024);  /* FakeGLx pattern */
```

Switch depth format: `D3DFMT_D24S8` → `D3DFMT_D16` (0x2C). No stencil channel; `D3DCLEAR_STENCIL` silently ignored.

Remove `D3DDevice_Reset()` — FakeGLx does not call it; no documented requirement.

**`std3D.c` — `std3D_StartScene`:**

Restore depth clear (safe now with D16):
```c
D3DDevice_Clear(0, NULL, D3DCLEAR_TARGET | D3DCLEAR_ZBUFFER, ...);
```

**Note on `D3DSWAPEFFECT_FLIP`:** NOT changed. XDK CHM Swap pseudo-code: *"D3DSWAPEFFECT_FLIP is treated identically to D3DSWAPEFFECT_DISCARD."* Changing it would have no effect.

### Expected log if the fix works

```
std3D_Startup: Direct3D_CreateDevice
std3D_Startup: CreateDevice hr=0x00000000 dev=0x...
std3D_Startup: SetFlickerFilter done
std3D_Startup: priming Clear
std3D_Startup: priming Clear done
std3D_Startup: priming Swap
std3D_Startup: priming Swap returned
std3D_Startup: D3D8 device ready ...
...
main: tick 0
main: -> StartScene
main: -> GuiAdvance
main: -> EndScene
main: -> Present
std3D_Present: calling Swap
std3D_Present: Swap returned         ← THE LINE WE NEED
main: -> Sleep
main: tick 1
...
```

TV should show repeated black frames (Clear colour). Any non-freeze = success. Next step after this: wire sithRender, confirm non-black pixels.

### Dead ends already investigated (don't repeat)
- **Surface pre-allocation via `D3DDevice_CreateSurface2` before CreateDevice:** surfaces allocated successfully (non-NULL), set in `BufferSurfaces[]`/`DepthStencilSurface`, `EnableAutoDepthStencil=FALSE`. CreateDevice still hung. The allocator path is not what's hanging.
- **Swap chain BackBufferCount/SwapEffect tuning in isolation:** ruled out as the sole cause (table above).
- **LTCG lib pairing** (d3d8ltcg + xgraphics vs d3d8ltcg + xgraphicsltcg): neither pairing mattered with the 52-byte struct.
- **`d3d8.lib` (non-LTCG 3-symbol stub):** cannot link; only exports `Direct3DCreate8`, `ValidatePixelShader`, `ValidateVertexShader`. XDK Release config depends on header-inlined implementations that `d3d8_xbox_minimal.h` doesn't use. Not viable.
- **`d3d8i.lib` (Profile):** links and builds, same hang.
- **`d3d8-xbox.lib` (debug lib at `C:\XDK\lib\`):** "hangs on retail hardware" per previous note in this file. Don't try.
- **D3D_SDK_VERSION=0 vs 120:** fixed (now 120); was not the hang cause (Direct3DCreate8 returns non-NULL sentinel regardless).
- **patchxbe.py library-table injection** (D3D8/XGRAPHC entries added post-imagebld, intended for CXBX-R HLE detection): not the cause of the hang. Injection is currently disabled; re-enable via `PATCHXBE_INJECT_LIBS=1` env var if testing on CXBX-R.
- **C++ vtable dispatch** (`g_pD3D->CreateDevice(...)` as in XDK Tut01): fundamentally unavailable on our toolchain. `Direct3DCreate8` returns an opaque sentinel (value `0x1`), not a real COM object with a vtable. Dereferencing it hangs. **Flat C API (`Direct3D_CreateDevice`) is the only viable path.** Test XBE at `build/xbox/test/default.xbe` confirmed this; source at `test_tut01/tut01_test.cpp`.
- **"Softmod quirk" as an explanation for CreateDevice hang:** ruled out. Softmods don't replace the kernel — retail games' D3D works fine on softmodded Xboxes. Whatever's hanging is about our parameters or our call pattern, not the runtime environment.

### Diagnostic tooling currently in code
- `std3D_Startup`: logs `Direct3DCreate8`, `Direct3D_CreateDevice`, then `CreateDevice hr=0x%08X dev=%p` on return. If the last line you see is "Direct3D_CreateDevice" (without "hr=..."), CreateDevice hung.
- `std3D_Present`: logs `calling Swap` and `Swap returned`.
- `main_xbox.c`: logs first 5 game-loop iterations by name (StartScene, GuiAdvance, EndScene, Present, Sleep) and every 100th tick thereafter.
- `jkMain.c` (CURRENT TRACES BUILD): dense `JKTRACE`/`JKTRACEF` calls in `jkMain_GameplayShow` (SetVideoMode ok/ToggleCursor/Flush/jkGame_Update/thing_eight/done), `jkMain_GuiAdvance` LABEL_35 (stopTick/state/tickFunc ptr/calling/returned/returning), and `jkMain_GameplayTick` (enter/tick/sithMain_Tick/nullsub/jkGame_Update check+call+done/returning). Each fires at most 3 times (static counter guard).
- `sithMain.c` (CURRENT TRACES BUILD): `XDBG` at every major step of `sithMain_Tick`'s single-player else branch. `sithTick: enter else` → `ResumeMusic` → `TimeTick` → `dt=X frames=N ReadControls` → `ControlTick` → `FinishRead` → (if loop runs: `loop i=N SoundMixerTick` → `EventAdv` → `AITickAll` → `SurfaceTick` → `ThingTickAll` → `MotsTick` → `CogTickAll` → `loop end`) → `Console` → `HandleTimeLimit` → `GamesaveFlush` → `return0`. Each fires once (static counter `_st<1` guards). Key: "dt=0.000000 frames=0" expected (sithTime stubs), so loop should NOT run.
- Canary probes (Swap-only, KickPushBuffer, device-byte dump, StartScene sub-logs) have been **removed** — they served their diagnostic purpose.

### Comprehensive research notes
**`build/xbox/research_notes.md`** (~300 lines) — reference material extracted from the XDK CHM (now also decompiled at `build/xbox/chm_extract/`), Cxbx-Reloaded source, nxdk source, and XboxDev wiki. Covers: D3D API reference (CreateDevice, Swap pseudo-code, Reset, CreateTexture, D3DFORMAT values with render-target restrictions, enum-value gotchas), pushbuffer concepts, XBE format (cert flags, library-flag bit layout, XOR keys), DirectSound init (can't be called from C++ global ctors), XInput init pattern, memory architecture, nxdk/pbkit as backup if MS D3D ever proves incompatible. **Read this before trying anything new** — it will save hours of re-researching.

## Bugs Fixed This Session

### 1. `stdConffile_OpenRead` was stubbed (ROOT CAUSE of JKL open failure)
- `xbox_autostubs.c` had `int stdConffile_OpenRead(char * a0) { return 0; }` — silently failed every file open through stdConffile.
- `sithWorld_Load` calls `stdConffile_OpenRead("jkl\01narshadda.jkl")` which goes through `stdConffile_OpenModeCommon(fpath, "r", 0)` → `std_pHS->fileOpen(fpath, mode)`.
- **Fix:** Added `src\General\stdConffile.c` to `build_xbox.bat`, removed all stdConffile stubs from `xbox_autostubs.c` (OpenRead, Printf, ReadArgs, ReadLine, WriteLine, Close).

### 2. GOB entry case mismatch
- The real `stdHashTable` (in `stdHashTable.c`, compiled as C++) uses case-sensitive `_strcmp` for key comparison AND a case-sensitive hash function (`65599 * hash + char`).
- `stdGob_FileOpen` lowercases the search key before lookup.
- GOB entries stored from binary in original case → hash mismatch AND strcmp mismatch.
- **Fix:** In `stdGob_xbox.c` GOB load loop, entries are now lowercased + slash-normalized inline at load time (before `stdHashTable_SetKeyVal`).

### 3. `stdString_CStrToLower` crash via `__tolower`
- `stdString_CStrToLower()` calls `__tolower()` from `jk.h`.
- `jk.h` line 326 declares `char __tolower(char a)`. `crt_aliases.c` line 31 defines `int __cdecl __tolower(int c)`. Signature mismatch.
- Crashed on first GOB entry normalization attempt.
- **Fix:** `stdGob_xbox.c` uses inline `if (*p >= 'A' && *p <= 'Z') *p += 32` instead of `stdString_CStrToLower`. Also added `#ifdef TARGET_XBOX` inline path in `stdString.c:stdString_CStrToLower`.

### 4. `_sscanf` broken va_list wrapper
- `crt_aliases.c` wrapped sscanf with va_list which can't forward variadic args (XDK lacks vsscanf).
- **Fix:** `#define _sscanf sscanf` in `platform_xbox.h`, removed broken wrapper.

### 5. Episode GOB not loaded
- `jkGuiMain_Show` stub called `jkMain_SwitchTo5` without loading the episode GOB first.
- **Fix:** Added `jkRes_LoadGob("JK1")` call before `SwitchTo5`.

### 6. `stdDisplay_VBufferNew` returned NULL
- Stubbed to return 0, causing all material loading to fail (texture allocation).
- **Fix:** Implemented software VBuffer allocator (malloc'd pixel data).

### 7. VBufferNew rejected 0-size mipmaps
- Mipmap loop halves dimensions; small textures hit 0 which was rejected.
- **Fix:** Clamp to 1x1 minimum.

### 8. `rdSprite_NewEntry` stubbed to 0
- **Fix:** Return 1 with name copy.

### 9. `rdThing_SetModel3` stubbed to 0
- Caused NULL dereference on `model3->radius` during template loading.
- **Fix:** Store model pointer in `rdThing` struct.

### 10. sithCog 1MB struct (COG_DYNAMIC_STACKS)
- `QOL_IMPROVEMENTS` set `SITHCOGVM_MAX_STACKSIZE` to 0x10000, making each `sithCog` ~1MB.
- 91 cogs x 1MB = 95MB, exceeding Xbox 64MB RAM.
- **Fix:** Define `COG_DYNAMIC_STACKS` in `platform_xbox.h` for heap-allocated stacks.

## Systemic Issue: jk.h CRT Function Pointers

**IMPORTANT for future un-stubbing work:**

`jk.h` has TWO paths controlled by `#ifdef WIN32_BLOBS`:
- **Lines 41-275 (`WIN32_BLOBS` defined):** Static function pointers to hardcoded Win32 EXE addresses (0x5xxxxx). DEADLY on Xbox.
- **Lines 277-384 (`WIN32_BLOBS` NOT defined):** Proper function declarations that link to `crt_aliases.c`.

**`WIN32_BLOBS` is NOT defined for Xbox** (only set in `cmake_modules/plat_hooks.cmake` for PC hook builds). So the Xbox build takes the safe `#else` path. The CRT functions (`_memcpy`, `_strtok`, `_strchr`, `_sprintf`, etc.) are proper extern declarations resolved by `crt_aliases.c` at link time.

**Exception:** `__tolower` has a signature conflict between `jk.h` (`char __tolower(char)`) and `crt_aliases.c` (`int __cdecl __tolower(int)`). This is worked around in `stdString.c` with inline code under `#ifdef TARGET_XBOX`. Be cautious of any other code that calls `__tolower` directly.

## Diagnostic Logging In Place
The following trace logging exists and should be trimmed once file open is confirmed working:
- `jkRes.c:jkRes_FileOpen` — logs every call, dumps all 5 GOB directory slots, logs each `stdGob_FileOpen` attempt
- `jkRes.c:jkRes_LoadNew` — logs `stdFileUtil_NewFind` results, each GOB file found, final numGobs count  
- `jkRes.c:jkRes_LoadGob` — logs episode GOB loading
- `jkRes.c:jkRes_NewGob` — logs `util_FileExistsLowLevel` check
- `stdGob_xbox.c:stdGob_FileOpen` — logs GOB name, filepath, normalized lookup key, and hash table result

## Next Steps (in order)
1. Fix D3D8 Present/Swap hang on hardware
2. Get first frame rendered (even just a clear color)
3. Wire sithRender into game tick for level geometry
4. Re-enable texture upload to GPU via std3D texture cache
5. Controller input via stdControl_xbox.c

## CRT Aliases Required
The game source uses underscore-prefixed CRT names. `crt_aliases.c` provides:
`_memcpy`, `_memset`, `_strcmp`, `_strncpy`, `_strtok`, `_atoi`, `_sprintf`, `__vsnprintf`, `__strcmpi`, `_qsort`, `_wcsncpy`, `_memcmp`, `_strlen`, `_wcslen`, `ceilf`, `SetCurrentDirectoryA`, `HeapValidate`, `RtlUnwind`, `_except_handler4`
