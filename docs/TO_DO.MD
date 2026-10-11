# OpenJKDF2 Xbox To-Do

## Tracked Items

1. Resolve remaining bot navigation stalls.
2. Complete one consolidated Xbox hardware run.
3. Fix missing boss attack fans with modern saber enabled.

## 1. Resolve Remaining Bot Navigation Stalls

Separate unfinished work per the user's 2026-09-14 clarification: **do not continue routing work and do not revert the existing routing changes**. The elevators/FPS goal is closed. Completed lift-contact and round-trip evidence is recorded in [bot lifts and performance](bot-lifts-performance.md).

- [ ] Resolve CTF routing stalls. Run `20260913_043528-bots-authored-speed-ctf` recorded no flag events in the inspected polls. Diagnose routing without restoring bot-only movement boosts.

## 2. Complete One Consolidated Xbox Hardware Run

- [ ] Perform one consolidated original Xbox hardware run later.

Accepted XEMU fixes remain accepted; this hardware pass does not put them back on the unfinished-work board.

## 3. Fix Missing Boss Attack Fans with Modern Saber Enabled

Reported behavior: boss characters show no saber swing fan with modern saber enabled. The rendering change is implemented and built; this item remains open for the outstanding visual acceptance below.

2026-10-11 verified progress: prior sequential native reviews confirm stock-content fans for Yun, both Boc blades and Jerec, plus Modern Off fallback for Boc. The new rounded white/colored tip is directly confirmed at 480p in player first- and third-person views. GLOW3-inherited and bad-camera runs are excluded. A boss-only 50 ms history of real poses addresses traced render-rate-dependent narrow fans; live keyframe damage windows clear stale history. This is supported for the traced cases, not a universal explanation of every reported missing fan. Player fan shape, collision and authored attack timing are unchanged.

The release build and geometry/history, lighting, menu/depth, model-lighting and batching tests passed as recorded in the handoff. Final XBE SHA-256: `C1B2452E5CF94A91EFEA2899E53CDDCDE7334155B317D5247CD914C927884890`. It has not been deployed. The final MotS and 720p runs completed with zero recorded fatal events; metadata confirms 60 captures at 640x480 and 12 at 1280x720 respectively. Completion and fan counters do not constitute visual acceptance.

Remaining acceptance:

- [ ] Establish conclusive visual coverage of MotS Kyle attack start, swing and recovery. The reviewed arena sequence was mostly idle/power recovery; the later 60-capture attack sequence remains unreviewed despite real attack/fan traces.
- [ ] Finish third-person sequence review (024-039) and visually review the 12 true-720p captures. These captures were not reviewed during the documentation follow-up; the subsequently displayed proofs came from the earlier reviewed 480p sequences.
- [ ] Complete outstanding native interruption/retraction/death and comprehensive depth coverage where practical; deterministic reset tests pass, but the recorded native reviews do not establish these cases. Timed captures are not every rendered frame; retain this limitation and the non-frame-matched Boc On/Off comparison.
- [ ] Close this item only after outstanding visual acceptance is resolved. Hardware remains tracked separately in #2; audio was muted and is untested for these changes.

Full implementation, recorded visual reviews, final-run metadata, build/test evidence and limitations: [modern saber](modern-saber.md). This work does not reopen colored lighting.

---

Last updated: 2026-10-11. This is the authoritative board for unfinished work only.
Scope decision: colored dynamic lighting was closed without further work on 2026-10-08 at the user's request.
Completed-work evidence: [issue fixes](xbox-issue-1.md), [bot integration](bots-system-widescreen.md), [multiplayer HUD and setup verification](local-multiplayer-hud-verification.md), [bot lifts and performance](bot-lifts-performance.md), [console controls menu](console-controls-menu.md), [MotS Xbox compatibility](MOTS_XBOX_COMPAT.md), and [720p Xbox hardware validation](xbox-widescreen-output.md).
