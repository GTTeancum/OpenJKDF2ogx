# Xbox Efficiency Smoke Log

This log tracks autonomous CXBX-R efficiency passes. Each entry should include the
commit or dirty state tested, the path exercised, the run artifact folder, a short
outcome, and a risk tag for any code changes that followed.

Risk tags:
- `RISK:LOW`: harness/logging only, no runtime behavior change intended.
- `RISK:MEDIUM`: resource limits, allocation guards, or cleanup ordering that can
  affect quality or edge cases.
- `RISK:HIGH`: renderer, audio, timing, lifetime, or gameplay behavior changes
  where a regression may only show up after real play.

## Baseline

- Commit: `1cf69d11 Checkpoint Xbox memory diagnostics`
- Branch: `codex/xbox-efficiency-smoke-baseline`
- Open issues: hardware hangs at or just after first level tick, cutscenes now use
  XMV and are smooth but skip responsiveness/subtitles remain unfinished, and
  memory pressure after level load is still the primary suspect.

## Iterations

### 000 - Harness Upgrade

- Change: replace the simple CXBX smoke script with a process-clean, per-run
  artifact harness using `cxbxr-ldr.exe /load`, game heartbeat detection,
  fatal-pattern detection, and captured loader stdout/stderr.
- Risk: `RISK:LOW`
- Sanity run: `build\xbox\smoke_runs\20260519_195742-harness-sanity`
- Outcome: reached `fmv`; no fatal patterns. The run exposed repeated
  `CutsceneTrace: TexImage create` lines for 1024x512 textures during playback,
  so the first efficiency target is FMV texture lifetime/reuse.

### 001 - Title Directory Log

- Change: prefer `D:\debug_openjkdf2.txt` for Xbox logging, matching the retail
  title-directory convention visible in local Unreal Tournament Xbox source
  (`appBaseDir()` returns `D:\System\`). The old Partition1 path remains as a
  fallback.
- Risk: `RISK:LOW`
- Build: `cmd /c build_xbox.bat` succeeded.
- Smoke run: `build\xbox\smoke_runs\20260519_200423-d-log-sanity`
- Outcome: reached `static,fmv`; no fatal patterns. Active log path was
  `C:\Games\Emulators\CXBX\openJKDF2x\debug_openjkdf2.txt`, confirming `D:\`
  logging is working under CXBX-R.

### 002 - Large UI Texture Release

- Change: track Xbox UI bitmap texture uploads and release large padded UI
  textures (`>= 256x256`) when `std3D_XboxReleaseMenuTextures()` runs.
- Risk: `RISK:MEDIUM`
- Reason: startup/title/menu screens upload as 1024x512 textures. They should
  not remain resident when transitioning into level load, while smaller HUD
  assets stay cached.
- Build: `cmd /c build_xbox.bat` succeeded.
- Smoke run: `build\xbox\smoke_runs\20260519_200756-large-ui-release-sanity`
- Outcome: reached `fmv`; no fatal patterns. Log confirms
  `released large UI bitmap textures count=12`, all from startup/title UI
  uploads, before XMV playback starts.

### 003 - Smoke FMV Time Limit

- Change: add an optional smoke-only marker file,
  `D:\xbox_smoke_fmv_seconds.txt`, that causes XMV playback to auto-terminate
  after the requested number of seconds. The smoke harness can create it with
  `-FmvLimitSeconds`; normal builds and hardware runs without the file are
  unchanged.
- Risk: `RISK:LOW`
- Reason: full normal-boot smoke stayed in the intro XMV for 180 seconds, so it
  could not exercise FMV cleanup followed by level load.
- Build: `cmd /c build_xbox.bat` succeeded.
- Smoke run: `build\xbox\smoke_runs\20260519_201503-normal-fmv5-level-after-large-ui-release`
- Outcome: XMV auto-skip armed and fired after 5 seconds, released cutscene/menu
  state, then loaded `static.jkl`. It did not reach gameplay because the normal
  main menu stayed open after static load.

### 004 - Smoke Menu Autostart

- Change: add an optional smoke-only marker file,
  `D:\xbox_smoke_autostart_level.txt`, that makes the real Xbox main menu call
  the normal single-player loader for the requested level. The smoke harness can
  create it with `-AutoStartLevel`.
- Risk: `RISK:LOW`
- Reason: autonomous smoke must exercise FMV -> static cleanup -> normal
  single-player level load without hand-driving the menu or bypassing the
  post-FMV startup path.
- Build: `cmd /c build_xbox.bat` succeeded.
- Smoke run: `build\xbox\smoke_runs\20260519_202323-normal-fmv5-autostart-01narshadda`
- Outcome: reached `static,fmv,autostart,level-load`, then CXBX-R exited
  during `01narshadda.jkl` model loading with no game-side fatal string.
- Follow-up smoke run:
  `build\xbox\smoke_runs\20260519_202610-normal-fmv5-autostart-01narshadda-rerun`
- Follow-up outcome: reached
  `static,fmv,autostart,level-load,gameplay-show-done,first-tick,xbox-frame`;
  stayed alive for the 300-second watchdog with no fatal patterns. The first
  exit did not reproduce.

### 005 - Respect Existing CXBX-R Sessions

- Change: before a smoke run, if any CXBX-R process is already running, wait
  three minutes and check again. If it is still running, abort the smoke run
  instead of killing the existing emulator session.
- Risk: `RISK:LOW`
- Reason: autonomous smoke should not interrupt a manual hardware/emulator
  observation that is already in progress.

### 006 - Throttle Texture Diagnostics

- Change: reduce the Xbox texture diagnostic budgets for texture create,
  subimage, texture stage, and direct movie draw logging from large
  kitchen-sink values to 12 lines each.
- Risk: `RISK:LOW`
- Reason: the FMV/texture diagnosis has already shown the relevant path; keeping
  hundreds of repeated `CutsceneTrace: TexSubImp` lines per smoke run adds log
  I/O pressure and hides the useful level-load tail.
- Build: `cmd /c build_xbox.bat` succeeded.
- Smoke run:
  `build\xbox\smoke_runs\20260519_203354-normal-fmv5-autostart-01narshadda-throttled-texlog`
- Outcome: reached
  `static,fmv,autostart,level-load,gameplay-show-done,first-tick,xbox-frame`;
  stayed alive for the 300-second watchdog with no fatal patterns. Texture
  diagnostic lines dropped from 272 to 36 on the comparable smoke path.
