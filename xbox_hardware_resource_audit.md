# OpenJKDF2 Xbox Hardware Resource Audit

This port should be treated like a retail Xbox title, not a desktop app that
happens to boot as an XBE. CXBX-R can hide resource problems that real hardware
will expose as load-end freezes, audio stalls, missing assets, or full-screen
video failures.

The core rule from the retail-source references still applies here: every
resource needs a named lifetime, and every transition must return the process to
a known-good state. For OpenJKDF2, the relevant lifetimes are boot, frontend,
cutscene, game launch, level, multiplayer/session, mod, and shutdown.

## Retail Patterns Worth Copying

### XQuake

XQuake is still the closest structural reference: old PC engine, constrained
console memory, and an Xbox renderer sitting behind a compatibility layer. Its
useful pattern is not an exact allocator API, but a hard boundary between
persistent startup allocations, level/session allocations, and disposable cache
allocations.

OpenJKDF2 already follows part of this spirit by using XQuake-style FakeGL on
Xbox. The missing piece is an equivalent resource-phase model for files, GOBs,
materials, UI bitmaps, sound buffers, music streams, and cutscene/video objects.

### Mercenaries / UC / Re-Volt

The retail projects are consistent about ownership:

- assets belong to managers or resource groups;
- large transient work is staged, committed, or discarded explicitly;
- hot paths avoid heap allocation, disk probes, and logging;
- frontend resources are prepared once and released as a group;
- hardware-only resource statistics are phase snapshots, not frame spam.

OpenJKDF2 should copy that discipline rather than any one API.

## Current OpenJKDF2 Resource Domains

### Memory Ownership

Current behavior:

- Xbox host services still route many allocations through normal `malloc`,
  `free`, and `realloc`.
- Some fixed pools already exist: `MAX_OPEN_FILES` in `stdFile_xbox.c`,
  `MAX_DS_BUFFERS` in `stdSound_xbox.c`, and fixed renderer scratch buffers in
  `std3D.c`.
- There is no single Xbox allocation ledger that can answer "which phase owns
  this memory?"

Risks:

- failed level loads can leave partially owned allocations behind;
- repeated frontend -> cutscene -> level -> frontend loops can fragment memory;
- CXBX-R memory numbers do not map directly to a 64 MB retail Xbox;
- out-of-memory symptoms can be caused by leaks, fragmentation, DirectSound
  heap pressure, texture pressure, or file/video staging buffers.

Target:

- add an Xbox resource-phase coordinator with owners for `boot`, `frontend`,
  `cutscene`, `game-launch`, `level`, `mod`, `sound`, `video`, and `scratch`;
- add lightweight allocation counters and high-water marks for Xbox builds;
- log one memory/resource snapshot at phase boundaries and failed-load exits;
- require any large temporary allocation to name its owner and expected
  lifetime.

### Filesystem And GOB State

Current behavior:

- `stdFile_xbox.c` uses a fixed file pool, which is good console behavior.
- `stdGob_xbox.c` reads GOB entries from disk and does not intentionally map
  entire GOBs into RAM.
- Xbox path probing, GOB loading, file-exists checks, and find operations still
  have diagnostic logging paths that can become noisy.

Risks:

- mod/base/resource search order can outlive the phase that introduced it;
- repeated file probes during menus or loading can stall if they also log;
- GOB table allocations are not visibly attached to a mount group;
- stale mod state can contaminate stock SP or MP level loads.

Target:

- define mount groups: `base-resource`, `frontend`, `active-mod`,
  `active-level`, and `cutscene`;
- make `resetForFrontend()` and `resetForGameLaunch()` leave no stale mod GOBs,
  loose search paths, or file mappings;
- keep the fixed file pool, but add open-file high-water logging;
- gate `util_FileExists`, `stdFileUtil_NewFind`, and GOB-entry diagnostics
  behind explicit verbose builds.

### Level Loading And Failure Cleanup

Current behavior:

- Sith world, COG, materials, sounds, models, AI, and UI state are initialized
  through the inherited desktop flow.
- Friendly "could not load level" handling exists, but hardware has also shown
  freezes after the loading bar reaches completion.

Risks:

- a load can be far enough along to allocate GPU textures, sound buffers, COG
  state, and file handles before failing;
- load failure and load success likely do not unwind through identical cleanup
  paths;
- MP and SP loads exercise different late-load systems.

Target:

- make level load two-phase: stage required assets, then commit active level;
- on failure, destroy the staging phase and return to a clean frontend phase;
- log resource snapshots at `level-load-begin`, `level-load-commit`,
  `level-load-fail`, and `level-unload-done`;
- include free memory, host allocation total/peak, file handle count, GOB mount
  count, material count, bitmap count, FakeGL texture count, DirectSound buffer
  count, and current level/mod name.

### Materials, Bitmaps, And GPU Textures

Current behavior:

- Xbox rendering goes through `src/Platform/Xbox/std3D.c` and the FakeGL bridge.
- Texture capacity is capped by `STD3D_MAX_TEXTURES`.
- `rdMaterial`, `stdBitmap`, UI bitmap refs, menu/cutscene textures, and wheel
  icons all feed texture pressure.
- There are purge hooks such as material cache purging, bitmap ref purging,
  menu texture release, and cutscene texture release, but ownership is spread
  across several systems.

Risks:

- UI, cutscene, wheel, and level textures can compete for the same fixed texture
  namespace;
- material/bitmap load and free logs can become transition-time stalls;
- full-frame or tiled uploads during menus/cutscenes are expensive when they
  happen outside a controlled phase;
- 16-bit MAT and alpha/translucency paths need explicit Xbox coverage because
  mod maps can expose formats the stock campaign does not stress.

Target:

- track texture owners: `world`, `model`, `sprite`, `frontend-ui`,
  `gameplay-ui`, `cutscene`, and `scratch`;
- report texture count and approximate bytes in phase snapshots;
- purge `frontend-ui` before entering gameplay unless the menu is still needed;
- purge `cutscene` immediately after XMV/SMK playback;
- keep weapon/force wheel assets in the gameplay UI owner and release them on
  return to frontend or world shutdown;
- gate `rdMaterial_FreeEntry`, `stdBitmap_EnsureData`, and
  `stdBitmap_UnloadData` logs outside diagnostic builds.

### Frontend, Menus, And UI Assets

Current behavior:

- GUI screens load backgrounds, fonts, saber previews, button prompts, virtual
  keyboard assets, and weapon/force wheel images through the normal bitmap and
  render paths.
- Controller focus work has produced useful diagnostics, but menu navigation is
  now sensitive to flicker and repeated redraw costs.

Risks:

- menu focus changes can cause repeated bitmap ensures, redraws, or logs;
- UI prompts and wheel images can stay resident into gameplay unless grouped;
- frontend rendering can consume texture slots and CPU time needed by gameplay.

Target:

- add a frontend UI cache that loads menu backgrounds, fonts, Duke button
  prompts, and virtual keyboard art once per frontend phase;
- make menu selection changes dirty only the changed UI regions where practical;
- ensure `New Player`, character edit, force rank, and virtual keyboard screens
  share the same input/focus resource path;
- unload frontend-only UI textures before level load, while preserving
  gameplay UI assets only when entering gameplay.

### FMV And Cutscenes

Current behavior:

- XMV playback is now the correct hardware path and is smooth on Xbox.
- SMK/libsmacker still exists and remains useful for compatibility or desktop,
  but hardware testing showed black video, thin horizontal slices, and stuttery
  audio from that path.
- Cutscene diagnostics and tiled upload experiments were valuable, but should
  not remain part of normal hardware builds.

Risks:

- SMK software decode, tiled texture upload, audio sync, subtitles, and skip
  input can fight for CPU and memory during the same phase;
- leaving cutscenes can leak video textures, streams, or audio state into the
  next menu/level;
- cutscene skip needs to clean up exactly like natural playback completion.

Target:

- make XMV the preferred Xbox FMV path;
- treat SMK on Xbox as fallback/diagnostic only unless a specific compatibility
  mode is requested;
- create a dedicated `cutscene` phase that owns video textures, video buffers,
  subtitle overlays, input skip state, and audio stream state;
- guarantee both skip and end-of-file call the same cleanup;
- remove or compile-gate kitchen-sink FMV diagnostic logs after the current
  investigation is closed.

### Audio, Music, And DirectSound

Current behavior:

- `stdSound_xbox.c` uses DirectSound buffers with a fixed table.
- `stdMci_xbox.c` maps CD music to OGG files via `stb_vorbis`, with a streaming
  open path and a whole-file fallback.
- Pause/resume music is now the desired behavior for ESC/menu pause, not stop
  and restart.

Risks:

- whole-file OGG fallback can consume too much RAM on hardware;
- DirectSound create/release churn can fragment memory or fail late in loading;
- audio-path logs, lock spans, disk reads, and decode spikes can cause stutter;
- pause-menu transitions can accidentally leave music stopped or restart the
  wrong track.

Target:

- disallow or tightly budget whole-file music fallback on retail hardware;
- expose DirectSound buffer count, total bytes, failures, and high-water marks;
- make pause/resume music a single idempotent state transition;
- forbid logging, heap allocation, and disk probes from audio update/mix paths
  in non-diagnostic builds;
- consider predecoded ADPCM/XMA-style music assets if OGG decode cost remains a
  measured startup or streaming spike.

### Logging, Saves, And Settings

Current behavior:

- Xbox logs can write to `D:\debug_openjkdf2.txt` and related debug files.
- Non-smoke builds flush every log write for crash survival.
- `XBOX_PERF_SMOKE` filters logging more aggressively.
- Saves and settings are still closer to the desktop model than a final Xbox
  title model.

Risks:

- flush-every-write is useful for crash diagnosis but can absolutely create
  stutter during menu, load, audio, and gameplay transitions;
- hot-path log formatting can cost even when the output is later filtered;
- save writes can stall or leave partial files if not atomic;
- mutable desktop settings can enable unsafe Xbox combinations.

Target:

- use ring-buffered logs with explicit flush on fatal, phase transition, and
  user-triggered dump;
- make hardware performance builds log phase snapshots, warnings, and fatal
  lines only;
- keep verbose logs behind `XBOX_VERBOSE_DEBUG` or narrower subsystem flags;
- write saves atomically: temp file, flush, rename;
- keep Xbox-critical settings in a constrained profile and ignore unsupported
  desktop-only keys.

## Transition Audit Pass - 2026-06-04

Patched:

- Multiplayer map load now follows the same late world-init sequence as normal
  level loads: `sithMain_Mode1Init_3()` calls `sithWorld_Initialize()` after
  `sithWorld_Load()` and before `sithMain_Open()`. This targets the hardware
  freeze seen after the MP loading bar completes.
- Failed current-world loads now clear `sithWorld_pCurrentWorld` after
  `sithWorld_Load()` rejects the map. `sithWorld_Load()` already frees the
  failed world object, so the important cleanup is removing the stale global
  pointer.
- Failed static-world loads now clear `sithWorld_pStatic`, and
  `sithMain_Load()` checks allocation before writing `level_type_maybe`.
- Gameplay show failure for multiplayer now shuts down the local MP session
  immediately before returning to the frontend error path.
- `jkGui_SetModeMenu()` now rolls back `jkGui_modesets` when display-open or
  mode-set failure prevents the frontend mode from actually being entered.
- Cutscene close is now idempotent across Xbox/SDL/TWL paths, and natural
  SMK/Smush playback completion defers cleanup to `jkMain_VideoLeave()` instead
  of doing a partial tick-time cleanup. Skip and EOF now converge on the same
  close owner.
- `jkCutscene_CleanReset()` no longer closes the Xbox PCM movie stream twice
  during reset.
- Gameplay and escape-menu multiplayer exits now clear the DirectPlay session
  active flag before `sithMulti_Shutdown()` clears `sithNet_isServer`.
- Xbox MP load breadcrumbs now cover host creation, episode load, world load,
  world init, Sith open, multiplayer startup, player setup, and video-mode
  handoff.
- Xbox transition resource breadcrumbs now log phase snapshots at gameplay
  show/leave, escape-menu leave, MP world load, load failure cleanup,
  video-mode handoff, episode next-level scheduling, and the generic
  `jkMain_GuiAdvance()` leave/show transition boundary. `ResourceTrace`
  includes free memory, current/next GUI state, gameplay mode, frontend mode
  count, multiplayer/split-screen state, level name, and live world counters.
- Active world allocation is now checked before `sithWorld_Load()` in
  single-player, normal, and multiplayer map-load paths. Under Xbox memory
  pressure, allocation failure now returns to the caller instead of passing a
  null world into the loader.
- Xbox/SDL gameplay video-mode setup now rolls back partial HUD, canvas,
  control, and window-handler setup if canvas allocation or display mode setup
  fails. Freed gameplay canvases are nulled on `Video_SwitchToGDI()` teardown.

Audited transition pairs:

- Menu-to-menu transitions run through `jkMain_GuiAdvance()` and each state's
  `leave`/`show` pair. Most menu-specific resources are still owned by the
  individual GUI screens, not by a shared frontend phase.
- Menu-to-level and level-to-level transitions converge on
  `jkMain_GameplayShow()` and `jkMain_GameplayLeave()`. Successful exits close
  player/game/world state through `jkPlayer_Shutdown()` and `sithMain_Close()`.
- Level-to-pause and pause-to-level transitions use `jkMain_EscapeMenuShow()`
  and `jkMain_EscapeMenuLeave()`. Non-multiplayer pause resumes time/music,
  while multiplayer keeps ticking during the menu.
- Level-to-cutscene and cutscene-to-level routes go through `jkMain_VideoShow()`,
  `jkMain_VideoTick()`, and `jkMain_VideoLeave()`, with cutscene texture release
  also present inside the cutscene code.
- Title/static-world setup uses `sithMain_Load("static.jkl")` and
  `sithMain_Free()`. The static-world failure path now avoids a dangling global.

Still open:

- `jkMain_SetVideoMode()` now rolls back known Xbox/SDL canvas and display-mode
  setup failures, but it is still not a fully staged resource transaction.
  Palette, HUD, camera, and render-state setup should eventually move behind a
  clearer prepare/commit boundary.
- `jkGui_SetModeMenu()` has failure rollback now, but existing call sites still
  generally do not inspect failure. Frontend mode setup needs a broader counted
  ownership audit around nested dialogs and menu-to-menu transitions.
- Cutscene skip and natural EOF now converge on `jkMain_VideoLeave()` /
  `jkCutscene_sub_421410()`. Hardware still needs to verify XMV open/finish,
  SMK fallback open-failure, and pause/skip paths do not leave audio or texture
  state behind.
- Frontend, gameplay UI, cutscene, and world textures still share implicit
  ownership. Texture purge hooks exist, but they are not yet proven as one
  coordinated phase exit.
- MP hardware validation remains required. The patched `sithWorld_Initialize()`
  asymmetry is a strong code-level bug, but the real proof is loading the same
  split-screen MP map on hardware and checking the new `MPLoadTrace` and
  `ResourceTrace` sequence.

## High-Risk OpenJKDF2 Findings

- There is no central Xbox resource owner for phase transitions.
- Texture ownership is implicit even though world, UI, wheel, cutscene, and
  menu assets all share the same GPU/FakeGL budget.
- Level-load failure cleanup is not visibly staged or transactional.
- SMK/cutscene diagnostic code should be isolated now that XMV is the working
  hardware route.
- Music has both streaming and whole-file paths; the fallback needs a hard
  retail-memory policy.
- Several useful debug logs still sit near file, bitmap, material, GUI, GOB,
  audio, and render transition paths.
- Hardware logs need memory/resource snapshots at level load by default, but
  not noisy per-item chatter.
- Stale asset flushing is partially implemented but not proven. World teardown
  frees major level-owned systems, bitmap unloads purge GPU refs, menu/cutscene
  texture release paths exist, GOB directories can unload their handles, and
  audio buffers/music have release paths. The missing guarantee is that these
  paths run as one coordinated phase exit and leave no frontend UI textures,
  wheel assets, movie textures, GOB/search-path state, material cache entries,
  DirectSound buffers, or mod-local assets behind.

## Priority Fix Plan

1. Add an Xbox resource-phase coordinator.
   - Enter/exit hooks for frontend, cutscene, game launch, level, mod, and
     shutdown.
   - One cleanup path per phase.
   - One failed-load rollback path.

2. Add resource snapshots.
   - Free memory and allocation high-water marks.
   - File handle count and high-water.
   - Mounted GOB/search-path counts.
   - Material, bitmap, FakeGL texture, and UI texture counts.
   - DirectSound buffer count and bytes.
   - Active phase, level, mod, and cutscene name.

3. Add stale-asset flushing verification.
   - After every phase exit, log before/after counts for GOB mounts, search
     paths, file handles, material cache entries, bitmap data, FakeGL textures,
     large UI textures, cutscene/movie textures, DirectSound buffers, music
     stream state, and gameplay UI assets.
   - Treat equivalent transitions as a leak test: repeating
     frontend -> cutscene -> level -> frontend should return to the same counts.
   - Add explicit flush points for frontend-only UI, gameplay wheel assets,
     cutscene/movie textures, mod-local GOB/search-path state, and failed-load
     staging assets.
   - Warn loudly when a phase exits with owned resources still resident, but
     keep per-asset dumps behind a verbose flag.

4. Gate hot-path logging.
   - File probes, bitmap ensures, material frees, GOB iteration, GUI focus,
     audio status, and cutscene frame logs should be opt-in.
   - Default hardware logs should be readable phase summaries.

5. Make level loading transactional.
   - Stage resources first.
   - Commit only after required assets succeed.
   - On failure, release staging and return to frontend.

6. Make texture ownership explicit.
   - Separate world/model/sprite/frontend/gameplay-ui/cutscene/scratch owners.
   - Purge phase-local textures on phase exit.
   - Add a hard warning before `STD3D_MAX_TEXTURES` exhaustion.

7. Harden audio.
   - Keep music pause/resume idempotent.
   - Budget or remove whole-file OGG fallback.
   - Log DirectSound/musical stream stats only at phase boundaries.

8. Promote XMV to the normal Xbox FMV path.
   - Keep SMK as fallback/diagnostic.
   - Make subtitles, skip input, and cleanup owned by the cutscene phase.

9. Move frontend/UI toward console ownership.
   - Cache static menu assets for the frontend phase.
   - Release them before gameplay when possible.
   - Keep gameplay wheel assets in a separate gameplay UI group.

## Definition Of Done

- Boot -> profile/menu -> new player/character menus -> level 1 -> XMV cutscene
  -> level 2 works repeatedly on hardware.
- SP and MP level loads either commit cleanly or fail back to frontend without
  rising memory, texture, file, GOB, or DirectSound counts.
- A mod map with non-stock MAT formats renders correctly or logs a precise
  unsupported-format reason.
- Pause/resume does not restart or permanently stop music.
- Repeated 30-minute hardware runs show stable phase snapshots with no rising
  resource high-water after equivalent transitions.
- Hardware logs contain enough resource state to diagnose failures without
  flooding audio, file, material, bitmap, GUI, or cutscene hot paths.
- CXBX-R remains useful for smoke testing, but hardware behavior is the final
  authority.
