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
