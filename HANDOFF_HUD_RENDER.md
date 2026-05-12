# HUD render handoff — OpenJKDF2 Xbox port

## Observed runtime behavior (user-reported, current build)

Flashed XBE: `build/xbox/release/default.xbe` produced by `build_xbox.bat` at
the current HEAD of the working tree (uncommitted changes in
`src/Platform/Xbox/std3D.c` listed in §"Recent attempts that did not change
the symptom" below).

In-game observations on real Xbox + Cxbx-Reloaded:

- **Renders correctly:** world geometry, textured world surfaces, sprites,
  blaster bolts (after Codex's `src/Raster/rdCache.c:1185` fix).
- **Renders, but pixelated:** the in-game inventory bitmap (the bitmap that
  appears when an inventory item is shown).
- **Renders, dynamically updates:** the small state-indicator icons inside
  the HUD region — health/shield/ammo. They change as those values change
  in-game.
- **Does NOT render at all:** the two large status-panel backgrounds
  (left/right corner circles).
- **Does NOT render at all:** any digit fonts in the HUD (ammo number,
  health number, shield number, etc.) — the screen positions where they
  should appear are empty.

That is the *entire* set of facts about visual output. Nothing else is
known about what is on screen.

## Facts about the assets (verified by direct GOB extraction)

Dumped via a one-off Python script reading `bm_dump.txt`-style structured
data from `build/xbox/debug/JediKnight/Res1hi.gob`:

```
ui\bm\statusleft.bm     palFmt=1 numMips=1 colorkey=0x0    is16bit=0 bpp=8  rgb_bits=(0,0,0)  shifts=(0,0,0)   mip0: 59x60
ui\bm\statusleft16.bm   palFmt=1 numMips=1 colorkey=0xF81F is16bit=1 bpp=16 rgb_bits=(5,6,5) shifts=(11,5,0)  mip0: 59x60  first 16 u16: 0xF81F × 16
ui\bm\statusright.bm    palFmt=1 numMips=1 colorkey=0x0    is16bit=0 bpp=8  rgb_bits=(0,0,0)  shifts=(0,0,0)   mip0: 61x59
ui\bm\statusright16.bm  palFmt=1 numMips=1 colorkey=0xF81F is16bit=1 bpp=16 rgb_bits=(5,6,5) shifts=(11,5,0)  mip0: 61x59  first 16 u16: 0xF81F × 16
ui\bm\sthealth.bm       palFmt=1 numMips=6 colorkey=0x0    is16bit=0 bpp=8                                    mip0: 18x18
ui\bm\sthealth16.bm     palFmt=1 numMips=6 colorkey=0xF81F is16bit=1 bpp=16 rgb_bits=(5,6,5) shifts=(11,5,0)  mip0: 18x18  first 16 u16: F81F F81F F81F 5000 8800 C000 8800 5000 F81F × 8
ui\sft\armornum.sft     (not BM signature: 'SFNT')
ui\sft\armornum16.sft   (not BM signature: 'SFNT')
ui\sft\helthnum16.sft   (not BM signature: 'SFNT')   etc.
```

`Res1hi.gob` is the active resource pack on this build (we have not verified
which GOB the game actually loads from for these names; `Res2.gob` and
`JediKnight.GOB` are also on disk). The font files start `SFNT` not `BM  `;
their internal BM was not dumped.

## Facts about the code path

These are verified by direct reading of the listed source lines, **not**
runtime traces.

### Preprocessor environment on Xbox

- `build_xbox.bat:49` defines `TARGET_XBOX`, `SDL2_RENDER`, `STDSOUND_NULL`,
  `_XBOX`.
- `src/Platform/Xbox/platform_xbox.h` defines `TARGET_XBOX`, `SDL2_RENDER`,
  `PLATFORM_NOSOCKETS`, `QOL_IMPROVEMENTS`,
  `TARGET_NO_MULTIPLAYER_MENUS`, `LINUX_TMP`, `STDSOUND_NULL`,
  `COG_DYNAMIC_STACKS`.
- `src/engine_config.h:298-335` gates `RDMATERIAL_MINIMIZE_STRUCTS`,
  `STDBITMAP_PARTIAL_LOAD`, `STDHASHTABLE_*`, `COG_DYNAMIC_*`,
  `SITHAI_CRC32_INSTINCTS`, `STDPLATFORM_HEAP_SUGGESTIONS`,
  `RDMATERIAL_LRU_LOAD_UNLOAD`, `JKGUI_SMOL_SCREEN`,
  `RDCLIP_WORK_BUFFERS_IN_STACK_MEM`, `RDCLIP_CLIP_ZFAR_FIRST`,
  `SITHRENDER_SPHERE_TEST_SURFACES`, `RDCACHE_RENDER_NGONS`,
  `STDBITMAP_PARTIAL_LOAD` (again) all behind `#if defined(TARGET_TWL)`.
- Xbox does not define `TARGET_TWL`. **None of those defines are active on
  Xbox.**
- `OPTIMIZE_AWAY_UNUSED_FIELDS` is gated on `EXPERIMENTAL_FIXED_POINT`
  (`engine_config.h:391-396`) — also not defined on Xbox.

So on Xbox: full `rdTexFormat`, full `stdBitmap`, full `stdVBuffer`,
full `stdVBufferTexFmt`. No partial-load.

### Render path the engine takes for HUD

`src/Main/jkHud.c`:

- `jkHud_aBitmaps[8]` (line 43-52) lists the 8 HUD BMs by `(path8bpp,
  path16bpp)`. `jkHud_Open` (line 82-) loads each via `stdBitmap_Load`
  with `_sprintf(tmp, "ui\\bm\\%s", ...)` choosing `path16bpp` because
  `SDL2_RENDER` is defined (line 102 `#ifndef SDL2_RENDER` gates the
  8bpp branch off).
- `jkHud_Draw` (line 309) at line 425-428 unconditionally tail-calls
  `jkHud_DrawGPU()` when `SDL2_RENDER` is defined.
- `jkHud_DrawGPU` at line 1041-1042 draws the panels:
  ```c
  std3D_DrawUIBitmap(jkHud_pStatusLeftBm,  0, jkHud_leftBlitX,  jkHud_leftBlitY,  NULL, jkPlayer_hudScale, 1);
  std3D_DrawUIBitmap(jkHud_pStatusRightBm, 0, jkHud_rightBlitX, jkHud_rightBlitY, NULL, jkPlayer_hudScale, 1);
  ```
- Same function at line 1066, 1079, 1103, 1115, 1145, 1176 draws the
  state-indicator icons via `std3D_DrawUIBitmap(<bm>, <state-index>,
  ...)`.
- Same function at line 1137 (shield), 1168 (health), and the
  corresponding ammo block draw the digits via
  `stdFont_DrawAsciiGPU(font, x, y, 999, tmp, 0, jkPlayer_hudScale)`.
- `jkPlayer_hudScale` is set to `1.0f` for Xbox in
  `src/Platform/Xbox/xbox_stubs.c:117`.

`src/General/stdFont.c:1265, 1558, 1798`: `stdFont_DrawAsciiGPU`
iterates each character and calls
`std3D_DrawUIBitmap(font->pBitmap, 0, ..., &a5a, scale, alpha)` with
`a5a` being a glyph sub-rect of the font sheet. **All HUD digit
rendering goes through the same `std3D_DrawUIBitmap` entry as the
status panels.**

### Bitmap load chain

`src/General/stdBitmap.c`:

- `stdBitmap_Load` → `stdBitmap_LoadCommon` → `stdBitmap_LoadEntry` →
  `stdBitmap_LoadEntryFromFile`.
- Line 152 reads the 70-byte `bitmapHeader` from the file.
- Line 169 allocs the `mipSurfaces` pointer array.
- Lines 191-231: per-mip loop:
  - Reads `(width, height)` (8 bytes) per mip from the file (line 193).
  - `vbufTexFmt.format = out->format` (the BM-header `rdTexFormat`).
  - `stdDisplay_VBufferNew` allocs the surface (line 205).
  - `stdDisplay_VBufferLock(surface)` (line 211).
  - File-read directly into `surface->surface_lock_alloc` row by row
    (lines 214-219).
  - `stdDisplay_VBufferUnlock(surface)` (line 220).
  - **Line 221:** `if ((out->palFmt & 1) != 0) stdDisplay_VBufferSetColorKey(surface, out->colorkey);`
- Lines 246-258 (under `#ifdef SDL2_RENDER`): allocate
  `aTextureIds[numMips]`, `abLoadedToGPU[numMips]`,
  `paDataDepthConverted[numMips]`, then call
  `std3D_AddBitmapToTextureCache(out, i, !(out->palFmt & 1), 0)`
  **once per mip** at load time.

`src/Platform/Xbox/xbox_autostubs.c`:

- `stdDisplay_VBufferLock` (line 47) sets `bSurfaceLocked=1`.
- `stdDisplay_VBufferUnlock` (line 173) sets `bSurfaceLocked=0`.
  **Does not null out** `surface_lock_alloc` (unlike the SDL2 impl).
- `stdDisplay_VBufferSetColorKey` (line 48):
  `vbuf->transparent_color = (unsigned int)key`.
- `stdDisplay_VBufferNew` (line 55-77):
  - `bpp_bytes = fmt->format.is16bit ? 2 : 1;`
  - `pixel_sz  = w * h * bpp_bytes;`
  - `vbuf = malloc(sizeof(struct stdVBuffer) + pixel_sz);`
  - `memset(vbuf, 0, sizeof(struct stdVBuffer));`
  - `vbuf->format = *fmt;`
  - `vbuf->format.texture_size_in_bytes = pixel_sz;`
  - `vbuf->format.width_in_bytes  = w * bpp_bytes;`
  - `vbuf->format.width_in_pixels = w;`
  - `vbuf->surface_lock_alloc = (char*)(vbuf + 1);`

Therefore, after `stdBitmap_LoadEntryFromFile` returns:

- For every BM with `palFmt & 1 != 0` (which includes all 6 dumped
  ones), `vbuf->transparent_color == colorkey` from the BM header
  (`0xF81F` for the 16bpp variants, `0` for the 8bpp variants).
- `vbuf->surface_lock_alloc` points to the bytes immediately after the
  `stdVBuffer` struct, containing the raw file pixel data.

### Texture-upload path

`src/Platform/Xbox/std3D.c`:

- `std3D_AddBitmapToTextureCache(pBmp, mipIdx, is_alpha_tex, no_mip)`
  forwards to `xbox_upload_bitmap_mip(pBmp, mipIdx, is_alpha_tex)`.
- `xbox_upload_bitmap_mip`:
  - Bails if `bm`, `mipSurfaces`, `aTextureIds`, `abLoadedToGPU`,
    `mipSurfaces[mipIdx]`, or `mipSurfaces[mipIdx]->surface_lock_alloc`
    is NULL.
  - Bails if `mipIdx >= numMips` or already loaded.
  - Computes `w, h` from `vbuf->format.width_in_pixels` and
    `vbuf->format.height`.
  - **(Current uncommitted change)** Pads to next pow2:
    `padW = xbox_next_pow2(w); padH = xbox_next_pow2(h);`
    `g_texScratch[0..padW*padH*4] = 0;`
  - For `fmt_is16bit`: per-pixel RGB565/RGB1555 → RGBA8 conversion;
    pixel matching `transparent_color` per-channel gets `a=0`. Writes
    `(y*padW + x)*4` into `g_texScratch`. Forces `is_alpha_tex = 1`.
  - Else (8-bit): looks up palette via `vbuf->palette`, falling back to
    `bm->palette`, `g_pCurrentPalette`, `xbox_get_world_palette()`.
    Writes RGBA into `g_texScratch[(y*padW + x)*4]`. `idx == 0` gets
    fully transparent (unconditional, mirrors upstream
    `_ORIGINAL_READ_ONLY/.../Platform/GL/std3D.c:3294-3296`).
  - `id = g_nextTexId++`; `g_pfnBindTexture(GL_TEXTURE_2D, id)`.
  - `glTexParameterf` calls: `MIN_FILTER=NEAREST`, `MAG_FILTER=NEAREST`,
    `WRAP_S=CLAMP`, `WRAP_T=CLAMP`. (No `BASE_LEVEL`/`MAX_LEVEL` — see
    §"Recent attempts" for why.)
  - `glTexImage2D(GL_TEXTURE_2D, 0, /*internalformat*/4, padW, padH, 0,
    GL_RGBA, GL_UNSIGNED_BYTE, g_texScratch);`
  - `bm->aTextureIds[mipIdx] = id; bm->abLoadedToGPU[mipIdx] = 1;`

### UI draw path

`std3D_DrawUIBitmapRGBA` (same file):

- Reads `texW = vbuf->format.width_in_pixels`, `texH =
  vbuf->format.height`.
- **(Current uncommitted change)** Computes
  `padTexW = xbox_next_pow2(texW); padTexH = xbox_next_pow2(texH);` and
  divides UVs by `padTexW`/`padTexH` instead of `texW`/`texH`. Same
  `xbox_next_pow2` function as the upload, so the upload and the draw
  *should* agree on padding.
- `xbox_set_ui_state(need_blend)` sets ortho 0..640, 0..480 (NDC y
  inverted via `glOrtho(0, 640, 480, 0, ...)`); disables depth/cull/
  alpha test; enables blend with `(SRC_ALPHA, ONE_MINUS_SRC_ALPHA)`
  when `need_blend` (forced on for `fmt_is16bit`).
- Submits the two triangles via **two separate** `glBegin(GL_TRIANGLES)`
  blocks, three vertices each (work-around for the FakeGL
  `drawMode+1` quirk at `fakeglx.cpp:1416` per the comment at line
  1865-1877 of `std3D.c`).
- Calls `glColor4f` once **per vertex** inside the `glBegin` block (also
  per that comment, FakeGL inline-mode requires per-vertex color
  writes — color set outside `glBegin` leaves DIFFUSE at fuchsia).

### FakeGL upload internals

`src/Platform/Xbox/fakeglx.cpp` (third-party graft — **do not modify**;
see `RENDERER_GRAFT.md`):

- `glTexImage2D(target=GL_TEXTURE_2D, level=0, internalformat=4,
  width, height, border=0, format=GL_RGBA, type=GL_UNSIGNED_BYTE,
  pixels)` (lines 2245-2364):
  - `GLToDXPixelFormat(4, GL_RGBA)` returns `D3DFMT_A8R8G8B8` (line
    2760) unless `g_force16bitTextures`.
  - `ConvertToCompatablePixels` (case `D3DFMT_A8R8G8B8` at line
    3086-3137) converts our `R,G,B,A` scratch to `B,G,R,A` in a
    `StickyAlloc`'d buffer with pitch `4*width`.
  - `CreateTexture(width, height, levels=1, 0, D3DFMT_A8R8G8B8,
    D3DPOOL_MANAGED, ...)` (line 2347).
  - `glTexSubImage2D_Imp` (line 2445) → `XGSwizzleRect(compatablePixels,
    pitch, &rect, lockRect.pBits, desc.Width, desc.Height, &point,
    lockRect.Pitch / desc.Width)` (line 2459).
- `glTexParameterf` (line 2367-2399) recognises only `MIN_FILTER`,
  `MAG_FILTER`, `WRAP_S`, `WRAP_T`, and `D3D_TEXTURE_MAXANISOTROPY`.
  Anything else hits `default: LocalDebugBreak()` (no-op in release per
  `fakeglx.cpp:80-86`) **and** still calls `SetRenderStateDirty()` +
  `DirtyTexture(currentID)` at the top of the case.
- `glEnd` (line 3221-3224) is **a literal `return`** — does not call
  the FakeGL `glEnd` method.
- `glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA)` maps to
  `D3DBLEND_SRCALPHA` / `D3DBLEND_INVSRCALPHA` (lines 306-307).

## Recent attempts that did not change the symptom

Each of these was flashed and visually verified on hardware. Each
produced **no visible change** vs. the prior frame.

1. Switched UI-texture filter from `GL_LINEAR`/`GL_REPEAT` to
   `GL_NEAREST`/`GL_CLAMP`. (Originally a blur fix from
   `XBOX_HACKS.md`.)
2. Added `glTexParameterf(..., GL_TEXTURE_BASE_LEVEL, 0.0f)` and
   `..., GL_TEXTURE_MAX_LEVEL, 0.0f`. Reverted after reading
   `fakeglx.cpp:2367-2399` — those pnames hit `default:`/
   `LocalDebugBreak()` (no-op release) and dirty the render state.
3. Made `idx == 0` unconditionally transparent in the 8-bit upload
   path (mirrors upstream `:3294-3296`).
4. Mutated `stdVBuffer` / `stdBitmap` mirror layouts in `std3D.c`
   thinking `RDMATERIAL_MINIMIZE_STRUCTS` / `STDBITMAP_PARTIAL_LOAD`
   were active — reverted after re-reading `engine_config.h:298-335`
   and confirming they are `TARGET_TWL`-only.
5. Added a `tk == 0` fallback to `0xF81F`/`0x7C1F` in the 16-bit
   conversion — reverted after dumping the BM headers and confirming
   `transparent_color` is already `0xF81F` for every 16bpp HUD BM
   (verified end-to-end through `palFmt & 1 → VBufferSetColorKey →
   vbuf->transparent_color`).
6. **(Still in tree)** Pow2 padding of UI texture uploads. Hypothesis
   was that `XGSwizzleRect` requires pow2 destination dimensions and
   the HUD BMs at 18×18/59×60/61×59 produced scrambled swizzled
   layout. Symptom did not change after this fix shipped.

## What is instrumented and where the log lives

`src/Platform/Xbox/std3D.c` already emits `XDBGF` lines at each
decision point inside the UI bitmap path. They go to
`D:\debug_openjkdf2.txt` on the Xbox HDD (via `xbox_debug.h`).

The instrumentation tags, in order they fire for a single
`std3D_DrawUIBitmap` call:

- `UI gate: bm=... init=... mipSurf=... aLoaded=... aTex=... numMips=... mip=... palFmt=...`
- `UI ret#A:` if init/bmp/mipSurf/abLoadedToGPU is null
- `UI ret#B:` if mip out of range
- `UI ret#C:` if mip vbuf is null
- `UI lazy upload: bm=... mip=... palFmt=... alpha=...`
- (then xbox_upload_bitmap_mip fires its own `xup#A..D` / `xup 16bit:`
  / `UI tex upload:` lines)
- `UI ret#D:` if upload returned 0
- `UI ret#E:` if texId or bind fn is null
- `UI ret#F:` if texW/H is zero
- `UI proceed: bm=... texId=... w=... h=... pad=...x...`
- `UI draw: bm=... mip=... id=... dst=(x,y) sz=(w,h) tint=...`

**This log has not been pulled from the device for the current build.**
Reading it would tell you precisely which branch each of the
"renders" vs "doesn't render" bitmaps takes, including the actual
`w/h/padW/padH/palFmt/tkey/texId` values seen at runtime.

## What I have not been able to verify

These are the questions a runtime log (or a hardware-side probe) would
answer immediately and that source-reading cannot:

1. For each of `jkHud_pStatusLeftBm` / `pStatusRightBm` / the font's
   `pBitmap`: does `std3D_DrawUIBitmapRGBA` reach the `UI draw:` line
   or short-circuit at one of `UI ret#A..F`?
2. If it does reach `UI draw:`, what is the `texId` and what `(w, h,
   padW, padH)` does it pass to `glTexImage2D`?
3. Does `vbuf->surface_lock_alloc` at upload time still equal what
   `stdDisplay_VBufferNew` set (i.e., `vbuf + 1`), or has something
   freed/moved it?
4. Why do state-indicator icons (which draw with `mipIdx > 0` on the
   same `std3D_DrawUIBitmap` entry as the panel) render correctly,
   while the panel (`mipIdx == 0`) does not? Both go through the same
   functions with the same code path.
5. Why does the inventory bitmap render at all (even pixelated) when
   panels and digits don't? Are inventory items rendered via a
   different code path?

## Items that have been ruled out

- The BM files on disk are correct (header dump matches upstream PC
  expectations).
- `stdBitmap` load on Xbox correctly populates `surface_lock_alloc`
  (verified by reading `xbox_autostubs.c::stdDisplay_VBufferNew` and
  the `stdDisplay_VBufferUnlock` impl that does not clear it).
- `vbuf->transparent_color` is `0xF81F` post-load for every 16bpp HUD
  BM (verified through `palFmt & 1 → VBufferSetColorKey` chain).
- `RDMATERIAL_MINIMIZE_STRUCTS`/`STDBITMAP_PARTIAL_LOAD` mismatch is
  not the cause (gated on `TARGET_TWL`).
- 16-bit conversion math is correct given `tk=0xF81F` and RGB565 input.
- FakeGL `internalformat=4 + GL_RGBA` → `D3DFMT_A8R8G8B8` with B,G,R,A
  byte order — matches the R,G,B,A scratch we hand it.
- `GL_SRC_ALPHA`/`GL_ONE_MINUS_SRC_ALPHA` map to the correct D3DBLEND
  enums.
- `GL_TEXTURE_BASE_LEVEL`/`MAX_LEVEL` are silently dropped by FakeGL
  in release; not the cause (we removed them anyway).
- Bolt vector bug (separate handoff,
  `HANDOFF_BOLT_RENDER.md`) — fixed by Codex in
  `src/Raster/rdCache.c:1185`, unrelated to HUD.

## Constraints

1. `src/Platform/Xbox/fakeglx.cpp` and `src/Platform/Xbox/gl/gl.h` are
   the third-party FakeGL graft. **Do not modify.** See
   `RENDERER_GRAFT.md`.
2. **Xbox-only build.** This fork targets the original Xbox exclusively;
   other platforms (Linux SDL2, macOS, Web) are not in scope and do not
   need to keep building. Engine-side fixes are fine if they're the
   right fix — no need to gate them on `TARGET_XBOX` for portability.
3. C89-compatible. Build is XDK VC71 (`cl.exe` 13.x from XDK 5558).
4. No symptomatic patches. Find the actual cause.

## Where to start

Pull `D:\debug_openjkdf2.txt` after a fresh boot that reaches the
in-game cutscene/Kyle-in-bar state. Look at the `UI gate:` /
`UI ret#…:` / `UI lazy upload:` / `xup#…:` / `xup 16bit:` /
`UI tex upload:` / `UI proceed:` / `UI draw:` lines for the first
~12 UI draws. The pattern of which of those lines fire for each BM
(panel vs. icon vs. font glyph) will pin down which branch each
"not rendering" bitmap takes, which is the gating fact for every
subsequent decision.
