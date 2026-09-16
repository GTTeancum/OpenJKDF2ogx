# OpenJKDF2 Xbox To-Do

## Tracked Items

1. Resolve audio stalls and dropouts.
2. Resolve remaining bot navigation stalls.
3. Complete one consolidated Xbox hardware run.
4. Evaluate 720p output (deferred).
5. Explore colored dynamic lighting (post-beta).

## 1. Resolve Audio Stalls and Dropouts

- [ ] Resolve the original audio stall/dropout when a current-build reproduction or audible-output evidence is available.

Four-player traversal and the original multiplayer-menu scenario show healthy buffers. The latest build has not been checked audibly by the user; healthy buffers alone do not establish that the audible issue is fixed.

## 2. Resolve Remaining Bot Navigation Stalls

Separate unfinished work per the user's 2026-09-14 clarification: **do not continue routing work and do not revert the existing routing changes**. The elevators/FPS goal is closed. Completed lift-contact and round-trip evidence is recorded in [bot lifts and performance](bot-lifts-performance.md).

- [ ] Resolve CTF routing stalls. Run `20260913_043528-bots-authored-speed-ctf` recorded no flag events in the inspected polls. Diagnose routing without restoring bot-only movement boosts.

## 3. Complete One Consolidated Xbox Hardware Run

- [ ] Perform one consolidated original Xbox hardware run later.

Accepted XEMU fixes remain accepted; this hardware pass does not put them back on the unfinished-work board.

## 4. Evaluate 720p Output

- [ ] Evaluate 720p separately from the completed system-driven 16:9 support.

Deferred feature.

## 5. Explore Colored Dynamic Lighting

Future / post-beta candidates.

- [ ] Investigate colored dynamic lighting for sabers and projectiles.
- [ ] Add a release-safe projectile light color source.
- [ ] Make the Xbox textured render path consume RGB dynamic lighting while preserving grayscale fallback.
- [ ] Add dynamic-light cap prioritization, favoring local-player/nearby important lights.
- [ ] Add targeted performance logging when this feature is pursued.

---

Last updated: 2026-09-16. This is the authoritative board for unfinished work only.
Completed-work evidence: [issue fixes](xbox-issue-1.md), [bot integration](bots-system-widescreen.md), [multiplayer HUD and setup verification](local-multiplayer-hud-verification.md), [bot lifts and performance](bot-lifts-performance.md), and [console controls menu](console-controls-menu.md).
