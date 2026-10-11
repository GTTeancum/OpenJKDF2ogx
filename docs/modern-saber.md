# Modern saber

Implemented September 18, 2026 for Xbox. The setting now lives in **Setup >
Display** as **Modern saber rendering**, off by default. It applies to SP and MP
and saves as `modernSaber` in the title's writable `U:\xbox_registry.cfg`. The
console cvar is `r_modernSaber`. Xbox JSON is stubbed, so this preference uses
the file-backed registry.

On uses a white blade, colored edge and the accepted OpenJPB curved swing fan.
`src/Gameplay/jkSaberModern.inc` adapts `jpb_saber_fan_sector` and its relative
blade history from `Jedi Power Battles recomp/src/reconstructed/original/jedi.c`.
The white fan reaches the live tip, with a short five-percent outer feather.
History is isolated by view, actor, blade and POV; stale poses, attack ending,
large jumps and invalid owners reset it. Collision and weapon timing are unchanged.

The effect uses explicit RGBA opacity and near-plane clipping. Zero opacity
remains transparent; stock inverse-alpha materials retain their old behavior.
First-person modern saber now renders after the POV weapon model has written hand depth, then draws the blade with depth reads and no depth writes. That keeps hands and nearby world geometry from punching through or being overwritten by the modern blade while preserving the existing no-depth behavior for ordinary guns. Modern saber enables tight RGB dynamic lighting on level surfaces and nearby models using the saber's material color metadata. The September 23 cleanup samples three short-range lights along the blade, capped to a close player-scale radius, so the player 3DO and nearby world surfaces receive the saber hue without spreading across the room.

The installed runtime contains GLOW3.GOB, whose weapon meshes include their own
blade and fan. While Modern saber is on, the engine suppresses only GLOW3 glow
materials in its named saber meshes, preserving hands, hilt and COG behavior.
Off restores the original polyline and all legacy mesh faces. No installed GOB,
COG, model or texture was removed or edited. Unknown custom color names currently
fall back to green, following the existing saber color metadata behavior.

## Validation and native proofs

- Full Xbox release builds passed. Production geometry/history, near clipping,
  legacy mesh filtering, all effect opacity values, stock material blending,
  batched vertex streams, scalar/RGB lighting, Display-menu placement and
  first-person depth-state tests passed.
- Native menu probe checked On and Off and reloaded each saved setting with the
  expected result before the Display-menu move. The September 22 no-FMV Display
  capture attempt reached engine/loading state but stayed in cutscene/loading
  frames, so the current Display placement is covered by deterministic source
  guards until the next stable native menu capture or hardware pass.
- Inspected native XEMU captures show the white blade, swing fan, matching green
  illumination and Off fallback. The September 23 proof capture uses an explicit
  smoke-only dark inspection fixture so Kyle's face/player lighting is visible:
  the saber lights Kyle and the nearby wall/floor, while the farther background
  remains dim. Normal play keeps its existing camera and sector lighting.
  Captures are unedited.
- Four-view run verified P1's saber and green floor lighting while other views
  retained their own weapons and rendering. This was not four simultaneous
  sabers or a performance acceptance run. Two emulators ran concurrently during
  part of validation; their FPS is not a standalone performance benchmark.
- Audio was muted for these rendering tests. Original Xbox hardware remains
  untested. No host input, window capture or desktop automation was used.

Proof files under `build/xbox/modern_saber_validation/proofs/`:

- `setting.png`: original Gameplay checkbox selected and enabled before the
  setting moved to Setup > Display.
- `modern-blade.png`: white core with green edge.
- `modern-swing.png`: full visible sweep with short edge fade.
- `off.png`: existing GLOW3 rendering restored when disabled.
- `four-view-colored-light.png`: P1 saber and matching green floor illumination.

Additional September 22 proof files under `build/xbox/modern_saber_depth_display_validation/proofs/`:

- `modern-saber-dark-face-light.png`: native XEMU proof frame with a dark smoke-only fixture, visible Kyle face/player lighting, and nearby-only green spill.
- `modern-saber-dark-face-proof-summary.txt`: proof run summary showing gameplay reached and ModernSaberProbe enabled with proof mode.
- `modern-saber-tight-light.png`: earlier native XEMU gameplay frame retained as a non-acceptance comparison; it was too bright and did not show Kyle's face clearly.
- `modern-saber-depth-light-autostart-summary.txt`: earlier autostart run summary.
Additional September 23 proof files under `build/xbox/modern_saber_depth_display_validation/proofs/`:

- `modern-saber-pov-hand-depth.png`: native XEMU first-person proof frame from `ModernSaberPovProof`; the POV hands/hilt are drawn in front of the blade where appropriate instead of punching through it.
- `modern-saber-pov-hand-depth-crop.png`: enlarged crop from the same native frame for readability.
- `modern-saber-colored-world-3do.png`: native XEMU dark proof frame showing green spill on nearby world surfaces and the saber-side player 3DO.
- `modern-saber-colored-world-3do-crop.png`: enlarged crop from the same native frame for readability.

Source runs: `20260918_005429-modern-saber-persist-fixed`,
`20260918_011659-modern-saber-unmasked`, `20260918_012609-modern-saber-four-sabers`,
`20260918_012849-modern-saber-inspection` (burst frame 037), and
`20260918_012900-modern-saber-stock-final`. The misleading `four-sabers` run name
 does not mean all four players equipped sabers; only P1 did in the visual proof.
Earlier diagnostics and failed staging experiments are retained as artifacts,
not presented as successful proofs. Explicit fixture files activate the probes;
they are not part of ordinary gameplay launches.

Release deployed to `C:\Games\Emulators\CXBX\openJKDF2x\default.xbe`.
September 23 deployed proof XBE SHA-256: `991EEF2C8663AADFCEE2BC5279710092000EB2E7DF883BF933D26A22C969A769`.
This September 23 build covers first-person hand-depth composition, multi-point short-range colored saber lighting, global RGB/MotS lighting availability and the Setup > Display toggle. The previous runtime XBE is backed up under `build/xbox/modern_saber_depth_display_validation/deploy_backup/`.
The final build also covers explicit opacity in the alternative non-batched
backend; the native captures exercised the release batched backend. No commit or
push.




## October 11: boss fan investigation and rounded blade tip

The modern blade strip previously ended at a straight cross-section. A camera-facing
semicircular cap now continues its white core, colored feather and transparent outer
edge at the tip. An exactly end-on blade gets a full disk. The cap uses the existing
RGBA near-plane clipping and depth path; collision length and authored poses are unchanged.

The boss candidate uses a 50 ms history of actual relative blade poses, with interpolation
and no extrapolation. Retail Yun, Boc and Jerec keys have damage markers 4/5: the missing
fan report is not explained by missing markers. Traces show valid changing poses and fan
submissions, sometimes only a very narrow sector with one-render-frame history. The new
history addresses that render-rate dependence. A live keyframe/window guard also clears
history after an interrupted or completed attack. Player history and fan shape are unchanged.
This is a candidate explanation of the reported failure, not proof that every absent fan
had the same cause. The traced bosses did not expose a 64-bucket history collision.

Validation uses disposable stock-content stages. Earlier October 9 runs inherited GLOW3
and are excluded from acceptance. `-StockSaberAssets` removes GLOW3 archives/loose content
and, for JK, the MotS compatibility archive; each new run records mod names and hashes.
The boss inspection fixture uses a manually aimed world camera and an invulnerable player;
it never sends desktop input. Native XEMU captures are timed samples, not every rendered
frame. Inspection-camera failures are diagnostic runs, not visual acceptance evidence.

Production geometry tests pass for the rounded core/feather boundaries, end-on silhouette,
near-plane rejection, boss marker windows, interruption, retraction, death, reuse, both
blades, separate local views and 30/60/120 FPS pose history. Lighting, menu/depth, model-light
and batching regression tests also pass. No production lighting constants were changed;
the lighting test fixture was aligned with the already accepted .68/.95 constants.

### Recorded native visual reviews

These results preserve the prior chat's sequential visual reviews, checked against its
written review files and handoff. No images were opened or displayed during the text-only
documentation follow-up. Run directories are under
`build/xbox/boss_saber_validation/xemu_smoke_runs/`.

| Run | Reviewed coverage and result |
| --- | --- |
| `20261011_094434-stock-boc-cap-visible` | All 30 captures, 000-029. Both purple fans visible in 019/022, attached to their blades and absent between swings/recovery. Player green blade independently visible. |
| `20261011_094612-stock-jerec-cap-visible` | All 30. Red fans in 000/027, narrow wedge in 003, clear during recovery/idle. Player blade separately visible. |
| `20261011_094749-stock-yun-cap-visible` | All 30. Foreshortened yellow wedge in 006/019, narrow adjacent 007/020; recovery/idle 021-028 clear. Actors occlude the blade during the forward swing. |
| `20261011_095237-stock-boc-modern-off` | All 30. Stock textured purple/green blades restored. Same map/camera as On, but different AI phases and partial player occlusion; not frame-matched A/B. |
| `20261011_100006-stock-player-pov-cap-720p` | All 40. Actual 640x480. Rounded white tip and continuous green edge in 003-021, 025-026 and 031; player fans in 023/033. Foreground clutter prevents comprehensive hand-depth acceptance. |
| `20261011_100136-stock-player-third-cap-720p` | Only 000-023 of 40, per handoff. Actual 640x480. Rounded tip visible in 009-023. Camera close to wall; blade sometimes leaves frame. Captures 024-039 remain unreviewed. |
| `20261011_095739-stock-mots-kyle-arena` | All 30, per handoff. Mostly idle/power recovery; insufficient visual evidence of a complete attack. `kysnap3.key` fan submission in trace is not visual acceptance. |

The JK boss fans and rounded tip in both player views are visually supported at 480p.
Boc's retained trace had a median draw gap of 20 ms and 255 fan submissions in 281
entries; its native captures were spaced 0.153-0.182 seconds apart. These sequences do
not inspect every rendered frame or prove that every reported missing fan had one cause.
Bad-camera Boc runs and the first MotS holding-room run remain excluded.

### Final queued runs: completion and metadata only

Both runs completed, reached gameplay, recorded zero fatal events, and had XEMU alive
at their final poll. A subsequent process check found no matching XEMU/smoke process.
The native capture logs report success for every requested capture, and PNG headers were
read without displaying images. Neither sequence has been visually reviewed.

- `20261011_100442-stock-mots-kyle-attacks`: 109 seconds elapsed against a 105-second
  target; 60/60 captures at 640x480. Eleven of twelve polls succeeded; poll 010 had an
  incomplete monitor-memory reply, with later polls recovering. Kyle actor 91 attacks
  in `s5l4_lowersith.jkl`, with player/boss moved to authored checkpoint things 90/92.
  Real `kysnap1.key` tracks produce `fan=1`; a 352 ms gap resets history. Performance
  hitches remain in the log, so this is not performance acceptance or visual fan proof.
- `20261011_100631-stock-cap-native720p`: 84 seconds elapsed against a 75-second target;
  12/12 captures at **1280x720**, all ten polls successful. `XboxVideo: created` confirms
  a 1280x720 backbuffer, 720p and 16:9 with `-probe720p-sp`. The first-person fixture
  reached gameplay without fire input. This confirms mode selection, not cap appearance
  at 720p. The log also records an autosave write failure; save behavior was not validated.

Both content manifests identify the final XBE below and confirm no GLOW3 archive or
loose tree. JK retains only `xbox_patch.gob`; MotS also retains required
`mots_xbox_patch.gob`. Host audio was muted.

### Build and remaining acceptance

Final release: `build/xbox/release/default.xbe`, SHA-256
`C1B2452E5CF94A91EFEA2899E53CDDCDE7334155B317D5247CD914C927884890`.
The hash was rechecked against both final run manifests. Build log:
`build/xbox/boss_saber_mots_fixture_build.log` (`BUILD SUCCEEDED`). Earlier JK visual
evidence used `31687DBC8B5AFC4225A8E5CF5A11FD1EA3970BA3A5E8630925F0A3275679D762`;
subsequent changes were MotS fixture support, per the handoff.

The handoff records passing `test_modern_saber.py`, `test_modern_saber_light.py`,
`test_modern_saber_menu_depth.py`, `test_modern_saber_model_light.py` and
`test_lit_batch.py`; geometry and menu/depth were rerun after the final fixture changes.
The text-only follow-up changed documentation only and did not duplicate those tests.

TODO #3 stays open for conclusive MotS attack visual acceptance and the remaining
sequence review. Native interruption/retraction/death and comprehensive depth coverage
are not established by the reviewed captures; those reset cases have deterministic test
coverage. The final 720p sequence and third-person captures 024-039 remain unreviewed.
Original Xbox hardware and audio for these changes remain untested. The October 11
release has not been deployed to the installed runtime or hardware. No commit or push.

### Commit preparation

At the subsequent user request to commit and push, all five regression checks above
passed again against an isolated export of the exact staged files; the staged smoke
script also passed PowerShell syntax validation. The commit includes the previously
uncommitted modern-saber foundation and required rendering support, boss history, cap,
fixtures, tests and documentation. Unrelated MotS gameplay/menu and video-mode changes
remain in the working tree. The native release hash above describes the tested working
build, not a fresh build of this selectively staged snapshot. Native capture artifacts
remain local; the recorded visual coverage and outstanding acceptance are unchanged.
