# OpenJKDF2 Xbox Port — Roadmap

This document tracks the phased plan for porting OpenJKDF2 (Jedi Knight: Dark
Forces II reverse-engineered source) to the original Xbox console.

For day-to-day port context, build environment, and current debug state see
CLAUDE.md. For build toolchain background see BUILD_MIGRATION.md. For
D3D8-specific research see build/xbox/research_notes.md.

---

## Phase 0 — Boot + engine init  (complete)

- Xbox boots, Main_Startup runs, engine subsystems init (real or stubbed)
- File I/O works on hardware (CreateFileA path; D:\ remap)
- Debug log writes to D:\debug_openjkdf2.txt
- 14 engine core modules enabled from real source (stdGob, stdString,
  stdHashTable, stdMath, stdMemory, etc.)
- DirectSound wired, XInput wired, XBE generates and runs (title image
  not yet rendering on Xbox dashboard — punted to Phase 7)
- Build pipeline: XDK 5558 VC71 cl.exe → build_xbox.bat → imagebld.exe → XBE

Exit criteria: game boots cold from FTP install, log shows full subsystem
init without crashes. Done.

---

## Phase 1 — First pixel  (active, blocked on NV2A wedge)

- ✅ Reliable Clear+Swap loop with no GPU lockups (60-frame stability test
  passes; visible blue/green flicker on TV confirms present chain)
- ✅ Engine integration: full boot, GOB load, JKL parse, cog scripts,
  game tick, sithRender produces real geometry (273 verts / 129 tris of
  level surfaces reach `std3D_DrawRenderList`)
- ❌ Single triangle on TV via DrawPrimitiveUP — current wall
- ❌ Textured triangle
- ❌ Stable rendering across N frames

The wall: any real `DrawPrimitiveUP` call (with valid vertex data and
state) returns CPU-side, but the next `Present` hangs forever. NV2A
accepts the queued commands and wedges on execution; Present's fence
never signals.

Cross-referenced against five Xbox D3D8 sources (xquake — Microsoft's
own Quake port to OG Xbox; D3DQuakeX homebrew; Re-Volt; Rayman 4;
Mercenaries; UC2). Today's investigation eliminated:
- d3dpp config (now matches xquake exactly)
- BackBuffer / depth format combinations
- MSAA mode
- Recorded push buffer vs immediate API
- Locked-resource GPU sync barrier (texture + VB now properly unlocked
  / replaced with CPU heap)

Remaining suspects: implicit pixel shader bound at CreateDevice;
incomplete texture stage state at draw time (NV2A's stages 0-3 register-
combiner state); explicit render target binding. See `CLAUDE.md` for
the live test table.

Exit criteria: colored geometry rendering on screen, GPU stable across
hundreds of frames.

---

## Phase 2 — Renderer integration

- Wire engine's rdCache (NGON / line / sprite render lists) → real D3D8
  DrawIndexedPrimitive calls
- Texture upload pipeline (std3D_AddTexture family) → actual D3D8 textures
  with correct format and tile/swizzle layout for NV2A
- Convert engine vertex format to a working FVF (XYZ | DIFFUSE | TEX1 baseline)
- Render-state mapping for per-batch settings (alpha test, cull, fog)

Exit criteria: JK main menu renders correctly with text, buttons, and
backdrop.

---

## Phase 3 — First level

- Enable sithRender and its dependency chain (sithSurface, sithSector,
  sithThing render path)
- Sector-portal traversal runs unchanged from PC (Sith engine is
  sector-portal based — no BSP-style preprocessing needed; the engine
  draws sector-by-sector through portals in real time)
- World geometry on screen
- Player camera + movement
- Basic lighting (vertex-lit surfaces baseline, before Phase 6 enhancements)

Exit criteria: 01narshadda.jkl loads, the level renders, you can fly the
camera around it.

---

## Phase 4 — Gameplay loop

- XInput controller fully wired: movement, look, fire, weapon switch,
  force powers, jump, crouch, activate
- DirectSound audio playing — effects + ambient + music (CD audio or
  streamed)
- HUD + UI rendering (jkHud)
- Cogs + AI executing
- Save/load to memcard or HDD via standard Xbox save APIs
- Cutscene playback (Bink/SMACKER decoder)

Exit criteria: start a level, play it through to completion with full
input/audio/save support.

---

## Phase 5 — Performance & memory polish

- Performance tuning toward stable 30 fps minimum, 60 fps where possible
- Memory budget audit — Xbox has 64 MB unified RAM total; need headroom
  for GPU resources, audio buffers, save data, OS reservation
- Texture streaming / mip control
- LOD strategy where the original engine doesn't already provide one
- Pause/menu polish (back-stack, controller-friendly defaults)

Exit criteria: full DF2 campaign playable end-to-end with no softlocks,
OOM crashes, or sub-target framerate hot spots.

---

## Phase 6 — Feature parity with PC OpenJKDF2 enhancements

Bringing the OpenJKDF2 PC enhancements over to Xbox:

- Dynamic saber / blaster glow — additive light contribution from emissive
  sources (sabers, projectiles, muzzle flashes) onto nearby surfaces.
  Requires per-vertex or per-pixel runtime light injection.
- Overall improved lighting — better than vanilla vertex lighting;
  candidates include per-pixel lighting, normal-mapped surfaces where
  meaningful, smoother light falloff
- DDS texture support — load DDS-format textures (likely DXT1/3/5) for
  higher quality / smaller VRAM footprint than the engine's MAT format
- Alpha blending support — proper translucent surfaces (windows, glass,
  force effects) instead of the original's stipple/dither workarounds
- Multiplayer — system link, split screen, and the combination
  (split-screen with remote players over system link)
- Multiplayer bots — biggest item in this phase. Substantial work:
    - Prior art: the RBots mod (https://rbots.massassi.net/) implements bots
      at the .cog level on top of vanilla. Useful as behavior reference and
      validation target, but a navmesh-driven approach is the better long-
      term architecture (cleaner pathing, faster, less hand-authored data).
    - Navmesh generation from sector geometry (sector-portal walk → walkable
      surface extraction → mesh stitching across portals; needs to handle
      vertical movement, jump links, and force-jump reachability)
    - Navmesh persistence: bake at level load and cache to a side file
      (.nav alongside the .jkl). For shipped levels we can precompute and
      include the .nav in the build. For mod / user maps that don't ship a
      .nav, generate it at runtime on first load and write the cache so
      subsequent loads are fast.
    - Multiplayer-style AI behavior — more aggressive than the campaign AI:
      aim leading, dodge, weapon switching, force power usage, objective
      awareness for team modes, retreat / regroup logic
    - Bot configuration UI (count, difficulty, team, optional class/loadout)
    - Plays nicely with split-screen and system link players in the same
      match

Exit criteria: feature parity with the PC enhancements list, plus
functional bots in MP, validated on hardware.

---

## Phase 7 — Compatibility & release

- Retail Xbox hardware — primary target. All NTSC/PAL/PAL-60 video modes,
  all official memcard/HDD configurations
- Xbox 360 backwards compatibility — secondary target. Requires the game
  to work under the 360's BC layer; quirks vs. retail OG Xbox documented
  and worked around where possible
- Emulators not supported — CXBX-R and others can be useful dev tools but
  are not a release target
- XBE packaging finalized: title image (currently embedded but not
  rendering on Xbox dashboard — likely format / .xtf vs .xpr / placement
  issue), save image, version metadata
- FTP install documentation
- Public release notes / changelog

Exit criteria: stable build runs on retail hardware and 360 BC, public
release published.

---

## Out of scope (for now)

- Live multiplayer (Xbox Live is dead for OG Xbox; system link covers LAN)
- Custom modding / level loading beyond what vanilla supports
- HD textures beyond what fits in 64 MB
- Native Xbox 360 build (BC only)
