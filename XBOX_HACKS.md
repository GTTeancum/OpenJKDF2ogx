# Xbox Port — Known Hacks, Bugs & Tech Debt

Running log of intentional shortcuts and known-broken behaviors on the Xbox
port. Each entry has enough context to find the code and understand *why*
something is wrong, so we can fix it later instead of rediscovering a pile
of mystery problems.

Format: one section per item. Newest at the top within each part. Mark
`[FIXED]` (with the fixing commit) once it's gone instead of deleting — the
history is useful.

---

# Known Bugs

## Cutscene volume needs a separate mixer setting

**Status:** Open.
**Severity:** Audio balance. SMK cutscenes now play in sync on Xbox, but
their decoded PCM is roughly 30% quieter than in-game sound effects and
music when routed through the Xbox streaming buffer.

**Desired fix:** Add a separate cutscene volume setting or scalar instead
of baking compensation into the stream code. Cutscenes should be tunable
independently from normal in-game SFX so intro/dialogue playback can be
balanced without making weapons, ambience, or menu sounds too loud.

**Where to look:** `src/Main/jkCutscene.c` where `jkGuiSound_cutsceneVolume`
is passed into `stdSound_XboxStreamOpen`, and
`src/Platform/Xbox/stdSound_xbox.c` where the streaming buffer applies
the DirectSound volume.

## HUD / BM overlay bitmaps render blurry — [FIXED]

**Status:** FIXED — `src/Platform/Xbox/std3D.c:xbox_upload_bitmap_mip` now
sets `GL_NEAREST` filter and `GL_CLAMP` wrap for UI textures, pins them
to mip 0 via `GL_TEXTURE_BASE_LEVEL`/`GL_TEXTURE_MAX_LEVEL`, and makes
palette index 0 unconditionally transparent in the 8-bit path (matches
upstream `Platform/GL/std3D.c:3106-3115,3294-3296`). Combined with
removing the per-frame mip-level mismatch warning from Cxbx, HUD bitmaps
now render crisp and at correct alpha.

(original entry preserved below for history)

**Status (original):** Open.
**Severity:** Cosmetic but pervasive. Every BM overlay (HUD status panels,
ammo counters, force bars, menu graphics, mouse cursor, anything routed
through `stdBitmap`) renders with bilinear-blurred pixels instead of the
pixel-perfect look the artwork was authored for. Combined with the magenta
colorkey alpha test, this also produces faint magenta halos at the edges
of transparent regions.

**Symptom:** HUD bitmaps look soft / fuzzy / smeared rather than crisp.
World textures look fine (linear filtering is correct for those at the JK1
texel density). The colorkey itself is hard-clamped (exact equality, no
fade — confirmed at `std3D.c:1318` / `:1338`), so this isn't a colorkey
bug — it's a texture-filter bug.

**Root cause:** `src/Platform/Xbox/std3D.c:1412-1413` sets
`GL_TEXTURE_MIN_FILTER` / `GL_TEXTURE_MAG_FILTER` to `GL_LINEAR` for
**every** texture uploaded, including HUD bitmaps. JK1 HUD art is
pixel-perfect 1:1 at 640×480 and needs `GL_NEAREST` to render sharp.

**Why upstream is fine:** The PC SDL2 build (`_ORIGINAL_READ_ONLY/.../Platform/GL/std3D.c`)
uses `GL_LINEAR` for game-world texture uploads but `GL_NEAREST` for the
HUD/menu/palette textures (different upload paths). It also uses GLSL
shaders that can pick the filter per-sampler. Our Xbox fixed-function
pipeline applies one filter mode globally at upload, so the world+HUD
distinction is lost.

**Fix options (when revisited):**
1. **Cheapest:** Switch the texture upload to `GL_NEAREST` globally —
   matches the authentic 1997 JK1 look (no bilinear back then), HUD
   instantly sharp. World textures pick up more visible pixelation but
   that's the era-accurate look. ---NOT AN OPTION (Steve)
2. **Right:** Set the filter mode per-bind based on the active
   geometry/render context — `GL_NEAREST` when rendering in ortho (HUD)
   or for any material flagged as UI; `GL_LINEAR` for perspective world
   geometry. Costs an extra `glTexParameter*` call per filter change
   (negligible).
3. **Best:** Tag each cached texture with a filter mode at upload time
   based on the engine's `rdGeoMode_t` / surface flags / material kind.
   `std3D_AddTextureToCache` already knows the format and stage — extend
   the cache entry to remember `GL_NEAREST` vs `GL_LINEAR` and re-apply
   on each bind.

**Where:** `src/Platform/Xbox/std3D.c:1412-1413` (upload-time filter setup),
plus per-bind filter selection inside `std3D_DrawRenderList` at the
texture-bind site (`std3D.c:1076`).

## Translucent windows render opaque

**Status:** Open.
**Severity:** Cosmetic. Doesn't block gameplay but breaks visual fidelity
on every level with glass/window surfaces (most JK levels — bartop windows
in narshadda, observation windows, etc.).

**Symptom:** Surfaces flagged as semi-transparent windows render as fully
opaque textured walls. The scene behind them is not visible.

**Root cause (suspected):** JK1 marks these surfaces with a translucency
flag on the surface/material; the upstream renderer reads the flag and
emits alpha-blended geometry. Our Xbox renderer path (`std3D_DrawRenderList`
or the inline GL adapter in `fakeglx.cpp`) is likely either not honoring
the flag, not enabling `GL_BLEND`, or routing the surface through the
solid-color/textured path instead of the translucent path.

**Where to look:**
- `src/Engine/sithRender.c` — sector/surface render loop; find the
  translucent-surface flag check and the path it takes.
- `src/Raster/rdCache.c` — translucent-vs-opaque batching.
- `src/Platform/Xbox/std3D.c` — DrawRenderList and per-vertex/per-face
  blend-state setup.
- Material flag on the bartop window's `.mat` for a concrete repro.

**Defer until:** after the blaster-bolt mesh orientation bug is solved.

---

# Hacks & Tech Debt

## sithGamesave_Load hijacked into a soft respawn — REVERTED

**Status:** REVERTED. Engine froze after one frame post-`Present` on boot. Cause
not yet identified — could be a C++/C linkage mismatch on
`sithPlayer_pLocalPlayerThing` (declared `struct sithThing*` in xbox_stubs.c
but defined in sithPlayer.c which is compiled as C++), a stale link artifact
from removing the auto-stub, or unrelated. Reverted both halves of the edit
(restored auto-stub returning 0, removed the soft-respawn impl) until we can
diagnose. Respawn-after-death remains broken — player must restart the game
after dying.

**Next attempt should:** add the soft-respawn body *inside* the existing
`xbox_autostubs.c` definition (replace the `return 0` line in place) rather
than moving the definition between TUs. Avoids any linker layout shift.
Forward-decl `sithThing*` with the real typedef by `#include "types.h"` or
similar so the C++ mangling matches sithPlayer.c's definition exactly.

**Where (when retried):** `src/Platform/Xbox/xbox_stubs.c` — `sithGamesave_Load`
**Added:** Phase 4, respawn-after-death fix
**Severity:** Medium. Functional for the only caller we currently hit, but
semantically wrong and will mislead any future caller.

**What we did:** `sithGamesave_Load` ignores its `saveFname` argument and
unconditionally calls `sithPlayer_debug_ToNextCheckpoint(sithPlayer_pLocalPlayerThing)`,
then returns 1.

**Why:** The death/respawn flow goes
`sithPlayer_sub_4C9150` → (3s + fire) → `sithPlayer_debug_loadauto` →
`sithGamesave_Load(autosave_fname, 0, 0)`. The original stub returned 0,
which made the fallback (`_JKAUTO_<jkl>.jks`) run, which also returned 0,
which left the player frozen on the death screen with no way out except
rebooting the Xbox. Hijacking this single entry point into a debug
checkpoint reset gets the player back into the game with health restored
and `SITH_TF_DEAD` cleared.

**Why it's wrong:**
1. Any *non-death* caller of `sithGamesave_Load` (real quickload, menu
   "Load Game") will also get a checkpoint reset instead of their actual
   save. We don't hit those today because we have no save UI, but the
   moment one is wired up this hack will silently eat the load.
2. The `_JKAUTO_*.jks` fallback in `sithPlayer_debug_loadauto` is
   suppressed because we return 1. Once a real save system exists, the
   fallback won't run either.
3. `sithGamesave_Write` is still the auto-stub that returns 0, so we
   never write the autosave the loader is "loading."

**Proper fix:** Implement Xbox save/load via `XCreateSaveGame` /
hostfs UDATA partition writes. Route `sithGamesave_Load` and
`sithGamesave_Write` through it. Then delete the hijack and let
`sithPlayer_debug_loadauto` run its normal path. The debug-checkpoint
behavior can still be exposed via a separate dev key if useful.
