# Handoff: Blaster Bolt Render Bug — Xbox Port

## Mission

The Xbox port of OpenJKDF2 visually mis-renders blaster bolts. Find the root cause and fix it. Physics is fine; collision is fine; user gets correct hit feedback. What's broken is purely the visible mesh of the bolt projectile in flight.

## Project context

**Repo:** `C:/Programming/GitHub/OpenJKDF2ogx/` — port of the open-source JK1 engine reimplementation (`_ORIGINAL_READ_ONLY/OpenJKDF2-master/`) to the original Microsoft Xbox (NV2A / XDK 5558).

**Build:** `build_xbox.bat` (Windows cmd). VC71 CL + XDK headers. Produces `build/xbox/release/default.xbe`. All source files compile as C++ (`/Tp`) except a small explicit C list (`xbox_debug.c`, `crt_aliases.c`, `quake_stubs.c`).

**Critical hands-off zones:**
- `src/Platform/Xbox/fakeglx.cpp` — graft from another project, never modify. Read for understanding only.
- `src/Platform/Xbox/gl/gl.h` — graft header, same rule.

**Diagnostic logging:** `XDBG` / `XDBGF` / `xbox_debug_Printf` from `src/Platform/Xbox/xbox_debug.h`. Writes to `OutputDebugStringA` and `D:\debug_openjkdf2.txt` with auto-flush. User retrieves via FTP after each test run on hardware (Cxbx-Reloaded emulator captures these strings to its log window too).

**Test loop:** edit code → `build_xbox.bat` (~30s) → flash XBE → user fires bolts in 01narshadda.jkl → user pastes `D:\debug_openjkdf2.txt` tail. ~3-5 minute round-trip per iteration.

**Known existing diagnostics still in tree** (search `xbox_debug_Printf` / `XDBGF`):
- `BoltSrc[N]` (sithRender.c) — bolt's `pThing->position` and `lookOrientation` at render time
- `rdThing_Draw[NEW model3=]` / `Model3Draw[NEW model=]` / `Model3Cull[NEW model=]` (rdThing.c, rdModel3.c) — entry probes with pointer-dedup (32 unique slots each)
- `DrawMeshEnter[NEW mesh=]` (rdModel3.c) — mesh entry probe (same dedup)
- `XformProbe[N]` (rdModel3.c) — view-space output after `view × lookOrientation` transform
- `SmallTri[N]` (std3D.c) — view-space tris from any small mesh in the per-tri loop
- Many older diags scattered through sithMain.c, sithRender.c — heartbeats and pipeline traces

When done, these should be removed or commented out — there are XXX-many of them and they add boot-time overhead.

## The bug

**Symptom (user's own words):**
> "Blaster bolts fly down vertically, as in starting in the sky."
> "Sometimes I don't see that vertical fire artifact until multiple shots in."
> "There's a Rodian that fires green blaster bolts and I've seen a green artifact from that."
> "Different weapons (crossbow, rail detonator, concussion rifle) all show artifacts."
> "I see the bolts, they're just rendering incorrectly. It's not a visibility issue, it's an issue with the rendered bolt being on the wrong vector."

**The bug:** the bolt's mesh renders, but along the wrong vector. Collision is on the correct aim ray (the spark lands where aimed). The visible bolt model travels in a different direction from where the collision goes. Artifact color matches the firing weapon (red bryar, green Rodian's bolts, etc.) — confirming it's actually projectile geometry, not unrelated debug or sky elements. Reproduces with multiple weapons (bryar, crossbow, rail detonator, concussion rifle, Rodian).

**Reproduces in:** JK1.gob → `01narshadda.jkl` (single player). Also JK1MP.gob → `m10.jkl` (multiplayer test level).

## What's been definitively ruled out

Don't waste time re-checking these — direct measurement (logged on hardware) has confirmed them clean:

1. **Bolt's `sithThing.position`** — `BoltSrc[N]` logs show correct progression along aim vector. Physics is right.
2. **Bolt's `lookOrientation` matrix at render time** — orthonormal, `lvec` along velocity, `scale = position`. Logged extensively.
3. **`.3do` model data for the bryar bolt** (`bry0.3do`):
   - 1 hierarchy node, meshIdx=0, posRotMatrix is identity
   - HNode0: pos=(0,0,0), rot=(0,0,0), pivot=(0,0,0)
   - insertOffset=(0,0,0)
   - Mesh0: 15 vertices, 11 faces, bbox `X=[-0.008..0.008] Y=[-0.091..0.018] Z=[-0.006..0.006]` (long axis = local +Y, matches engine lvec convention)
4. **Near-plane clipping** — `NEAR-PROBE` showed bolt vertices in view-space have `ymin = 0.043..0.121`, well past `zNear=0.0156`. Not a near-plane issue.
5. **D3DVERTEX struct layout** — `D3DVERTEX_ext` is used consistently (SDL2_RENDER defined). The `memcpy` from `rdCache_aHWVertices` → `GL_tmpVertices` preserves x/y/z correctly.
6. **rdMatrix math** — `Multiply34`, `TransformPoint34`, `InvertOrtho34`, `BuildFromLook34` all match upstream byte-for-byte and arithmetically. Verified by hand against logged values.
7. **Cross-pass camera matrix swap** — earlier theory that `rdCamera_pCurCamera` was being swapped between bolt transform and std3D probe. Disproven: `XformProbe` captures `view.scale` *at the transform call site* and the values shown match what `BoltTri` later sees — same camera throughout.
8. **The "vertical red line" measured at `(0.013, 0.101, 1.202)` was NOT the bolt** — it's the player's first-person gun (constant view-space coords across totally different player positions and camera matrices). All the early "+1.246 Z offset" analysis was on the wrong triangle. The gun's untextured sub-faces consistently fired the `!t->texture` filter first and saturated the counters.
9. **Frustum cull on the bolt** — `Model3Cull[NEW model=0CF1FC94]: radius=0.091 frustumCull=1 (INSIDE)`. The bolt reaches `rdModel3_DrawHNode` and `rdModel3_DrawMesh`. Not being rejected by sphere-in-frustum.

## What's known about the bolt's render path

```
sithMain_Tick → sithRender_Draw
  → per sector: render world surfaces (textured, normalTris)
  → per sector: thingsList iteration
    → sithRender_RenderThing(bolt)                      ← BoltSrc logs here, pos correct
      → pThing->lookOrientation.scale = pThing->position  ← scale = position, verified
      → rdThing_Draw(rdthing, &lookOrientation)         ← rdThing_Draw probe fires
        → rdModel3_Draw(thing, mat=lookOrientation)     ← Model3Draw probe fires
          → frustum-cull check                           ← Model3Cull shows INSIDE
          → rdPuppet_BuildJointMatrices (no puppet path)
            → hierarchyNodeMatrices[0] = lookOrientation * Translate(0) * identity
                                       = lookOrientation
          → rdModel3_DrawHNode(rootNode)
            → rdModel3_DrawMesh(mesh, &hierarchyNodeMatrices[0])  ← DrawMeshEnter SHOULD fire but counter is saturated
              → rdMatrix_Multiply34(out, view, hierarchyNodeMatrices[0])
              → rdMatrix_TransformPointLst34(out, mesh->vertices, aView, n)
              → rdModel3_DrawFace per face
                → rdPrimit3_ClipFace / NoClipFace → produces vertexDst (view-space)
                → fnProjectLst → memcpy on Xbox (pass-through)
                → rdCache_AddProcFace
  → rdCache_Flush → SendFaceListToHardware
    → per face: AddTextureCache, copy verts to aHWVertices
    → std3D_AddRenderListVertices(aHWVertices) → memcpy → GL_tmpVertices
    → std3D_AddRenderListTris (aHWNormalTris bucket if textured / aHWSolidTris if solid)
    → std3D_DrawRenderList
      → glFrustum setup
      → per tri: glBegin(GL_TRIANGLES); V3F_ENGINE_TO_GL macro = glVertex3f((v)->x, (v)->z, -(v)->y)
```

**Xbox-specific divergences vs upstream PC (SDL2/GL):**

- `rdCamera_PerspProject` and `PerspProjectLst` at `src/Engine/rdCamera.c:405,427` are **pass-through on Xbox** — vertices stay in view-space, GPU handles projection. PC pre-projects to screen-space.
- `rdCache_aHWVertices` write in `src/Raster/rdCache.c:768-786` has a `#ifdef TARGET_XBOX` branch that copies view-space xyz straight through and zeros `.nx/.nz`. Different from the PC path which writes pre-projected screen coords.
- `std3D_DrawRenderList` in `src/Platform/Xbox/std3D.c:922+` uses `glFrustum` for HW projection and the `V3F_ENGINE_TO_GL` axis swap (`x, z, -y`) before `glVertex3f`. Engine convention: x=right, y=depth, z=up. GL: x=right, y=up, z=back.

**`rdCamera_pCurCamera->fnProjectLstClip`** is NOT assigned on non-TWL builds (only inside `#ifdef TARGET_TWL` blocks in `rdCamera.c:170,182`). Called at `sithRender.c:927` and `sithRenderSky.c:55`. Function pointer is likely garbage/null on Xbox. Hasn't crashed so the calling sites must be gated, but worth a closer look as a potential silent rendering miss. **Upstream PC also doesn't set this** — so it's not Xbox-specific, but it's also not clear why it works on PC.

## What still hasn't been measured

1. **`rdthing.type` for each weapon variant.** Bryar logged as MODEL (type=1). Crossbow, rail det, concussion, Rodian, etc. — unknown. If any take a different path (SPRITE3, PARTICLECLOUD, POLYLINE) all our model3-side probes have been blind to them.
2. **The bolt template's `lookOrientation` matrix** — pre-multiplied into every spawned bolt at `sithThing_Create:1065`. Never logged.
3. **Whether `frameTrue == rdroid_frameTrue` ever hits true for a bolt** (which would skip BuildJointMatrices and use stale matrices). Never logged.
4. **The bolt's actual view-space vertex values during real-time render** — `XformProbe` slots got filled by characters before the bolt arrived. Filter needs to be bolt-specific.
5. **Whether secondary `pTemplate` things spawn alongside bolts** — could be a glow sprite or trail that's the actually-misrendered visual the user sees, not the bolt's own mesh.

## High-probability next theories

We're looking for a code path where the bolt's geometry gets transformed using something other than its true `sithThing.position` / `lookOrientation`, OR where an extra rotation is baked into its render matrix that doesn't apply to its motion vector.

1. **Bolt is rendered through a different rdthing.type path than MODEL.** `rdThing_Draw` (`rdThing.c:154-180`) dispatches by type: MODEL → `rdModel3_Draw`, SPRITE3 → `rdSprite_Draw`, PARTICLECLOUD → `rdParticle_Draw`, POLYLINE → `rdPolyLine_Draw`. We only logged `rdthing.type=1` (MODEL) for one bryar bolt. **Crossbow / concussion / rail / Rodian bolts may have different `rdthing.type` values** and go through sprite or particle paths where the rendering math is completely different. Sprites in particular billboard to camera and use a fixed forward-facing quad — if the billboarding code uses the wrong center position (e.g. `templateThing->position` instead of `spawnedThing->position`), the sprite renders at template world origin (0,0,0 or wherever the template lives in the JKL) instead of at the bolt's flight position. **This would produce exactly the "bolt appears in the sky / wrong vector" symptom.** Add a probe at the top of `rdSprite_Draw` and `rdParticle_Draw` that logs the input matrix's `scale` (= position) when called for `type=3` (WEAPON) sithThings.

2. **`pTemplate` chain rendering the wrong thing.** `sithThing_Create` (`sithThing.c:1079-1089`) spawns a *secondary* thing if the projectile template has a `pTemplate`. Common for bolts to have a nested "glow sprite" or "trail" sub-template that renders alongside the bolt body. If THAT secondary thing's rendering is broken (wrong position, wrong attach, wrong inherit) but the user is looking at it and assuming it's the bolt, the misrender we're chasing is actually the sub-thing. Log secondary-thing spawns by adding a probe in `sithThing_Create:1079-1089` when `pThingRet->pTemplate` is non-null.

3. **`hierarchyNodeMatrices` reused across the same frame.** `rdModel3_Draw:1204` skips `rdPuppet_BuildJointMatrices` when `pCurThing->frameTrue == rdroid_frameTrue`. If somehow the bolt thing's `frameTrue` matches `rdroid_frameTrue` from a previous render call this frame, BuildJointMatrices is skipped and the bolt uses STALE `hierarchyNodeMatrices[0]` from before. The stale matrix would contain whatever world transform the bolt had on its previous render call — which (if cached across frames) could be its **spawn position from frame 0** instead of its current flight position. Especially nasty if the bolt is in multiple sectors' `thingsList` (we saw `EnterSector` log the bolt entering 3 sectors). Probe: log `pCurThing->frameTrue` vs `rdroid_frameTrue` at `rdModel3_Draw` entry for bolts; if they ever match, the stale-matrix theory is confirmed.

4. **The `aView` global buffer aliasing.** `aView[512]` (declared `globals.c:37`) is the temp output of `rdMatrix_TransformPointLst34` in `rdModel3_DrawMesh:1328`, then handed straight to `rdPrimit3_NoClipFace` as `vertexSrc.vertices` at `:1382`. If something else also writes `aView` (sky? particles? render-prep utility?) between bolt's transform and bolt's face processing, the bolt's view-space coords get clobbered with whatever the other writer put there. Especially relevant if bolt rendering is interleaved with sprite/particle rendering. Grep ALL writers of `aView`. (`grep -rn "aView" src/` already done once: only rdModel3 writes it, but a static-analysis pass for sprite/particle equivalents is warranted.)

5. **A baked rotation in the bryar bolt template's `lookOrientation`.** `sithThing_Create:1065` does `PreMultiply34(&pThingRet->lookOrientation, &pTemplateThing->lookOrientation)`. If the bryar bolt **template** has a non-identity `lookOrientation` (e.g. rotated 90° in the JKL/template file), every spawned bolt inherits that rotation in addition to the fire-direction matrix. Bolt physics uses the resulting compound to compute velocity (so it flies correctly, matching collision). But the mesh's RENDERED orientation reflects the same compound rotation — if the template rotation is "non-physical" (designed to orient the mesh's visual differently from its motion vector), the mesh would point in one direction while traveling in another. This actually matches the user's observation. **Probe: log `pTemplateThing->lookOrientation` for the bryar template at `sithThing_Create` entry.** Different templates having different rotations would also explain why crossbow/rail/concussion show different artifact orientations.

## Suggested approach

1. **Confirm `rdthing.type` for ALL bolt variants** — bryar, crossbow, rail, concussion, Rodian. Add a probe at `sithThing_Create:1056` (right after `CreateThingOfType`) that logs the template name, `pThingRet->type` (sithThing type), `pThingRet->rdthing.type` (rdthing type). Run, fire each weapon once. The result tells us whether different bolts take different `rdThing_Draw` dispatch branches.
2. **Log the bolt template's `lookOrientation` once per template** — at `sithThing_Create` entry, log `pTemplateThing->lookOrientation` (rvec/lvec/uvec/scale). If any bolt template has a non-identity orientation, the `PreMultiply34` at line 1065 mixes it into every spawned bolt's matrix and the mesh visibly travels along the WRONG direction relative to motion. This is the strongest candidate given the user's "wrong vector" framing.
3. **Probe `rdSprite_Draw` and `rdParticle_Draw` entries.** Log the input matrix `scale` and the owning thing's `type` and `position` whenever they're called. If ANY bolt routes through sprite/particle rendering, we'll see it.
4. **Check the stale-matrix theory.** Add a probe at `rdModel3_Draw:1204` (the `frameTrue != rdroid_frameTrue` check) — log when the comparison is **true** (meaning BuildJointMatrices is skipped) for `type=3` things. If bolts ever hit this path, the bug is stale hierarchyNodeMatrices.
5. **User-marker key for visual correlation.** Add a probe wired to a D-pad button: when pressed, write `"*** USER MARKER FRAME=<frame> ***"` to the log. User holds it the instant they see the artifact. Gives us a precise log anchor to scan backward from.
6. **Stop trying to fix before the visual evidence and runtime probe data correlate.** We've been guessing for too long.

## Files most relevant to this bug

- `src/Engine/sithRender.c:3196-3260` — `sithRender_RenderThing`, the per-thing render entry
- `src/Primitives/rdModel3.c:1148-1294` — `rdModel3_Draw`, `rdModel3_DrawHNode`
- `src/Primitives/rdModel3.c:1297-1490` — `rdModel3_DrawMesh`, the per-mesh transform
- `src/Primitives/rdModel3.c:1490-1620` — `rdModel3_DrawFace`, per-face submission to rdCache
- `src/Raster/rdCache.c:380-1500` — `rdCache_SendFaceListToHardware`, the big function that classifies faces, walks materials, builds the HW vertex buffer
- `src/Raster/rdCache.c:1450-1500` — `rdCache_DrawRenderList`, calls std3D
- `src/Platform/Xbox/std3D.c:803-902` — `std3D_AddRenderList*` (just memcpys into GL_tmpVertices / GL_tmpTris)
- `src/Platform/Xbox/std3D.c:922-1200` — `std3D_DrawRenderList`, the per-tri GL submit loop
- `src/Engine/rdThing.c:154-180` — `rdThing_Draw` type dispatch
- `src/Engine/rdPuppet.c:57-340` — `rdPuppet_BuildJointMatrices` (puppet vs no-puppet path)
- `src/Engine/rdCamera.c:403-440` — `rdCamera_PerspProject` and `Lst` with Xbox pass-through

## What NOT to do

- Don't add more loose-filter probes. They will saturate with non-bolt geometry and tell us nothing.
- Don't try to "fix" anything until you have a runtime probe that confirms the bolt's actual render-path values. Every hand-derived fix attempt so far has been on the wrong triangle.
- Don't touch `fakeglx.cpp` or `gl/gl.h`.
- Don't reformat existing files — surgical edits only.

## Tooling tips

- User runs Cxbx-Reloaded emulator (works ~identically to hardware). Capture window shows OutputDebugString. They paste the relevant tail.
- `build_xbox.bat` is fast (~30s incremental). Always rebuild before reporting "done."
- The user is patient with iteration but is sensitive to wasted cycles. Be concise and decisive.
- See `XBOX_HACKS.md` for other known issues and intentional shortcuts in this port.

## Final note

The user is personally invested in fixing this bolt visual. It's been the focus of many hours. They've been very patient. If you can move this from "we have data but no signal" to "here is the line of code that's wrong," that would be a real win. Good luck.
