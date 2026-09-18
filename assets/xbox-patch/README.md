# Xbox gameplay patch

`cog/force_jump.cog` is the user's February 12, 2024 combined normal/Force
jump script, copied without behavior changes from the installed runtime.
Tap versus hold uses its existing 0.15-second threshold. This package does
not change controller bindings or inventory availability rules.

Run `python scripts/assets/build_xbox_patch.py` to generate
`build/xbox/release/mods/xbox_patch.gob`. The Xbox build and XEMU staging
script run this automatically. Ship the `mods` folder beside `default.xbe`.
The generated GOB is rebuilt from this tracked source; stock archives stay intact.

The Xbox JK resource loader explicitly loads this archive before the other
mod/resource GOBs, including `mots_xbox_patch.gob`. It is excluded in MotS mode.
Normal episode overrides and loose-resource precedence remain unchanged.
Remove any old loose `Resource/cog/force_jump.cog` from deployments so that
it cannot shadow the packaged version. Preserve it as a backup outside the
runtime if needed.

## Verification (2026-09-17)

- Full Xbox build passed; GOB round-trip and staged Git source match the
  original user's COG byte for byte (SHA256
  `48622522c12dab99cbfe1178e355b5be4e167ef6e31942e1188ba574ece2a770`).
- XEMU run `20260917_215156-force-jump-resolution` used a snapshot disk,
  no host input, and no loose jump COG. Its log confirms:
  `XboxPatch: force_jump resolved from mods\xbox_patch.gob`.
- Inspected native 51-second capture confirms Nar Shaddaa rendered with HUD
  and player weapon after the script loaded. Tap/hold input behavior was not
  tested here; this change packages the existing script without modifying it.
- Installed the XBE and GOB locally; backed up the identical loose COG outside
  the runtime before removing it so it cannot shadow the archive.
