# OpenJKDF2x Saber Rendering Notes

These notes are for a future Codex pass that borrows the OpenJPB saber draw
path for OpenJKDF2x. They summarize the source-backed behavior in this
repository as of the lightsaber occlusion pass.

## Source Chain

- `jedi_HandleSabre` is the gameplay owner for player saber emission.
  Inventory anchor: `inventory/function_map.tsv`, matched-PC RVA `0xB23F0`.
- `jedi_draw_sabre_blade`, `jedi_draw_long_sabre`, and
  `jedi_draw_power_sabre` compute world-space blade endpoints from authored
  model nodes and call `fx_screenGlow` for the blade/core.
- Powered saber variants also call `drawCylinder`; the portable runtime
  captures those cylinders through the same glow queue so they share ordering
  and occlusion behavior.
- `fx_screenGlow` is a PDB-named API at matched-PC RVA `0xA3C40`. In the
  original Windows renderer it submits immediate `_NoScaleEndPoly` geometry
  with camera-space depth. In OpenJPB it remains the gameplay-facing API and
  forwards to a portable hook.

## Portable Pattern

The port should keep three concepts separate:

- Gameplay emits glow requests in world space, with the original color and
  width values intact.
- The renderer owns projection and depth comparison, not `jedi.c`.
- The glow pass runs after the opaque/world and model/immediate occluders have
  written depth.

OpenJPB currently implements that as:

- `src/reconstructed/original/jedi.c`: node selection, saber colors, widths,
  and `fx_screenGlow` calls.
- `src/reconstructed/original/fx.c`: `fx_screenGlow` hook handoff.
- `src/reconstructed/portable/game_runtime.c`: queues glow draws, snapshots
  the completed shared depth surface, then flushes glow draws.
- `src/reconstructed/portable/software_renderer.c`:
  `jpb_SoftwareDrawGlowLine` projects the segment, draws additive discs along
  it, and skips samples hidden by the supplied depth buffer.

## Occlusion Detail

The important bug fix was timing. Snapshotting the glow depth buffer directly
after world render made sabers ignore later actor/enemy/immediate occluders.
The correct portable timing is:

1. Render world to the shared depth buffer.
2. Render player, second player, enemies, and deferred immediate screen polys
   into the same depth buffer.
3. Snapshot that completed surface for glow.
4. Flush `fx_screenGlow` and captured cylinder draws against the snapshot.

OpenJPB logs the proof as `saber_glow=queued/dropped/composited/rejected`.
The final `rejected` value is the number of glow samples culled by depth.

## What To Port

For OpenJKDF2x, port the architecture rather than JPB-specific constants:

- Keep the game-facing saber API thin: pass endpoints, color, and width.
- Resolve blade endpoints from the engine's authored weapon/skeleton nodes.
- Defer rendering to a translucent/glow queue.
- Feed the glow queue a depth surface that already includes world and actor
  occluders.
- Track both composited and depth-rejected glow samples in diagnostics.

Avoid copying JPB-specific model IDs, node IDs, color tables, or powerup
rules unless OpenJKDF2x has an equivalent authored data source. Those belong
to `jedi_HandleSabre`, not the renderer.

## Verification To Recreate

Minimum gates worth recreating:

- A projection/renderer unit test where an unoccluded glow draws pixels.
- A second unit test where a fully nearer depth buffer produces zero glow
  pixels and nonzero depth-rejected samples.
- A gameplay capture log that proves a real saber was queued, attached to the
  player weapon nodes, and had nonzero depth-rejected samples in a real level.

