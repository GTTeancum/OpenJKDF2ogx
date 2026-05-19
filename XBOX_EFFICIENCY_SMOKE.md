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
