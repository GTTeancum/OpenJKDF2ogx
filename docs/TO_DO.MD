# OpenJKDF2 Xbox To-Do

Last updated: 2026-06-23

This is the quick-access working board for active Xbox port items. Keep it short, current, and biased toward things we still need to fix or verify.

## Immediate Fixes

- [ ] Fix 2P split-screen HUD stretching in horizontal split.
- [ ] Fix CTF initialization in split-screen so all local players enter the correct waiting/spawn flow.
- [ ] Fix P2 weapon model rendering in split-screen.
- [ ] Fix external camera behavior in split-screen.
- [ ] Audit and fix any P1 input/control state leaking into secondary split-screen viewports.
- [ ] Finish MP level select cleanup before the next build.
- [x] Change score limit and time limit to sliders; `0` means infinite.
- [x] Set default max Jedi rank to `8`.
- [x] Use the escape/pause menu background for MP level select.
- [x] Hide advanced options for split-screen level select.

## UI And Navigation

- [ ] Fix all escape/pause menu variants that overlap or bleed into the footer.
- [ ] Continue footer audit for screens still missing the console-style footer.
- [ ] Keep contextual button glyphs outside the colormap path so they stay clean.
- [ ] Verify ready-up flow: `A` joins, only P1 `Start` advances, only P1 backs out.

## Rendering

- [ ] Fix forcefield translucency; current MP forcefields can render as solid yellow.
- [ ] Investigate colored dynamic lighting for sabers and projectiles.
- [ ] Add a release-safe projectile light color source.
- [ ] Make the Xbox textured render path consume RGB dynamic lighting while preserving grayscale fallback.
- [ ] Add dynamic-light cap prioritization, favoring local-player/nearby important lights.
- [ ] Add targeted perf logging around dynamic light rendering.

## MotS Compatibility

- [ ] Build a curated `mots_xbox_patch.gob`; do not repack the full MotS resource archive.
- [ ] Ensure `JK_.CD` is not authoritative for mode switching.
- [ ] Keep MotS force powers scoped to SP for now; MP keeps JK force powers.
- [ ] Verify SP MotS -> JK switching fully reinitializes resource and mode state.
- [ ] Verify `mots_xbox_patch.gob` is ignored in MotS mode and loaded again when returning to JK compatibility mode.

## Validation And Housekeeping

- [ ] Two-machine/System Link probe testing is still pending.
- [ ] Revisit resource cleanup across menu-to-menu, menu-to-level, level-to-cutscene, and level-to-level transitions.
- [ ] Keep generated logs, smoke outputs, and temporary assets out of commits unless explicitly useful.
