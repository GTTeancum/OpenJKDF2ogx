# Xbox XEMU Soak Plan

This plan is for long-running stability coverage on XEMU without growing stale
ISO/stage artifacts. The prep script defaults to prepare-only mode. It should
not launch XEMU unless `-Run` is explicitly passed.

## Goals

- Prove the current Xbox build can survive repeated menu and level transitions.
- Exercise single-player map loads from JK1.
- Exercise multiplayer launch into a real gameplay map.
- Exercise four local split-screen players at least once through the System Link
  smoke harness.
- Capture RAM logs and screenshots often enough to diagnose black screens,
  menu clobbering, resource leaks, and post-load stalls.
- Leave only the latest disposable artifacts behind, with generated folders
  covered by `.gitignore`.

## Prepared Artifacts

`tools/xbox_xemu_soak.ps1` prepares:

- `build/xbox/openjkdf2_xemu_soak_current.iso`
- `C:\Games\Emulators\Xemu\OpenJKDF2Soak\xemu.toml`
- `build/xbox/xemu_soak_runs/manual_start/soak_manifest.txt`
- A System Link host/client setup using the existing two-XEMU launcher when
  System Link prep is enabled:
  - `C:\Games\Emulators\Xemu\OpenJKDF2SyslinkHost\xemu.toml`
  - `C:\Games\Emulators\Xemu\OpenJKDF2SyslinkClient\xemu.toml`
  - host/client XISOs inside those two instance folders.

The single-instance config is a manual launch checkpoint. The full supervised
run should be started through the script with `-Run` after the user confirms
XEMU is clear to start.

## Route

1. Menu boot leg
   - Boot through title/menu with FMV capped.
   - Stay in menus long enough to catch texture cleanup, footer rendering, and
     focus recovery issues.

2. Single-player map leg: `01narshadda.jkl`
   - Direct smoke autostart into the first JK campaign map.
   - Watch loading completion, gameplay frames, HUD draw, and resource traces.

3. Single-player map leg: `06abarons.jkl`
   - Direct smoke autostart into a different mid-game resource set.
   - Watch repeated teardown/load behavior after the previous XEMU phase.

4. Single-player map leg: `15maw.jkl`
   - Direct smoke autostart into another later-game resource set.
   - Watch memory counters and gameplay heartbeat continuity.

5. Four-player split-screen/System Link leg
   - Use the existing two-XEMU System Link harness.
   - Enable `XboxSystemLinkSmoke.ini` and `XSL4P.TXT`.
   - Host and client each auto-join four local players.
   - Verify host local players 0-3 and client local players 4-7 reached
     gameplay through RAM-log markers.

## Monitoring

Single-instance legs use `tools/xbox_xemu_smoke.ps1`:

- XEMU monitor RAM-log polling through `scripts/xbox/poll_xemu_ram_log.py`.
- Native XEMU screenshots through `scripts/xbox/xemu_native_screenshot.py`.
- Per-leg summaries under `build/xbox/xemu_smoke_runs`.

The 4P leg uses:

- XEMU monitor ports `4488` and `4489`.
- RAM-log polling into `build/xbox/xemu_ram_logs`.
- Verification through `scripts/xbox/verify_syslink_logs.py`.
- Optional native screenshots into `build/xbox/xemu_soak_runs`.

## Pass Signals

- The process remains alive until each phase timeout.
- RAM logs show boot, menu, load, and gameplay frame progress.
- `MPLoadTrace: GameplayShow done` appears for multiplayer/System Link.
- `SplitScreenPostLoad: armed enabled=1 locals=4` appears in the 4P leg.
- Host log contains `SplitScreenPostLoad: slot=0 player=0` and
  `SplitScreenPostLoad: slot=3 player=3`.
- Client log contains `SplitScreenPostLoad: slot=0 player=4` and
  `SplitScreenPostLoad: slot=3 player=7`.
- Screenshots are non-empty and show expected menus/gameplay, not blank white
  screens or clobbered UI.

## Fail Signals

- XEMU exits before the phase timeout.
- RAM-log polling cannot read the mirror.
- Fatal markers appear: `Received Exception`, `FATAL`, `Out of memory`,
  `E_OUTOFMEMORY`, `Unhandled`, `SECTION PARSE FAILED`, `Memory alloc failure`,
  `D3D Error`, or `failed 0x`.
- The 4P verifier reports missing or forbidden System Link markers.
- Repeated screenshots show a stuck loading bar, blank screen, or corrupted
  menu state.

## Commands

Prepare only:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\xbox_xemu_soak.ps1 -PrepareOnly
```

Run the supervised soak after XEMU is clear to launch:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\xbox_xemu_soak.ps1 -Run
```

Clean generated soak artifacts:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File tools\xbox_xemu_soak.ps1 -CleanArtifacts
```
