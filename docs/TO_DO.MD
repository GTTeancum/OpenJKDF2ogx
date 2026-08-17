# OpenJKDF2 Xbox To-Do

Last updated: 2026-08-17

This is the quick-access working board for active Xbox port items. Keep it short, current, and biased toward things we still need to fix or verify.

## Pre-Beta Remaining

- [ ] Re-run the final always-on XEMU soak after the MotS/mod-map/XMV fixes, covering JK SP/MP, MotS SP/MP, split-screen, System Link, and available mod maps.

## Completed Immediate Fixes

- [x] Fix 2P split-screen HUD stretching in horizontal split.
- [x] Fix CTF initialization in split-screen so all local players enter the correct waiting/spawn flow.
- [x] Fix P2 weapon model rendering in split-screen.
- [x] Fix external camera behavior in split-screen.
- [x] Audit and fix any P1 input/control state leaking into secondary split-screen viewports.
- [x] Confirm MotS strafe direction fix.
- [x] Fix XEMU soak APHC mod selection; `aphc.gob` playable level is `fire-control.jkl`, not loose test map `city3.jkl`.
- [x] Finish MP level select cleanup before the next build.
- [x] Change score limit and time limit to sliders; `0` means infinite.
- [x] Set default max Jedi rank to `8`.
- [x] Use the escape/pause menu background for MP level select.
- [x] Hide advanced options for split-screen level select.

## UI And Navigation

- [x] Fix all escape/pause menu variants that overlap or bleed into the footer.
- [x] Continue footer audit for screens still missing the console-style footer.
- [x] Keep contextual button glyphs outside the colormap path so they stay clean.
- [x] Verify ready-up flow: `A` joins, only P1 `Start` advances, only P1 backs out.

## Rendering

- [x] Fix forcefield translucency; current MP forcefields can render as solid yellow.

## Future / Post-Beta Candidates

- [ ] Investigate colored dynamic lighting for sabers and projectiles.
- [ ] Add a release-safe projectile light color source.
- [ ] Make the Xbox textured render path consume RGB dynamic lighting while preserving grayscale fallback.
- [ ] Add dynamic-light cap prioritization, favoring local-player/nearby important lights.
- [ ] Add targeted perf logging around dynamic light rendering.

## MotS Compatibility

- [x] Fix MotS-only reversed strafe direction.
- [x] Build a curated `mots_xbox_patch.gob`; do not repack the full MotS resource archive.
- [x] Ensure `JK_.CD` is not authoritative for mode switching.
- [x] Keep MotS force powers scoped to SP for now; MP keeps JK force powers.
- [x] Verify SP MotS -> JK switching fully reinitializes resource and mode state.
- [x] Verify `mots_xbox_patch.gob` is ignored in MotS mode and loaded again when returning to JK compatibility mode.

## Validation And Housekeeping

- [x] Include `box art.png` in the beta/release package.
- [x] Include `OpenJKDF2xCutsceneConverter.exe` in the beta/release package beside `default.xbe` or in `cutscene_converter\`.
- [x] Fix XMV converter packet alignment and verify regenerated JK/MotS intro XMVs through the Xbox runtime decoder in XEMU.
- [x] Fix MotS SAN cutscene audio extraction; embedded SMUSH `IACT` audio now becomes a PCM XMV audio stream instead of silent video-only output.
- [x] Investigate MotS `s2l1_palace.jkl` XEMU black-screen/stall; fixed per-instance AI actor allocation and verified MotS palace -> mod -> JK transition proof.
- [x] Investigate `impsiege/impsiege.jkl` XEMU soak stall; verified gameplay-ready transition in the same MotS/mod/JK proof pass.
- [x] Complete live System Link real-lobby proof in XEMU or XEMU+Xbox.
  - [x] Audit against UC2 secure XNet/session flow.
  - [x] Reset stale System Link launch/session state after stop/game close.
  - [x] Pass deterministic two-XEMU 4+4 local-player smoke harness.
  - [x] Pass live two-XEMU real-lobby discovery/secure-launch proof using the pcap backend; local UDP stayed at `peers=0`, multicast failed monitor setup and should not be treated as the proof path.
- [x] Revisit resource cleanup across menu-to-menu, menu-to-level, level-to-cutscene, and level-to-level transitions.
- [x] Keep generated logs, smoke outputs, and temporary assets out of commits unless explicitly useful.
