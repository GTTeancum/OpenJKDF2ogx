# Xbox 0.9b issue 1 remediation

The authoritative unfinished-work board is [TO_DO.MD](TO_DO.MD). This file retains investigation evidence and completed history.

## September 12: gameplay audio liveness probe

Added opt-in `-AudioProbe` staging `xbox_smoke_audio.txt`. The DirectSound
backend records live buffer count, playing/looping voices, cursor movement,
status/cursor errors, missing native buffers, table-capacity drops and failed
Play calls. It does not restart or alter voices. An unchanged looping cursor
would be ambiguous because of wraparound and is not treated as a failure.

Build succeeded. Run `20260912_233414-issue1-audio-cursors` lasted 166 seconds,
with sound enabled, four local players, firing, safe authored-spawn traversal,
and a Nar Shaddaa-to-Bespin transition. All five RAM polls succeeded; fatal
count was zero and the process was alive before normal cleanup. Across 76
unique timestamped samples: at most 34 buffer entries (capacity 128), four
playing voices, and two looping voices. Status/cursor errors, missing native
buffers, capacity drops and Play failures all remained zero. Every sampled
voice with a previous playing cursor had changed position. This run does not
reproduce the original stall and does not prove audible host output is clean.

The PCM ring is cutscene-only, so its potential underrun behavior is not a
justified target for fixing the reported gameplay sound stall. XEMU's monitor
exposes `wavcapture`, but its active sound device has no registered audiodev;
the command rejected capture with `audiodev '' not found`. No WAV evidence
was obtained and host audio was not changed. Further audio work needs either
reproduction or native output evidence, not speculative voice resets.

Water source inspection confirms the underwater FOV/aspect oscillation feeds
`xbox_get_camera_params` and the GL projection. Next validation must observe
motion and the water boundary; tint alone is insufficient. The existing water
probe still uses permissive CmdWarp and must be made safe before the next run.

## September 12: accepted fixes and fresh goal (current status)

This section supersedes older validation caveats and status tables below.
The user accepts completed fixes through XEMU validation. These are done;
original Xbox testing will be one consolidated hardware run later and is
not a condition for closing these reports.

- FIXED / ACCEPTED: weapons after menus, save/load and respawn.
- FIXED / ACCEPTED: four-player firing while menus are open.
- FIXED / ACCEPTED: reproduced 17.3-second save stall.
- FIXED / ACCEPTED: game clock running too fast above 60 FPS.
- FIXED / ACCEPTED: harness outside-map teleporting on the validated route.
- FIXED / ACCEPTED: opaque forcefield when shot.
- FIXED / ACCEPTED: obscured Setup button layout.
- FIXED / ACCEPTED: crosshair, explicitly closed by the user; reopen only if needed.
- ACCEPTED: framerate. No further optimization requested.

Fresh active goal:

- PARTIAL: water surface appearance and distortion; underwater tint/projection already work.
- OPEN: surfaces flickering between lit and fullbright.
- OPEN: original long stalled sound / audio dropouts.

Assess causes, make targeted adjustments, and validate these three in XEMU.
Preserve accepted fixes; do not use blanket rollback. Keep sound enabled and
use only source, logs, native emulator capture and process-local test input.

Separate verification backlog, outside this fresh goal: brief P1 holster
spasms, exact damage fade timing, and falling-death camera presentation.
Deferred features remain HUD fonts, bots (0�8, easy/medium mix, teams),
widescreen and 720p.

## September 11: urgent traversal and weapon-state regressions

The user reported outside-map traversal, lit/fullbright flicker and repeated
P1 holstering. Do not blanket-roll back the performance changes; assess and
adjust. No transform or inline-color cache rollback has been made.

The initial item/actor route used CmdWarp's permissive sector search. Intended
and actual sector IDs could differ. An initial replacement checked every
sector boundary and body/eye clearance, but native captures from
`20260911_231705-issue1-safe-teleport` still show unacceptable clipped views
and pickup messages (Power Cells / Power Boost). All three distinct native
captures were inspected; the last shows the next level loading. This run had
no fire/input probe, so its weapon behavior cannot be attributed to simulated
weapon-switch buttons. It is not a successful visual validation.

The route now uses saved authored player spawn positions and orientations,
rejects body/eye boundary violations and nearby items/actors/active players,
and leaves the player in place when no candidate passes. It no longer lands
on pickup landmarks. Teleport callbacks now run with the destination player's
saved control/weapon state and save that state before restoring the caller.
Previously, SetContextForLocalSlot changed player/camera but left shared
weapon transients from the previous slot. A compiled production-function test
checks callback changes remain local, including nested and same-slot calls.
The boundary test also passes. The Xbox build produced an XBE.

Runtime follow-up `20260911_232604-issue1-authored-spawns` completed 158 seconds,
with four successful RAM polls, zero fatal signatures and the process alive
at the end before normal harness cleanup. Sound was enabled; no host input,
fire probe or stress-stick input was enabled. It completed the 90-second
Nar Shaddaa phase and reached Bespin. Successful polled teleports all have
matching intended/actual sector IDs. All four distinct native captures were
inspected sequentially: two Nar Shaddaa gameplay views, the Bespin loading
screen, then four-player Bespin gameplay. Gameplay captures show in-map
views, weapons drawn and no pickup messages. These are still images, so
continuous holster animation and transient lighting flicker are not certified
resolved. The revised route deliberately covers authored spawn locations;
it is not a substitute for walking through the entire level.

World submission now explicitly selects GL_MODULATE, avoiding dependence on
UI/movie texture-environment state. This is a defensive state correction,
not a confirmed explanation or resolution of the reported lighting flicker.

## User direction: stop further framerate optimization

The user considers the current framerate satisfactory and asked for the status
of the other items. Stop pursuing further FPS optimization. Finish validation
of the transform-submission change already underway, then prioritize the
remaining functional and visual gaps. This acceptance does not establish
measured 60 FPS on original hardware or implement future 16:9/720p modes.

## September 11: enabled crosshair minimum pixel width

The GPU HUD path constructs integer rectangles from the configurable float
`jkPlayer_crosshairLineWidth`. A positive fractional width below one truncates
to zero in all four crosshair arms. On Xbox the drawing path now floors the
effective width at one pixel when the existing visibility conditions permit
drawing. The visibility toggle and weapon/camera/death conditions remain
unchanged, and the stored preference is not overwritten.

The test compiles the production rectangle-construction block and verifies
fractional widths .25/.99 and nonpositive widths produce nonzero thickness,
while widths 1, 2 and 3.5 preserve their normal integer dimensions. The Xbox
build produced an XBE successfully. This fixes a reproducible configuration
failure path; the reporter's actual settings and original-hardware cause
remain unknown. No claim that this explains every missing-crosshair case.

## September 11: both fire modes covered on all local slots

The harness now labels its existing default behavior `Mixed` and supports
explicit all-primary and all-secondary selection. `Primary` stages a new
primary-only marker that suppresses the odd-port secondary selection;
production controller handling is unchanged. The menu-trigger host test and
Xbox build pass.

`20260911_230221-issue1-all-primary-menu` passes all four slots through three
menu cycles in `ram_poll_004_0091s.txt`. Every entry/min/max/exit ammo value
holds within each menu, followed by resumed consumption. Logged mode-port
pairs are secondary=0 for all ports 0–3. The run ends normally with sound
enabled, the emulator alive, and no recorded fatal errors. Together with
`20260911_225746-issue1-all-secondary-menu`, this closes both-mode hold/resume
coverage for all local slots. Remote-client menus and original hardware
remain separate unverified cases; this does not cover all weapon types.

## September 11: secondary fire on all four menu slots

Run `20260911_225746-issue1-all-secondary-menu` used the latest build with
three Escape-menu cycles and secondary fire on all four local ports. The
log explicitly records secondary=1 for ports 0–3. The four-player checker
passes snapshot `ram_poll_004_0090s.txt`: every slot's entry/min/max/exit ammo
is identical within each menu, followed by resumed ammo consumption.
Per-cycle ammo vectors are [44,44,44,44], [35,35,36,37], and [28,27,28,28].
The run completed normally with audio enabled and continued gameplay.

Together with the prior mixed-primary/secondary run, both modes now have
menu hold/resume evidence on slots 0 and 2. Primary fire on slots 1 and 3,
remote network clients, and original hardware still need separate coverage.
This is a gameplay regression check of the current build, not a performance
measurement or proof about untested weapon-switch combinations.

## September 11: reuse repeated FOV calculation

World render lists now reuse the last half-FOV tangent when the float FOV is
exactly unchanged. Viewport, aspect, clipping planes, and projection matrix
submission still update for each list; this is not a cached GL-state shortcut.
Changing FOVs (zoom, camera effects, different local cameras) recompute using
the original double-precision tangent expression and float result.

The production-helper test verifies 1000 identical requests invoke tangent
once, sweeps animated FOV values against the original formula, alternates
camera FOVs, and checks NaN is never reused. `build_xbox.bat` produced the XBE.
This proves removal of redundant trigonometry, not a measured FPS gain; no
new emulator performance comparison is claimed for this small optimization.

## September 11: signed-offset native verification

Run `20260911_225205-issue1-signed-color-capture` used the same build with
adjusted capture timing (30-second polls, capture each poll), stationary
four-player views, and sound enabled. It ended normally with no recorded
fatal errors and the emulator alive. Probe phases activated at 15027 ms and
cleared at 45025 ms.

Native images `xemu-2026-09-11-22-52-58.png` (active) and
`xemu-2026-09-11-22-53-44.png` (cleared) were inspected separately. The active
red increase/green decrease is confined to P3's lower-left world view; its
HUD retains normal colors. Clearing restores the view. A read-only pixel
comparison over P3's world rectangle x=0..319, y=240..409 matches clamped
R+96/G-64/B unchanged within two channel levels for 54344/54400 pixels
(99.897%). Other world-view regions compared are 99.804% exactly equal;
small animated/crosshair differences mean this is not a bit-exact whole-frame
claim. This verifies the tested signed effect, local isolation, and clearing
on XEMU. Original-hardware and other effect combinations remain outside
this native check; host blend-model tests cover additional combinations.

## September 11: signed-offset probe, capture gap

Added opt-in `-ColorProbe` staging and a process-local probe targeting slot 2's
camera-owned palette request: neutral for 15 seconds, add=(96,-64,0) until
45 seconds, then clear. Using the camera request avoids positive-only damage
decay erasing the negative test value. The probe checks the request index and
is disabled unless its marker file is present. The Xbox build succeeded.

Run `20260911_224913-issue1-signed-color` completed with three successful polls,
no recorded fatal errors, and the emulator alive. Logs record phase 0, active
phase at 15025 ms, and cleared phase at 45002 ms. Both native images were
inspected individually; world views and HUDs look normal before/after.
Neither image captures the active interval, so signed-offset appearance and
viewport isolation remain unverified. The next visual test must widen the
active interval or capture explicitly while the active phase is logged.
This stationary test is solely for visual comparison, not a performance run.

## September 11: signed additive color effects

The Xbox color-effect bridge previously exported tint/filter/fade but omitted
`rdroid_curColorEffects.add`. It now exports all three signed channel offsets.
World overlay rendering applies them after tint/filter and before fade, matching
the palette operation order. Positive offsets use additive blending; negative
offsets invert/add/invert to implement clamped subtraction with the existing
FakeGL blend factors. Only active offsets add passes. The existing viewport
restriction and restoration to normal alpha blending remain in place.

The production-function blend-model test exercises all 256 channel values and
offsets -300 through +300, mixed positive/negative channels, saturation before
fade, unaffected channels, and restored blending. Existing tint/filter/fade
cases also pass. `build_xbox.bat` produced the updated XBE successfully.
Native visual verification of signed offsets is still pending; the host model
does not prove hardware quantization or every in-game effect. This closes a
source-level omission, not all remaining color-effect validation.

## September 11: single final capture control

`20260911_224005-issue1-final-read-only` ran the same four-player traversal
with no periodic RAM reads or screenshots; its only RAM capture began at
153 wall seconds. Audio stayed enabled. The runner ended normally with one
successful poll, no recorded fatal errors, and the emulator alive. A separate
XEMU instance was active during this run (PID 16512, `stefx_xemu_0134`); it was
left untouched. Host contention is therefore an uncontrolled timing factor.

The complete retained m2 frame windows ending 33449–103517 ms measured
51.57–59.79 FPS, with maximum frames of 25–53 ms. No retained PerfHitch record
lies wholly inside gameplay; transition records begin at m2 completion
(110493 ms), and the 2045-ms record crosses m4 readiness at 119089 ms.
The following complete m4 window reaches 59.93 FPS with a 28-ms maximum.
The ring no longer retains every startup record, so this is not a blanket
claim that the entire process had no gameplay hitch.

The earlier isolated 325-ms gameplay hitch did not reproduce in the retained
unpolled interval. This is consistent with monitor or host overhead, not proof
of causation. Sustained four-player throughput still dips into the low 50s
without RAM polling; renderer/simulation optimization remains warranted.
Use final-only captures for performance comparisons where possible, and
preserve the host-contention limitation. No engine changes in this control.

## September 11: timed loading boundaries

Added monotonic timestamps to gameplay-ready/phase-complete and PerfHW
records, plus PerfHitch records for present intervals at least 250 ms.
These include per-frame draw/texture counts without changing rendering.
`build_xbox.bat` produced the XBE; existing pacing/arena parser tests pass.

Run `20260911_223631-issue1-hitch-boundaries` completed normally with audio
enabled and no screenshots. The first 2104-ms interval is wall time
13982–16086; m2 becomes ready at 16055, leaving only 31 ms after ready.
The 2415-ms interval on m4 is 113889–116304; m4 becomes ready at 116281,
leaving 23 ms after ready. Both large intervals span loading. Other recorded
transition intervals lie between m2 completion at 106058 and m4 readiness.
This is stronger evidence than associating a ten-second FPS window with its
nearest log marker: these multi-second intervals are not wholly gameplay.

A separate 325-ms interval at 71690–72015 occurs within m2 gameplay,
with 11 draw lists, 893 triangles, and zero texture/UI uploads. Its cause
remains unresolved; it must not be dismissed with the loading intervals.
Next isolation should compare against a run with no periodic RAM polling,
retaining a final capture to distinguish monitor overhead from engine work.

## September 11: clean capture pacing run

`20260911_223127-issue1-clean-pacing` used the corrected prompt reader
throughout, audio enabled, and no native screenshots. It finished after
155 seconds with seven polls, no recorded fatal errors, and the emulator
alive. The final snapshot contains 13 complete PerfHW windows spanning
130185 ms: time-weighted 52.694 FPS, worst frame 2442 ms, five frames at
least 500 ms. The multi-second pauses therefore persist without screenshots
and without the old slow monitor protocol.

Window sequence: initial world window 28.96 FPS / 2269-ms maximum;
eight subsequent windows 56.45–59.79 FPS / 26–49-ms maxima; transition-area
windows 33.70 / 1542 ms and 30.77 / 2442 ms; then two m4 windows
59.83 / 28 ms and 59.62 / 40 ms. The first arena completes at 90008 ms.
This points investigation toward loading and first-use work while preserving
the separate sustained-rendering shortfall. It does not prove texture uploads
cause the stalls: 73 uploads occur in one smooth window, and a transition
window has zero uploads. Explicit wall-clock endpoints on frame reports and
load-stage markers would strengthen boundary attribution before changing
texture-loading policy. No new engine build or rendering claim in this run.

## September 11: faster HMP capture and validation

The RAM reader previously waited 250 ms plus an 800-ms quiet-socket timeout
for every command, spreading a full mirror read over roughly 40 seconds.
It now waits for the completed HMP `(qemu)` prompt, handles fragmented/ANSI
responses, uses one carriage return (CRLF queued an extra empty command),
and rejects closed/incomplete replies, wrong word counts, or invalid mirror
magic. Host socket tests passed, including a response delayed beyond the old
800-ms timeout. No engine build was required.

In `20260911_222824-issue1-fast-monitor`, polls 1–3 were collected during
protocol debugging and have invalid headers: exclude them from all analysis.
Poll 4 (`ram_poll_004_0093s.txt`) has both correct magic values and coherent
clock probes. The measured interval around the header/mirror read was
0.486–0.513 host seconds, versus the former tens of seconds. The run ended
normally after 95 seconds; sound was enabled and no native screenshots were
requested. This verifies the final prompt handling against XEMU, not that
multi-second game stalls are fixed. The ring can still change during a read;
fast reads reduce that exposure but do not make an atomic snapshot.

## September 11: correction to reported FPS ranges

The 53–60 FPS ranges reported for recent traversal runs describe retained
PerfSplit windows only. `xbox_debug_ProfileFrame` drops a window whenever
the raw tick gap reaches one second; this can hide gameplay stalls as well
as intended menu/load pauses. Those ranges must not be read as an inclusive
performance baseline.

The new `tools/xbox/tests/analyze_frame_pacing.py` reports complete PerfHW
records, including long frames, using the corrected engine clock. In
`20260911_222137-issue1-four-pit-rescue/ram_poll_002_0114s.txt`, eleven complete
records cover 110076 ms and yield a time-weighted 53.281 FPS, with a worst
frame of 2277 ms and three frames over 500 ms. Individual records include
29.89 and 31.76 FPS. These records may cross loads; the 2277-ms record follows
the m4 gameplay-ready marker and can include its transition. Another 2136-ms
record occurs later in m4 gameplay. Texture uploads occur in both, but
correlation does not establish their cause. Emulator/capture overhead and
load boundaries must be separated before attributing stalls to engine work.

The analyzer's focused test passes time weighting, retaining long stalls,
zero-frame windows, and truncated-record rejection. The detailed profiler
report now explicitly warns about its discarded windows. No engine change
or new build was made for this analysis correction. Next performance work
must account for the multi-second hitches, not only optimize the retained
smooth windows. RAM-ring physical order can wrap; do not infer chronology
across the wrap or concatenate overlapping polls.

## September 11: traversal pit rescue

The traversal deaths were traced to the authored falling-death path in
`sithPlayer.c`, which sets DEAD directly when downward velocity exceeds 3
in a FALLDEATH sector. Actor damage immunity does not protect against that
path. Normal gameplay behavior remains unchanged. The opt-in traversal probe
now checks living local players each tick and warps them at downward velocity
over 2 in those sectors, before the normal death threshold. Ordinary five-second
route progression continues independently; successful rescue warps stop velocity.

The build produced an XBE. Run `20260911_222137-issue1-four-pit-rescue`
completed after 182 wall seconds with audio enabled, two successful polls,
no recorded fatal errors, and the emulator alive at the end. Its final RAM
snapshot contains seven successful pit rescues, no falling-death starts,
and m2 phase completion at 90011 ms. Seven profile windows measured
53.079–59.770 FPS. This is evidence for the tested rescues, not immunity
against every possible death or a performance improvement.

Both native captures were inspected individually. The first has mostly black
or wall-obstructed views in some slots; the second renders four views with
continued firing, including a clear central room in P2. Landmark placement
still needs camera/player clearance validation: CmdWarp tests a point inside
a sector, which alone does not establish an unobstructed camera or full body
clearance. The run therefore remains a stress workload, not a clean visual
benchmark. Existing measured world/simulation/HUD costs still guide performance
work; consistent 60 FPS has not been established.

## September 11: four-player landmark traversal

The opt-in traversal probe now visits cached item/actor landmarks for each
local split-screen player, spacing their routes across the landmark list.
It uses the player's normal context and warp command, skips missing/dead
players, restores slot 0 afterward, and leaves collision and firing active
between five-second warps. Remote-only multiplayer remains excluded. Each
warp reports its intended and actual sector; those can differ because of the
vertical offset or sector overlap, so coverage uses the actual sector.

`build_xbox.bat` produced the XBE successfully. Run
`20260911_221515-issue1-four-landmark-traversal` completed after 176 wall seconds
with audio enabled, two RAM polls, no recorded fatal errors, and the emulator
alive at the end. The second snapshot contains 17/19/19/19 successful warps
for slots 0/1/2/3, spanning 15/18/17/18 distinct actual sector IDs across the
captured maps, with no rejected warps. It completed m2's 90-second phase and
entered m4, where the route rebuilt with 31 landmarks (m2 had 16).

Five attributed m2 profile windows measured 53.898–59.416 FPS. Both native
screenshots were inspected separately: four world views and HUDs render;
the first records a P1 death message and the second shows resumed gameplay
and firing. Thus the route broadens coverage but invulnerability does not
prevent every death. It is not yet a clean sustained traversal baseline, nor
proof of 60 FPS or second-map phase completion. Investigate the death path
and collect longer route coverage before comparing optimization results.

## September 11: completed four-player phases in both arenas

Run `20260911_220225-issue1-four-two-arenas` completed both 90-second
gameplay phases in cycle 0: m2 at 90003 ms and m4 at 90013 ms, then entered
cycle 1. The runner finished successfully after 308 wall seconds, with four
successful RAM polls, no recorded fatal errors, and the emulator alive at
the end. Host audio remained enabled. This establishes sustained second-map
gameplay, rather than merely reaching its loading transition.

Unique explicitly attributed QPC profile windows measured 55.271–59.536 FPS
in m2 (six windows) and 54.601–59.950 FPS in m4 (five windows). Consistent
60 FPS is not established. The profile analyzer now attaches arena metadata
only after an explicit gameplay-ready marker and clears it on phase completion
or loading; its focused attribution test passes.

Both native captures were inspected individually. All four views and HUDs
render, but several players remain against walls. These are stress timings,
not representative coverage of either entire map. The screenshots do not
establish second-arena visual coverage. Next harness work should move players
among distinct authored landmarks so sustained wall contact does not dominate
the performance baseline. No original-hardware performance claim is made.



Source report: https://github.com/GTTeancum/OpenJKDF2ogx/issues/1



This is a work log, not a claim that the reported symptoms are resolved.

No hardware reproduction has been performed in this task.



## Requested follow-up work



- **HUD font cleanup** (requested September 9): improve the current HUD text

  and numeric readouts. Evaluate sharper font assets and rendering, scaling,

  spacing, alignment, and contrast at Xbox output resolutions and in split-screen.

  Compare before/after native captures at actual display size before choosing

  an approach; preserve the game's visual style and make the readouts easier

  to read. This is a to-do, not an implemented change.



## Four-player performance target and stress baseline



Consistent near-60 FPS (16.7 ms per frame) is now the performance priority.

Future 16:9 and possible 720p output must inform headroom decisions; neither

mode nor 60 FPS on original hardware has been verified.



Run `20260909_132029-issue1-four-player-stress` exercised four process-local

controller streams, movement through normal collision, primary/secondary

weapons, invulnerability and ammunition replenishment, three menu returns,

and progression from JK1MP m2 to m4. No host input was generated. All four

players changed position and consumed ammunition. The retained logs prove

90 seconds of the first arena and at least 40 seconds of the second, not two

complete 90-second phases. The runner remained alive and returned success;

this is not a blanket rendering/performance pass.



P0 ammunition stayed 47/42/36 respectively during the three menu pauses and

resumed falling after each return; `check_menu_run.py` passed. Native captures

showed four views with separate HUDs. The first included a bright yellow

close-up in P0; subsequent images did not. Its cause remains unclassified.

Three players later converged on the same sector with distinct positions.

The final native image is a loading screen, not four-view evidence.



Steady gameplay intervals ranged roughly 46–62 FPS, with most maximum frame

times around 29–49 ms. One interval reached 92 ms. The deliberate three-second

menu pauses and loading intervals are excluded from those figures. Available

physical memory recovered across the map transition (roughly 27–32 MiB free).

Other XEMU instances were running on the host, so these are observations of

this run, not isolated benchmark or original-Xbox timing claims. Host audio

was muted for this run; the reported audio stall remains unresolved.



## Resume checkpoint — September 9, user requested pause



Pause requested to conserve usage. The issue-remediation goal is unfinished;

no commits, publishing, or follow-up automation were created. Dedicated test

XEMU has exited; unrelated emulator instances must remain untouched.



Latest completed run: `20260909_133202-issue1-four-profile-baseline`.

198 seconds wall time, three successful RAM polls, alive at completion, no

matched fatal errors. Four active process-local controller streams; no host

input, no screenshots, host audio muted. Logs show first-arena traversal and

transition into m4, not completion of both phases. Performance varied from

prior runs; do not claim a measured optimization gain from this baseline.



Added aggregated microsecond profiling for simulation, all four world views,

POV, HUD, visibility, lights, geometry and objects. World rendering was the

largest measured scope (roughly 4.6–9.7 ms per four-view frame across retained

windows); geometry and objects dominate that scope. These are guest elapsed

timings, not isolated GPU measurements. Nested scopes must not be summed.

The newest instrumentation also measures buffer clear and outer presentation;

the engine's deferred flip alone does not measure actual presentation.



Two initial optimizations are implemented but **not runtime-benchmarked**:

- Xbox software buffer fill clips once and uses bulk fills/copies instead of

  checking bounds for every pixel. `test_vbuffer_fill.py` passed 8,000 oracle

  comparisons covering 8/16-bit colors, clipped rectangles, padding and guards.

- Per-vertex bounds/UV diagnostic scanning now compiles only when its debug

  HUD is enabled, matching the existing display guard.



Both the profiled baseline and final optimization builds succeeded. The final

XBE was regenerated at 13:36:39 on September 9 (1,576,960 bytes); no compiler

or linker errors were found in the build log. Next: run the same active

four-player plan with that build, capture

native visual evidence, compare steady timings while accounting for host load,

then optimize remaining geometry/object/POV costs. Near-60 FPS, original-Xbox

performance, 16:9 and 720p remain unproven. Audio remains unresolved. HUD font

cleanup is recorded above; bot integration remains deferred.



## September 11 validation and timing caveat

Resumed with the saved optimized XBE (September 9, 13:36:39). Buffer-fill
and process-local four-pad regressions passed again. Run
`20260911_202133-issue1-four-profile-optimized` uses the same m2/m4 plan,
four active controller streams, muted host audio and snapshot HDD. Another
unrelated emulator was active and was left untouched.

The first two native captures, `20-23-07` and `20-24-32`, were inspected
individually. Both show four rendered views, HUDs and crosshairs. The first
reproduces the large yellow shape in P0; the second shows weapon effects and
changed viewpoints. This is a recurring unresolved observation, not a clean
visual pass. Retained second-poll logs contain at least 22 distinct positions
for every controller, confirming active movement rather than an idle test.

The old profiler's summed detailed durations do not reconcile with its
10-second GetTickCount windows. Consequently, do not interpret this run's
higher reported FPS as proven real-time speed or optimization gain. Added
`tickSpanMs` and `counterSpanUs` to future profiler output to expose clock
agreement, and discard windows crossing startup or a >=1-second frame gap.
`test_profile_windows.py` passes startup/pause exclusion, independent-clock
reporting and unsigned counter-wrap cases. This changes diagnostics only;
it does not alter the game clock or physics. Runtime clock reconciliation is
the next prerequisite to a reliable performance comparison. The diagnostic
change built successfully through build_xbox.bat on September 11; its runtime
clock-span validation remains pending.

Run completion: 261 seconds wall time, three successful polls, alive at end,
no matched fatal errors; dedicated XEMU exited normally through runner cleanup.
The third native image (`20-25-52`) was also inspected: four views, HUDs,
crosshairs, weapon effects and visible other players. The summary counts six
image files because it includes three native images plus three stable-name
copies; there are only three distinct captures. Logs prove progression to m4
and at least 20 seconds there, not its full 90-second phase. P0 reached zero
health in one retained sample despite harness invulnerability (fall damage
can bypass it); do not claim a death-free soak.

## Clock validation and HUD submission follow-up

Run `20260911_202731-issue1-four-clock-validation` completed in 136 seconds,
with two successful polls, alive at end and no matched fatal errors. No other
XEMU was detected at launch. It exercised four moving/firing players; retained
logs cover approximately 60 seconds of the first arena only.

Six complete profiler windows report performance-counter spans of 13.03 to
21.84 seconds for tick-clock spans near 10 seconds. Counter-derived frame
rates are 55.37, 59.93, 59.88, 58.64, 56.48 and 58.64 FPS; the corresponding
tick-derived rates range from 76.39 to 125.50 FPS. Thus the existing PerfHW
FPS cannot be used as a real-time performance claim in this emulator run.
The discrepancy varies (ratio 1.30–2.18), rather than supporting a constant
scaling factor. Raw calculations are retained in the run's clock_analysis.json.
Performance-counter time has not independently been calibrated against host
wall time or original hardware. Neither the game clock nor physics changed.

World scope averages 6.09–10.60 counter-ms per frame; HUD 1.02–5.35 ms;
POV 1.77–4.58 ms. These timings support investigating both world submission
and UI, without treating different scene windows as paired benchmarks.

Implemented one additional bounded optimization: the two triangles of each
textured UI bitmap now share a GL_TRIANGLES submission. FakeGL's inline and
buffered paths both explicitly map that primitive to D3DPT_TRIANGLELIST, so
the old one-triangle workaround is obsolete. The original six vertices,
UVs, tint, order and surrounding state changes are preserved.
`test_ui_quad.py` passed topology, UV, color and single-batch checks.

The build succeeded through build_xbox.bat, producing a fresh XBE at 20:31:20.
Validation run `20260911_203145-issue1-four-ui-batch` completed after 175 seconds
wall time, with two successful polls, alive at completion and no matched
fatal errors. Both native images (`20-33-18`, `20-34-39`) were inspected
individually: all four HUDs, readouts, transparency and crosshairs remain
intact. The first retains the unrelated yellow P0 shape; the second shows
a different arena/view. Retained logs do not prove two complete 90-second
phases. Dedicated XEMU was cleaned up by the runner.

Seven counter-derived windows range 52.71–59.87 FPS (tick-derived 69.16–102.91).
HUD counter-ms/frame range 1.15–4.59. These are different moving workloads
from the baseline, so the change proves fewer submissions per bitmap but
NOT a measured overall FPS improvement or absence of performance regression.
Use tools/xbox/tests/analyze_split_profile.py on one complete RAM snapshot to
reproduce both clocks without silently conflating them.

Next performance work: independently calibrate the counter against host wall
time and investigate the game-clock discrepancy before altering physics or
claiming real-time/hardware FPS; profile and optimize remaining world/object
submission. The engine clock currently uses GetTickCount, so the discrepancy
may affect gameplay pacing too, but that consequence has not been tested.
Original hardware validation, audio, the recurring yellow shape, and the
remaining issue table items stay open. No commits or external posts were made.

## Host-clock calibration and gameplay pacing correction

Added a six-word sequence-guarded clock sample (tick time, counter time,
gameplay time and frame count). The existing RAM-log reader samples it before
and after its normal log copy and brackets both reads with host monotonic time.
No host input, emulator pause command or extra per-frame log I/O is involved.
`test_host_clock_probe.py` passes interval bounds, wrap, reset and torn-sample
checks; `test_profile_windows.py` also checks coherent publication.

Run `20260911_203833-issue1-clock-host-calibration` confirms the slowdown affects
gameplay, not merely displayed diagnostics:
- Sample 1: host interval bounded by 42.289–44.412 seconds; counter 43.377
  seconds; tick 24.864 seconds; gameplay 24.878 seconds; 2,520 rendered frames.
- Sample 2: host interval 42.302–44.418 seconds; counter 43.381 seconds;
  tick 30.001 seconds; gameplay 30.005 seconds; 2,484 frames.
The performance counter agrees with host elapsed time within read uncertainty;
the engine's old tick-based timer does not. These are emulator observations,
not proof of the same kernel-clock behavior on original hardware.

Implemented a shared performance-counter clock for stdPlatform_GetTimeMsec
and Linux_TimeUs, retaining a common epoch, monotonic results and a GetTickCount
fallback if counter initialization is unavailable. Controller timestamps and
the HostServices getTimerTick callback now use the engine clock too. The
conversion avoids long-uptime multiplication overflow. Game simulation rules
and timestep limits were not changed.

`test_xbox_clock.py` passes slow-tick pacing, shared epoch, backward/failed
counter handling, long uptime, millisecond wrap and unavailable-counter fallback.
Existing gameplay-entry clock, menu-trigger and four-pad tests pass. The first
build attempt caught an XDK header-order conflict in stdControl_xbox.c; the
include was moved after XTL/engine types. The corrected build succeeded and
produced a fresh XBE at 20:43:55.

Run `20260911_204408-issue1-realtime-clock-menus` completed in 172 seconds,
with two successful polls, alive at completion and no matched fatal errors.
The first host bracket spans 42.449–44.564 seconds; counter time advances
43.510 seconds and gameplay advances 43.360 seconds. This supports corrected
real-time pacing in this run, unlike the previous 24.878/30.005 game seconds
per approximately 43.4 host seconds. It is not a promise about every load,
menu, save or hardware situation. The second bracket was rejected because
a map/game-clock reset crossed it; it is not valid calibration evidence.
The first bracket's 2,178 frames bound host FPS at 48.87–51.31. Other retained
counter windows reach 55–60 FPS, so consistent 60 still needs optimization.

`check_menu_run.py` passed all three cycles: ammo 50->50 (resumed 48),
48->48 (resumed 47), and 47->47 (resumed 41). Both native captures
(`20-45-36`, `20-46-59`) were inspected: four world views, HUDs, crosshairs,
and weapons remain visible. The latter also shows repeated Shield Recharge
text; message placement/duplication has not been investigated. Logs retain
phase-one progression through 82.56 seconds and the following load, not
complete coverage of both phases. Dedicated test XEMU exited.

Renderer FPS/hitch logging and optional main-loop timing were subsequently
routed through the corrected engine clock too. Raw GetTickCount remains in
the dual-clock calibration diagnostic deliberately. This final logging-only
adjustment built successfully through build_xbox.bat; runtime confirmation of
its reported FPS remains pending. Next: verify single-player save/respawn with the corrected
clock, then optimize measured geometry/object costs at correct gameplay speed.

## Single-player regression check and user-observed speed changes

Run `20260911_204934-issue1-clock-sp-save-respawn` exercised level-one
landmark traversal, firing, deliberate death, respawn and native save/load.
It completed in 174 seconds with two successful polls, alive at end and no
matched fatal errors. Logs confirm death at game-ms 15002, subsequent respawn
with health 100, native roundtrip success, then ammunition 50->44->38->32->26
and continued traversal. This combined run proves firing after both operations;
it does not isolate an independent firing interval between respawn and the
immediately queued native save/load.

There was a logged 18,145-ms maximum frame gap during loading. This is a
significant stall, even though the run recovered; do not describe it as smooth.
Both native captures (`20-51-05`, `20-52-27`) were inspected. The first is a
dark close-up with weapon/HUD and repeated Shield Recharge messages; the
second shows another sector and fists after ammunition exhaustion.

The user reported apparent speed-up above 60 FPS and a possible freeze, then
qualified that it might be perception after seeing a flat 30-FPS interval and
later faster motion. Treat the freeze as unconfirmed, the recovered loading
stall as evidence, and the speed-up as unresolved until measured in single
player. Earlier host calibration covered split-screen only. Extended clock
publication to the single-player render path for an uninterrupted traversal
run without deliberate death/reload/menu pauses.

Also corrected a definite audio error path: stdSound_IsPlaying now initializes
status and returns false if DirectSound GetStatus fails. Previously it could
read undefined output. test_sound_status.py passes playing, stopped, failed
query and missing-buffer cases. The new build succeeded; this does not prove
the user's stalled sound is fixed, and host output remains muted in tests.

## Single-player clock measurement above 60 FPS

The pending build completed successfully and included single-player clock
publication and the DirectSound status-error fix. Run
`20260911_205355-issue1-sp-clock-isolated` used traversal and firing without
scripted death, reload or menus. It completed after 139 seconds, with two
successful polls, alive at completion and no matched fatal errors. No native
captures were requested for this timing-only pass. Test XEMU exited.

The first clock bracket began before gameplay publication and was rejected.
The second valid bracket measured 3,014 frames over host-time bounds of
42.338–44.458 seconds, giving 67.79–71.19 host FPS. Counter time advanced
43.383499 seconds and gameplay time 43.375 seconds. Raw GetTickCount advanced
only 27.642 seconds. This sample does not reproduce game-clock acceleration
above 60 FPS; it supports the corrected counter clock at that workload.
It does not rule out all frame-dependent animation/movement defects or every
possible frame rate. Retained corrected PerfHW gameplay windows ranged
60.51–89.28 FPS, with movement and firing continuing.

The earlier approximately 18-second respawn/load stall remains a separate
performance issue. Do not dismiss it based on successful recovery. Single-player
PerfSplit output now publishes frame/clock data; its world/POV/HUD top-level
scopes are not yet instrumented there, so zero values are not zero work.

## Isolated respawn/load timing (September 11)

Added counter-clock elapsed/stage milliseconds to the existing SaveLoad traces
and a separate SaveWrite duration. The XBE was rebuilt through build_xbox.bat
(21:02:57); the real JK save-callback link-map check passed.

`20260911_210311-issue1-respawn-stage-timing` exercised traversal, firing and
lethal respawn without a second save round trip. It completed in 142 seconds,
with two successful RAM polls, alive at completion and no matched fatal errors.
The legacy save restored in 194 ms: episode resources 30 ms, episode setup
53 ms, saved packet restoration 98 ms, other stages small. The containing
gameplay window's maximum frame was 211 ms. Post-respawn ammo decreased
50 -> 44 -> 38 -> 32, with traversal continuing past 60 restored game seconds.
This independently validates post-respawn firing without an intervening reload.

The prior 18-second stall was not reproduced by respawn alone. That combined
run also queued a new save immediately afterward; a separate save round-trip
measurement was needed before attributing its stall to restoration. Host audio
was muted and no visual captures were requested in this timing-only run.

`20260911_210553-issue1-save-stage-timing` then isolated the native save round
trip without death. It completed in 142 seconds, two successful polls, alive
at completion and zero matched fatal entries. SaveWrite measured **17,302 ms**;
the containing maximum frame was **17,312 ms**. The subsequent native save
load took **194 ms**, including 106 ms restoring packets. The resumed-success
marker recorded curMs=20352, and gameplay/traversal continued past 81 seconds.
This localizes the reproduced long stall to writing the save, not loading it.
It does not yet separate serialization CPU work from disk I/O. Source shows
sithComm_FileWrite emits each packet as three small writes, each reaching an
unbuffered platform fileWrite call (no application-level write aggregation).
Measure/optimize that path next, preserving save bytes and error handling.
No emulator from these tests remains running; no audio or visual claim follows
from these timing-only tests.

## Save write batching (September 11)

The Xbox save serializer now opts into a 16 KB sequential buffer in
stdConffile. Packet headers and payloads are concatenated without changing
their bytes or order, then sent to the platform in blocks. Other config
writers retain immediate writes. Final flush occurs before closing the save;
a short write latches failure, suppresses later writes, and prevents updating
the autosave name or announcing GAME_SAVED. State resets on the next open.
This uses 16 KB static storage and does not change the save format.

`test_save_write_buffer.py` compiles the production buffer/open/write/close
functions against an exact-byte sink. It passed variable small packets,
empty writes, a large payload spanning buffers, partial tail flushing,
immediate non-save writes, full-buffer and final-tail short writes, reopen
after error, and open failure. The XBE rebuilt successfully at 21:11:46 and
the real JK save callback link-map check passed. Runtime timing is required
before claiming that batching resolves the measured stall.

Runtime result: `20260911_211203-issue1-save-buffered` completed in 140 seconds,
two successful polls, alive at completion and zero matched fatal entries.
The same save/traversal/firing probe measured **170 ms saving**, versus
**17,302 ms before batching** (about 99% less time). Its containing frame
maximum was 184 ms. Loading the native save took 252 ms; the round-trip
resumed-success marker was curMs=20471. Firing and landmark traversal continued
through 89 game seconds, with post-load ammo 32 -> 26 -> 20 -> 14. A later
window covering reload/recovery still had a 532-ms maximum frame, so this
does not eliminate every hitch. This demonstrates removal of the reproduced long
save-writing stall in this emulator case; it does not establish constant
60 FPS, instantaneous loading, or original-hardware timing. No screenshots
were needed for this byte/timing change; host audio remained muted.

## Audio output clarification (September 11)

The user reported silence during performance work. Prior smoke runs explicitly
passed -MuteHostAudio, which selects SDL's dummy audio driver for the launched
XEMU child only; the launcher restores both environment variables afterward.
No XEMU process or persistent driver override remained at the time of checking.
Run `20260911_211931-issue1-four-submit-audio-enabled` omitted that switch.
The user then confirmed **"Sound is back"** during the run. Audible playback
is therefore user-confirmed with muting removed. Keep host audio enabled in
subsequent tests unless specifically requested otherwise. This does not prove
the earlier looping/stalled-sound symptom is fixed or rule out an intermittent
audio problem.

## Four-player submission profiling (September 11)

`20260911_211547-issue1-four-counter-baseline` completed in 142 seconds with
two successful polls and no matched fatal errors. Five complete QPC windows
measured 53.84–59.36 FPS, world 6.69–10.96 ms/frame, POV 1.97–4.20,
HUD 1.38–3.28, and simulation 2.00–3.38. First-arena activity through roughly
79 game seconds is retained; this is not completion of both arena phases.

Added XPROF_SUBMIT around nonempty std3D_DrawRenderList submissions to
separate geometry preparation from the FakeGL submission path. This scope
overlaps world/POV and their subscopes: never add it to them. It measures CPU
elapsed submission time including any waits, not independent GPU time.
The profiler window test passed and build_xbox.bat linked a fresh XBE at
21:19:12. A comment-only cleanup removes the stale warning against triangle
batching; existing batching behavior was not changed.

`20260911_211931-issue1-four-submit-audio-enabled` completed in 140 seconds,
two successful polls, alive at completion and no matched fatal errors. Five
complete QPC windows measured 52.31–58.21 FPS. Submission cost was
3.81–6.96 ms/frame, world 6.18–10.62, POV 2.42–3.97, HUD 1.70–3.66,
simulation 2.48–3.31. A later 10-second PerfHW window fell to 42.22 FPS
with a 77-ms maximum frame: consistent 60 FPS remains unmet. This run
continued four-player movement/firing through roughly 77 seconds of the first
arena, not both phases. Sound was user-confirmed audible (see above).

World rendering remains the largest measured category. Submission is a
significant portion, but geometry/thing preparation also consumes material
time; investigate those costs before assuming further draw-call batching
alone can achieve the target. These are moving-scene emulator observations,
not hardware benchmarks or proof of performance regression from audio.

## Texture sorting and forcefield report (September 11)

Replaced unrelated-pointer subtraction in the triangle and ngon texture
comparators with explicit uintptr_t ordering and normalized format grouping.
The production comparator test passed null keys, separately allocated keys,
antisymmetry, transitivity and sorting. The Xbox build succeeded at 21:26:15.
Run `20260911_212646-issue1-four-texture-sort` completed in 178 seconds with
two polls; both native captures (21:28:18 and 21:29:44) were inspected.
Four views/HUDs were present, but the first showed the existing opaque yellow
forcefield. No FPS improvement is claimed from the comparator correction.

The user identified the prior yellow close-up as Nar Shaddaa Loading Terminal's
forcefield turning opaque when shot and supplied a confirming screenshot.
Read-only inspection of JK1MP.GOB's m2.jkl and m2_ffieldswitch.cog identifies
surfaces 954/962, material 235 (00_YellowCued.mat), translucent face flags 0x2,
and a 0.5-second visible interval after damage. The current Xbox code used
GL_ONE source blending on straight colors. The PC shader applies inverse-alpha
weighting before using GL_ONE, so copying only its blend state saturated yellow.

Added a face blend helper using inverse-source-alpha/source-alpha weights for
flag 0x200 and straight-alpha blending otherwise. The host test covers both
textured and solid faces across all alpha byte values. Added alpha-test cutoff
handling so zero-alpha texture holes are discarded before inversion; that
follow-up passed the host test and awaits the final build after the live run.
Native forcefield validation is in progress; do not call the defect fixed
solely from the blend-equation test.

`20260911_213226-issue1-forcefield-blend` subsequently completed in 89 seconds,
one successful RAM poll, alive at completion and no matched fatal entries.
The native 21:33:54 capture was inspected: the upper-right field is now a
muted yellow overlay with underlying scene detail, rather than the previous
saturated yellow block. All four views/HUDs remain present and firing continued.
This verifies the blend correction visually on the reported stock arena.
The final alpha-cutout safeguard was built afterward and passed the host blend
test; textured cutout variants have not yet had separate visual coverage.

## Final blend build: underwater follow-up (September 11)

The opt-in WaterProbe now looks upward at 45 degrees in water and restores
level pitch on dry return. Normal gameplay is unaffected. build_xbox.bat
completed successfully with the final face-blend alpha-cutout safeguard.
`20260911_213845-issue1-water-surface-blend` completed in 169 seconds with two
successful polls, alive at completion and zero matched fatal errors. Audio
was enabled. Logs prove entry into sectors 447 and 449, an intervening dry
return, changing underwater FOV/aspect, and dry reset to aspect .75/tint zero.

Both native captures (21:40:14 and 21:41:34) were inspected individually.
They show upward underwater views, preserved HUD/crosshair and, in the second,
bubble sprites without opaque rectangular backgrounds. They do not clearly
isolate the water surface: nearby geometry obstructs the view. Do not promote
this run to a water-surface-transparency pass. The next surface probe should
target an authored wet/dry adjoin directly, rather than a sector center.
Custom Jedi High School water comparison and broader cutout coverage remain
open. The issue-status table was refreshed to include repeated menu tests,
the corrected clock, save batching and current four-player performance gaps.

## Authored liquid-boundary probe (September 11)

WaterProbe now selects horizontal wet-to-dry adjoins with downward wet-side
normals. It computes the polygon center, validates a warp just below/behind
that point, looks upward, and disables gravity only during wet observation.
The original gravity bit is restored on dry return. This replaces blind
sector-center placement that sank to the floor. Boundary IDs, material pointer,
face type and dry-sector ID are logged. The Xbox build passed.

`20260911_214503-issue1-water-boundary` completed in 178 seconds with two
successful polls, alive at completion and no matched fatal entries. It reached
surface 4418 (wet 511, dry 512) and surface 4439 (wet 513, dry 514), both with
face type 0x2 and a material, with a dry return between them. Their authored
wet tint is (0.6, 0.4, 0), explaining the orange appearance; these are different
liquid sectors from the earlier blue-water probe.

Both native captures, 21:46:34 and 21:48:00, were inspected individually. The
second clearly includes the liquid polygon overhead, while the first is more
obstructed. This establishes rendering of the targeted stock liquid surface;
it is not a calibrated opacity/original-game comparison. A clearer background
comparison and the custom Jedi High School case remain unverified. The test
instance exited normally and audio stayed enabled.

## All local slots: repeated multiplayer menu validation (September 11)

The opt-in Escape-menu probe now records entry, minimum, maximum and exit
ammo for every local slot, sampled on each menu tick. The new
check_four_player_menu.py requires all four records in each requested cycle,
positive unchanged ammo throughout each menu, and post-return consumption
in every slot. Its host regression rejects transient changes, missing slots,
absent resumption and incomplete cycles. The Xbox build passed.

`20260911_215108-issue1-all-slots-menu` completed in 140 seconds with two
successful polls, alive at completion and zero matched fatal entries. Three
seven-second gameplay intervals led into three-second Escape-menu visits.
Slots 0/2 used primary input and 1/3 secondary input through the process-local
controller harness. The final snapshot passed all twelve slot/cycle checks.
In-menu ammo was [44,44,44,44], then [35,35,36,37], then [28,27,28,29];
each slot's minimum, maximum and exit equaled its entry. Each resumed firing
after every return. Movement/firing continued beyond 71 game seconds.
Audio stayed enabled and the test instance exited normally. Remote network
clients and testing both fire modes on every individual slot remain separate
coverage, not claims made by this run.

## Inline vertex color submission (September 11)

Source review found that public FakeGL entry points already suppress unchanged
texture, blend and texture-environment state; adding checks inside the private
setters would duplicate existing work. The inline vertex path, however, emitted
SetVertexData4f for every glColor4f even when flat-lit faces repeated the color.

The USE_BEGINEND path now skips identical float-color writes within a batch.
It invalidates the cache on Begin, byte-color calls, Release and Initialize.
Colors including alpha are compared exactly; NaNs do not suppress writes.
Position and texture-coordinate submission are unchanged. The host test runs
the production inline class against a recording device: 300 vertices retained
their expected colors with 10 color writes, and boundary/alpha/reinitialization
cases passed. build_xbox.bat linked a fresh XBE. Runtime measurement and native
visual checks are required before asserting a gameplay benefit.

Runtime validation: `20260911_215801-issue1-four-color-cache` completed in
176 seconds with two RAM polls. Both native captures (21:59:32, 22:00:56)
were inspected: four views/HUDs, textured lighting, colored players/weapons
and the corrected forcefield appearance remain intact in these views.
Six complete QPC windows measured 55.57–59.66 FPS, with submission
4.71–5.56 ms/frame and world 6.36–10.20 ms/frame. The run progressed through
the first arena to the m4 loading transition. These moving-scene observations
do not isolate an FPS gain from this optimization or establish constant 60 FPS.
The host test establishes the reduction in redundant writes for repeated-color
input; broader hardware performance remains open. Audio stayed enabled.

## Changes and evidence



- Gameplay entry now refreshes the frame clock without resetting simulation

  time. Previously `jkMain_GameplayShow` called `sithTime_Startup` even on menu

  return, rewinding time while weapon cooldowns and COG deadlines remained

  absolute. The native clock regression exercises production clock code and

  the gameplay-entry calls with menu and loaded-save timelines. This does not

  prove that every reported firing failure has the same cause.

- Explicit Xbox footer bindings hide bottom-row controls, matching the default

  footer behavior. General Setup also binds Advanced to Y. Original controls

  extend to y=470 and the footer begins at y=440. Footer handlers deliberately

  accept hidden controls, so hiding the originals preserves their shortcuts.

- Physics catch-up warnings now aggregate counts and peak values over five

  seconds. The report's log contains repeated overruns during roughly 10 FPS

  gameplay, and the Xbox logger uses synchronous write-through I/O. The native

  regression verifies bounded writes, aggregation, and platform-clock wrap.

  Performance improvement still requires measurement on the same scene.

- `build_xbox.bat` had mixed line endings that allowed compilation but skipped

  linking/post-processing while printing success. The working copy is normalized

  and `.gitattributes` requires CRLF for batch files. Verify an actual link,

  successful image generation, and a fresh XBE, not just the success banner.

- Trigger polling now publishes released fire bindings outside gameplay, even

  when a physical trigger is held across menu entry. A native regression runs

  production polling statements and key-edge tracking through held-trigger menu

  entry, presses within a menu, and return to gameplay. Full weapon/COG behavior

  in a multiplayer game remains unverified.

- Save loading emits a few phase markers through the normal and performance

  log paths. These locate failures between episode loading, runtime teardown,

  packet restoration, and completion; they are diagnostics, not a crash fix.

- Xbox now applies tint/filter/fade to the rendered world before the HUD. The

  tint factors match `stdPalEffects_ApplyTint`; separate darkening and brightening

  passes preserve factors above one, followed by fade after saturation. The

  previous vertex fade is disabled on Xbox to avoid applying it twice. Native

  tests model the emitted GL blends, including full death fade and neutral

  no-op. Visual appearance and split-screen effect ownership need runtime checks.



- Split-screen now allocates separate damage/sector palette requests for each

  local player and gathers effects per view, excluding the other players'

  requests while retaining global requests. The camera's newly discovered

  sector tint is gathered again before the world overlay. Production masked

  aggregation tests cover sparse requests, global fade/add effects, all-excluded

  neutral output, and unchanged unmasked aggregation.

  Native capture `20260909_123122-issue1-split-damage/screenshots/xemu-2026-09-09-12-32-37.png`

  under the smoke-run directory shows player 0's upper view red and player 1's

  lower view neutral gray, with independent HUDs. The ordinary damage probe

  logs health 75 and firing consumes ammo. This verifies damage isolation in

  this two-player case, not water transitions or damage received by slot 1.

  The run ended normally after 145 seconds. The second native capture at

  `12-33-46` was also inspected and retains the same per-view isolation while

  player 0 punches after using up ammunition. Red P1 was caused by the opt-in

  repeated-damage probe; the user asked about it, and this was explained.

  Audio output was muted for diagnostics, not fixed or disabled in game code.



Damage-recipient follow-up: `sithActor_Hit` applied a flash only when the

damaged thing equaled the global local-player thing. Collision hits outside

the recipient's control tick could therefore omit P2â€“P4 feedback. Xbox now

dispatches to the damaged local player's palette request directly, preserving

the current gameplay context and excluding remote players. The production

dispatcher test covers both local recipients, saturation, unchanged green/blue

channels, remote exclusion and single-player fallback. `-DamageProbeSlot 1`

selects P2 for a native check; ordinary probe default remains P1 (slot 0).

The test also covers dispatch to slots 2 and 3 in a four-local-player setup.

The Xbox build linked successfully at 13:06 on September 9 after correcting

the test-slot parser's unavailable `atoi` call. Run

`20260909_130718-issue1-p2-damage` ended normally after 176 seconds with two

successful RAM polls. Logs show slot 1 health 75 and P1 firing until ammunition

is exhausted, followed by fists. Both native images were inspected individually:

`13-08-50` shows P2's lower world red, P1's upper world neutral, and HUDs with

their original colors. `13-10-13` shows the normal loading screen at the plan's

next level load, so it supplies no damage-tint evidence. The first capture

verifies damage ownership in both directions when combined with the earlier

P1-damage run. Repeated damage

and muted audio were explained before launching. This is a targeted visual

check, not an active-traversal performance baseline.



## Remaining symptom validation



Fall run `20260909_124341-issue1-fall-visual` completed after 150 seconds,

with two successful RAM polls and the emulator alive at shutdown. The

process-contained probe warped into authored fall-death sector 15 and applied

downward velocity; it did not set death or fade flags. Both native captures

(`12-45-01` and `12-46-11`) were inspected individually: the world is black,

the HUD remains visible, and no restart prompt is visible. This verifies the

final black overlay, not the camera transition or successful fall respawn.

The user asked about remaining on this screen; this run deliberately had no

fire/respawn input. Its black-screen FPS is not a performance baseline.

Fall begin/completion logs were compiled out; they now use the retained event

logger for the next build. `test_fall_sequence.py` passes authored pit detection,

noclip exclusion, camera changes, 1.44-second fade at 20ms steps, prompt timer,

and the multiplayer death branch using production sequence code. Native

prompt/respawn verification remains necessary. Stock `03katarn.jkl` contains

24 authored underwater sectors, providing a local target for water checks.



Follow-up source checks found two distinct points: `jkDev_PrintUniString`

expires messages after five seconds, so these late screenshots cannot prove

that a prompt was never issued. Separately, fall detection accepted already-dead

players once `FALLING_TO_DEATH` cleared; continued downward motion could restart

the sequence and reset its deadline. The extended production-code test failed

when simulating continued descent after completion. Detection now excludes

dead players; the same test passes and preserves the original prompt deadline.

Begin/completion and prompt-issued events are retained for runtime validation.



Build with the dead-player guard linked successfully at 12:49 on September 9.

Run `20260909_124935-issue1-fall-respawn` shows authored pit entry and fall begin

in its RAM snapshot. Its native capture at `12-50-56` was inspected: gameplay

has returned to the spawn area, health is 100, the crosshair is visible and a

Bryar shot is visible (ammo 43). However, the run finished after 82 seconds with

only one RAM poll, whose snapshot preceded fade completion. Its stricter fall

verdict therefore correctly failed for missing completion evidence. A longer

run is required to capture the event sequence; do not call the exit-1 result a

complete fall validation.



Run `20260909_125252-issue1-fall-events` completed normally after 136 seconds,

two successful RAM polls, and no matched fatal entries. The retained log shows

fall begin, fade completion at simulation time 6430ms, prompt issuance at

9435ms, normal legacy-autosave restoration through `SaveLoad: complete`, and

post-respawn firing reducing ammo 50 -> 44 -> 38 -> 32 -> 26 -> 20 -> 14.

This validates fall-to-restart and weapon recovery in XEMU. The camera motion

through the short fall transition and original-game visual comparison remain

unverified; no black-screen performance result is used as gameplay evidence.



Opt-in `-WaterProbe` alternates stock authored underwater sector centers and

the starting position every eight seconds, with ordinary invulnerability and

normal warp validation. It logs camera water state, tint, FOV and aspect once

per second. Its runner requires water entry, camera underwater state and a

return to spawn; these are traversal checks, not a visual-effects verdict.



Run `20260909_125524-issue1-water-traverse` completed normally after 180 seconds

with two successful polls. The later snapshot contains five authored water

entries (447, 449, 451, 463, 467), four returns to spawn, 34 underwater camera

samples and 40 dry samples. Underwater FOV ranges 105.0406â€“106.9761 degrees,

aspect 0.73333â€“0.76666, and sector tint is (0.4, 0.8, 1.0). Dry samples reset

to FOV 106.0158, aspect 0.75 and zero tint. Both native images (`12-56-53`,

`12-58-23`) were inspected individually and show the dry starting area with

normal colors and a crosshair. Thus camera modulation and dry-state reset

are verified, but underwater rendered appearance and surface rendering are

not: the capture timing missed wet phases. Use a longer wet phase for the

next native capture rather than claiming visual coverage from these images.



Run `20260909_130115-issue1-water-visual` uses a 60-second wet phase and an

eight-second dry phase. It ended normally after 179 seconds with two successful

polls, authored water entry, animated underwater camera parameters, and dry

reset. Both native images (`13-02-48`, `13-04-13`) were inspected individually:

they show underwater level geometry with a blue/green cast, the weapon darkened

with the scene, and readable untinted HUD/crosshair. This establishes stock

underwater rendered coverage, alongside the previous dry captures. Camera

distortion is supported by time-varying logged projection values; these still

images do not establish its motion quality. Water-surface behavior and the

Jedi High School map comparison remain open.



Additional user observation (2026-09-09): the multiplayer diagnostic run

produced one long stalled/repeating sound. Treat this as an unresolved failure,

not a passed audio check. The first MP attempt (`20260909_121203-issue1-mp-menu`)

lost its monitor connection before a complete RAM snapshot; PowerShell's native

stderr handling then aborted the runner. The launcher now retains emulator

stdout/stderr and allows a failed poll to reach its failure branch. The retry

uses `-MuteHostAudio` (child-only SDL dummy output, restored launch environment)

while retaining game audio processing. This silence is not an audio fix.



Audio source triage: the PCM ring stream's callers are in `jkCutscene.c`;

ordinary gameplay uses individual DirectSound buffers in `stdSound_xbox.c`.

The reported gameplay stall therefore needs buffer play/stop/status and cursor

evidence before attributing it to the cutscene ring's underrun behavior.



The muted MP retry (`20260909_121440-issue1-mp-menu-muted`) reaches two-player

`JK1MP/m2.jkl`, opens/returns from Escape, and advances beyond 74 simulation

seconds. However, ammo remains 50 through probe presses before and after the

menu. Thus the run does not establish that actual weapon firing stops in menus;

the split-screen input/weapon state path needs diagnosis first. The ring-buffer

audio backend also needs underrun/liveness investigation before attributing the

reported sustained sound to a specific cause.

The retry finished normally after 168 seconds, three successful snapshots,

and a completed 120-second MP phase followed by level reloading. Its exit code

0 is only a liveness verdict; unchanged ammo prevents a weapon-firing pass,

and dummy output prevents any audible-quality verdict.



| Reported symptom | Current status / next evidence |

| --- | --- |

| Weapons lock after menus | Clock defect fixed; repeated three-cycle Escape-menu tests pass in XEMU, including September 11 after the clock correction. Weapon-switch combinations and hardware remain unverified. |

| Weapons fail after save load | Restored real JK save callbacks and legacy compatibility; old-save reload and new-save round trip both resume with sustained firing in XEMU. Hardware validation remains. |

| Weapons fail after respawn, Xbox crashes | Stubbed JK callbacks caused missing player associations. Normal lethal damage, fire-button respawn, and subsequent firing pass in XEMU; hardware reproduction remains unverified. |

| Inconsistent performance | Catch-up log flooding removed; counter-clock calibration fixed emulator timing; save batching reduced the reproduced 17.3-second stall to 170 ms. Four-player world/submission profiling still shows sub-60-FPS intervals. Geometry work, broader stress coverage and hardware timing remain open. |

| Crosshair missing | Not reproduced in opening-level XEMU test: native screenshot `build/xbox/issue1_validation/xemu_smoke_runs/20260909_103029-issue1-world/screenshots/xemu-2026-09-09-10-33-17.png` shows a thin crosshair. Reporter settings/hardware remain unverified. |

| Setup buttons obscured | Native General Setup capture verifies a clear footer with A Select, B Back, Start Done, Y Advanced and no overlapping old buttons. In-game shortcut activation remains unverified. |

| Damage feedback missing | World tint pass implemented and blend math tested. Native damage/traversal capture shows red world/weapon tint with HUD and crosshair preserved; exact fade timing and hardware appearance remain unverified. |

| Water effect missing | Stock level 3 logs and native captures verify underwater tint, changing camera projection and reset on exit. Surface rendering, distortion motion quality and Jedi High School comparison remain unverified. |

| Falling-death presentation | World fade and repeat-trigger guard implemented. XEMU logs verify fade completion, prompt issuance, save restoration and post-fall firing. Camera motion and comparison with original presentation remain unverified. |

| Multiplayer menu permits firing | Separate all-primary and all-secondary runs each pass three menu cycles on all four local slots: ammo holds throughout each menu and firing resumes afterward. Remote network clients, other weapon types, and original hardware remain unverified. |



Split-screen follow-up: `xboxSplitScreen_TickPlayer` now runs each local

player's update in its own context and saves the resulting weapon/control

state. Previously only control handling saved that state; later player ticks

completed weapon selection, but rendering restored the old pending selection,

preventing sustained firing. Remote and non-split ticks retain their path.

`test_split_player_tick.py` checks state persistence across both local slots,

context restoration, remote-player isolation, and non-split behavior.



Run `20260909_122013-issue1-mp-state-fix` confirms actual firing after this fix:

ammo 50 -> 47 before Escape, 47 across the menu, then 41 -> 35 after returning.

Continued bursts empty the weapon and automatically select fists. The run

ended normally after 115 seconds, two successful snapshots, and no matching

fatal log entries. This covers one primary-fire menu cycle for slot 0;

secondary fire, repeated menus, other slots, and remote simulation during the

menu still need direct checks. Audio remained muted and is still unresolved.



The final `cogStrings.uni` warning in the attachment also occurs earlier during

successful loading; it is not by itself a crash diagnosis.



## Local checks



General Setup visual check: run `20260909_123624-issue1-setup-visual` used the

opt-in `-SetupProbe -OpenEscapeAfterSeconds 4` path to dispatch Escape -> Setup

-> General inside the game. Native capture

`screenshots/xemu-2026-09-09-12-37-52.png` was inspected at 640x480: all four

footer actions are visible and correctly spaced; the original Cancel/OK/Advanced

bottom-row controls no longer overlap the footer. The run ended normally after

89 seconds. No host input was used, no damage probe was active, and output was

muted. The probe chooses menu return IDs; it does not test physical shortcuts.



Damage visual probe: `-DamageProbe -Traverse` applies normal `sithActor_Hit`

damage once per second while traversing authored landmarks. It restores health

before each hit to sustain coverage; it does not directly set a palette effect.

Run `20260909_122529-issue1-damage-visual` logs health 75 after hits and successful

sector transitions. Its first native XEMU image,

`screenshots/xemu-2026-09-09-12-26-43.png`, visibly shows red world/POV tint and

preserved HUD colors/crosshair. The scene is dark, so it is limited evidence

for tint intensity, not a precise comparison with original-game appearance.

The stable copy is named `shot_0016s.png`, but that elapsed value is the poll's

start, not capture time; use the native filename for capture timing.

The second native image, `screenshots/xemu-2026-09-09-12-27-53.png`, shows the

red tint more clearly on illuminated wall textures and the weapon, with the

HUD/crosshair unaffected. Both captures were individually inspected. The run

ended normally after 145 seconds; the logs include ten successful landmark

warps and continued ordinary damage. This does not test split-screen ownership.



Jedi High School is absent from the local Episode archives. A legacy download

listing was found, but the original link timed out and the archive mirror

returned a browser challenge. No map was downloaded. Stock-water testing can

continue independently. Source shows water uses both sector tint and animated

camera FOV/aspect, so tint alone cannot establish resolution of the report.



Run from the repository root:



```powershell

python tools/xbox/tests/test_gameplay_clock.py

python tools/xbox/tests/test_catchup_logging.py

python tools/xbox/tests/test_menu_triggers.py

python tools/xbox/tests/test_color_effects.py

cmd /c build_xbox.bat

```



Native tests require Clang and execute only local harness processes. Build output

is `build/xbox/release/default.xbe`; the task's build log is

`build/xbox/release/issue1_build_log.txt`. No deployment is performed.



The isolated XEMU test uses a dedicated instance directory, `-snapshot` disk

writes, and disabled host input. Native capture succeeded; monitor capture in

the first run failed because its helper sent LF instead of CRLF (now corrected).

The opening-level run is a stationary smoke test, not hardware performance

qualification. An opt-in `ReloadAfterSeconds` harness flag stages a file that

requests the normal deferred autosave load inside the game for crash diagnosis.



Reload run `20260909_103711-issue1-reload` failed: the user observed a frozen

display. The last successful RAM snapshot reaches `SaveLoad: complete` after

the 15-second reload request, with no subsequent gameplay timing report. This

does not prove the restored game resumed. A monitor CPU-state query returned

no data, and monitor screenshots still failed after the CRLF correction; the

capture path must also be excluded as a possible source of the stall. The

specific test process (PID 19636, isolated instance, monitor 4779) was stopped;

the runner exited with a connection-reset error. Logs remain in that run

directory. Next reproduction must disable monitor screenshots and add

post-load tick markers before attributing this freeze to save restoration.



The follow-up `20260909_104247-issue1-reload-trace` disabled screenshots.

Snapshots 002 and 003 stop at the same stage: the first post-load render

finishes, then the next simulation tick enters `sithThing_TickAll` without

returning. Sound, events, AI scheduling, and surface updates finish before

that call. This reproduces the stall without the screenshot path. Added

opt-in, bounded per-object phase tracing to locate the affected restored

object and operation; no crash fix is claimed from this evidence alone.



Run `20260909_104657-issue1-reload-thing` narrows this further: object 0,

type PLAYER, reaches its control/type update and never reaches its handler,

movement, collision, or puppet stages after restoration. Added player-tick

markers around palette decay, weapon messages, and inventory firing for the

next reproduction. The runner now requires both save-load completion and a

post-load multi-frame resumption marker when a reload probe is requested;

process liveness alone no longer passes that test. Earlier summaries with

`aliveAtEnd=True` and `fatalCount=0` do not establish reload success.



Run `20260909_105054-issue1-reload-player` confirms palette decay completes

and the freeze occurs inside `sithWeapon_handle_inv_msgs`, before inventory

firing or the falling-death check. The stricter runner exits 1 with

`reloadResumed=False`. COG-call tracing is added for the next reproduction.



Run `20260909_105500-issue1-reload-cog` identifies the selected Bryar COG

32769, pending weapon 2, with mount wait 0. Its GetSourceRef call returns;

the subsequent `jkCog_SetPovModel` call (XBE 0x17120, map 0x407120) does not.

The next probe traces the resolved model, player association, and POV setup

stages. This is stronger localization, not yet a verified root-cause fix.



Root cause found in the actual link map: `jkDSS_Startup` resolves to

`xbox_stubs.obj`. The Xbox build omits `src/Dss/jkDSS.c`, so neither its

save callbacks nor JK packet handlers are registered. Inspection of the

359360-byte test autosave confirms the old layout: a 1580-byte header,

32-byte map, 28-byte state, then packets ending exactly at EOF; there is

no 36-byte JK episode prefix and no JK player/saber/POV packets. Restoring

things therefore loses `thing->playerInfo`, which weapon selection uses.

The fix links real jkDSS and removes its no-op stubs. An Xbox-only legacy

reader detects the omitted prefix and rebuilds JK player/saber associations

after loading old saves, retaining inventory and weapon deadlines. Build

and runtime verification are pending; new-format save round trips also

remain required.



Run `20260909_110422-issue1-reload-jkdss-fix` resumes after the formerly

freezing legacy autosave. Two snapshots contain the resumption marker and

the final log has two more 10-second gameplay timing windows. The initial

runner verdict was a false negative: verbose tracing wrapped the separate

`SaveLoad: complete` line out of the ring buffer. The game now latches load

success and requires it before emitting the resumption marker; the runner

checks that single marker. Per-object and COG-call tracing is removed.

The next build adds `-FireProbe` to exercise the normal fire binding and

`-Roundtrip` to save/reload through real callbacks on snapshot-backed E:.



Run `20260909_111203-issue1-fire-roundtrip` verifies the old save resumes and

fires: Bryar ammo falls from 50 to 44 during a three-second burst, eventually

reaching 0 and automatically selecting fists. Gameplay continues beyond

100 simulation seconds after the reload. New-save round-trip coverage is

still missing: the probe queued a save but its CRT read check did not find

the E: file. The next probe uses the engine file API for that check and logs

save-open failures. Its overall runner failure correctly reflects this

incomplete round trip, not a return of the old reload freeze.



Added opt-in `-DeathProbe` (ordinary lethal damage followed by normal

fire-button respawn) and `-MenuReturn` (returns from the Escape menu after

three seconds through the game menu result). These await build/runtime

validation and never send host input.



Run `20260909_111653-issue1-menu-death-save` verifies one menu cycle while

firing (ammo 47 before return, 41 after the next burst), ordinary lethal

damage (health 0), fire-button press/release respawn (health 100), and

post-respawn bursts (ammo 50 -> 44 -> 38 -> 32). The round-trip probe was

correctly rejected while dead; it now waits for a live player. A native XEMU

capture of the Escape menu is in that run's `screenshots` directory. No

host input or desktop capture was used. This does not yet verify MP menus,

falling-death visuals, or damage-flash appearance.



Run `20260909_120914-issue1-native-save` validates a newly written save:

the ordinary deferred serializer writes to the snapshot HDD, the ordinary

loader restores it, and the success marker records `curMs=20384`. Subsequent

firing reduces restored ammo from 32 to 26 to 20 by `curMs=30225`. The runner

completed normally after 114 seconds with two successful RAM snapshots and

zero matched fatal entries. `E:` was not writable in the prior ISO boot;

the test uses `\\Device\\Harddisk0\\Partition1\\issue1_roundtrip.jks` and the

Xbox file translator now preserves native absolute paths. This validates

serialization/loading, not the user's Save menu or a writable profile on DVD.

The build linked a fresh XBE and all five targeted host/link-map tests passed.



Active traversal is now available through `-Traverse` and

`tools/xbox/issue1_traversal_plan.txt`: the player is invulnerable to ordinary

damage and warps every five seconds among item/actor landmarks in distinct

sectors. Destinations outside the world are rejected and logged. The level

plan exercises level-to-menu-to-level transitions. The run below validates

traversal; it does not by itself establish weapon,

water, damage, or death coverage.



Traversal run `20260909_105806-issue1-traversal` completed in 278 seconds:

logs prove 17 successful landmark warps in level 1, a menu transition, and

11 successful warps in level 2. The emulator remained alive and the runner

stopped it normally. The last capture covers roughly 60 seconds of level 2,

so completion of that level's full 90-second phase is not established.



Opening-level smoke result (2026-09-09): 349 seconds elapsed, four successful

RAM snapshots, gameplay still alive at completion, and no matching fatal log

entries. See `build/xbox/issue1_validation/xemu_smoke_runs/20260909_103029-issue1-world/summary.txt`.

This run did not exercise death, reloading, menus, or water. Bots from the sibling

`OpenJKDF2ogx-ja-ut-bots` checkout are explicitly deferred until this work is done;

the requested follow-up is a 0â€“8 Setup slider, random easy/medium bots, and teams.



## Archived completed board entries (September 12)

Moved from TO_DO.MD so the working board contains unfinished work only.

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
- [x] Fix all escape/pause menu variants that overlap or bleed into the footer.
- [x] Continue footer audit for screens still missing the console-style footer.
- [x] Keep contextual button glyphs outside the colormap path so they stay clean.
- [x] Verify ready-up flow: `A` joins, only P1 `Start` advances, only P1 backs out.
- [x] Fix forcefield translucency; current MP forcefields can render as solid yellow.
- [x] Fix MotS-only reversed strafe direction.
- [x] Build a curated `mots_xbox_patch.gob`; do not repack the full MotS resource archive.
- [x] Ensure `JK_.CD` is not authoritative for mode switching.
- [x] Keep MotS force powers scoped to SP for now; MP keeps JK force powers.
- [x] Verify SP MotS -> JK switching fully reinitializes resource and mode state.
- [x] Verify `mots_xbox_patch.gob` is ignored in MotS mode and loaded again when returning to JK compatibility mode.
- [x] Include `box art.png` in the beta/release package.
- [x] Include `OpenJKDF2xCutsceneConverter.exe` in the beta/release package beside `default.xbe` or in `cutscene_converter\`.
- [x] Fix XMV converter packet alignment and verify regenerated JK/MotS intro XMVs through the Xbox runtime decoder in XEMU.
- [x] Fix MotS SAN cutscene audio extraction; embedded SMUSH `IACT` audio now becomes a PCM XMV audio stream instead of silent video-only output.
- [x] Re-run the final always-on XEMU soak after the MotS/mod-map/XMV fixes: 7203 seconds continuous, 89/89 RAM polls OK, fatal count 0, ISO cleaned, covered JK SP/MP, MotS SP/MP, 4P split-screen, and available mod maps.
- [x] Investigate MotS `s2l1_palace.jkl` XEMU black-screen/stall; fixed per-instance AI actor allocation and verified MotS palace -> mod -> JK transition proof.
- [x] Investigate `impsiege/impsiege.jkl` XEMU soak stall; verified gameplay-ready transition in the same MotS/mod/JK proof pass.
- [x] Complete live System Link real-lobby proof in XEMU or XEMU+Xbox.
  - [x] Audit against UC2 secure XNet/session flow.
  - [x] Reset stale System Link launch/session state after stop/game close.
  - [x] Pass deterministic two-XEMU 4+4 local-player smoke harness.
  - [x] Pass live two-XEMU real-lobby discovery/secure-launch proof using the pcap backend; local UDP stayed at `peers=0`, multicast failed monitor setup and should not be treated as the proof path.
- [x] Revisit resource cleanup across menu-to-menu, menu-to-level, level-to-cutscene, and level-to-level transitions.
- [x] Keep generated logs, smoke outputs, and temporary assets out of commits unless explicitly useful.


## September 12: water motion capture and probe correction

The water probe now targets its authored wet sector directly, checks body
and eye clearance, and resets cached positions on map changes. The Xbox build
and clearance unit test pass. Run `20260912_233947-issue1-water-motion` reached
sector 511 in `03katarn.jkl` and produced ten successive native screenshots
(23:41:04�23:41:15). All ten were inspected in order. They show subtle
projection oscillation and animated bubbles; one stationary boundary edge
alternates x=271/272 pixels across the sequence. The logs show changing
underwater FOV/aspect. The surface looks opaque in these views, but the runtime reports face type
0x2 (translucent) for boundary 4418. Its blend and occlusion need inspection;
do not assume the opaque appearance is authored or count the water report fixed.

The run exited with validation failure because the saved first-tick dry
position failed the new return clearance test; it stayed underwater. Do not
count this as a full wet/dry pass. The probe now caches a clearance-checked dry
position during ordinary physics before entering water, instead of accepting
the first gameplay tick unconditionally. That follow-up needs a build/run.

`scripts/xbox/xemu_native_sequence.py` reuses native screenshot discovery in
one process to obtain approximately 1.2-second-spaced captures after initial
lookup. No host input or desktop capture is used. `sequence.json` preserves
actual request/completion times and paths; still sequences are not continuous
video evidence.


## September 12: water report FIXED / XEMU validation complete

Respect authored flags per face; do not force all water translucent. Extracted
stock `03katarn.jkl` identifies underwater surface 4418 as faceflags 0x2,
adjoin 1404, with mirror 1409 belonging to above-water surface 4428 with
faceflags 0x0. Both use material 368. This matches the user's clarification:
opaque above can be intentional; the translucent underwater side must show
through when looking up.

Run `20260912_234642-issue1-water-flags` completed normally (141 seconds).
The opt-in WaterBlendProbe temporarily removes/restores only the underwater
face's translucency bit every ten seconds, leaving production behavior alone.
All fourteen native captures at 23:47:56�23:48:12 were inspected sequentially:
frames 0�5 show the deliberately opaque underside hiding above-water geometry;
frames 6�13 restore the authored translucent flag and visibly reveal that
geometry. This contradicts the earlier tentative opaque interpretation of the
normal underwater image. The accepted blend correction is working for water.
The face-blend test also passes opaque, cutout and translucent weighting.

Combined with the earlier ten-image distortion sequence, this verifies water
surface translucency and underwater projection motion in XEMU. The corrected
probe returned to dry sector 370 with FOV 106.0158, aspect .75 and zero tint,
then reached a second wet sector 513 / boundary 4439. The run exits successfully;
it is not the earlier failed dry-return run. Removed water from TO_DO.MD.
Custom map assets were not available, but the common renderer follows each
face's authored flags; no map-specific or global water-opacity override was
added. Hardware testing remains the deferred consolidated run.


## September 12: identified zero-light to fullbright discontinuity

Source inspection found a concrete brightness discontinuity in rdCache's
textured hardware path. A legacy May 12 fallback changed zero brightness to
1.0 for every mode other than Gouraud. Its comment incorrectly identified
lighting modes: actual FULLYLIT is 0 and NOTLIT is 1. FULLYLIT already has a
separate branch returning brightness 1.0.

RenderLevelGeometry explicitly collapses uniformly zero Gouraud faces into
NOTLIT, and collapses diffuse zero light similarly. The fallback then makes
those dark faces fullbright, while a small nonzero light remains dim. This
provides a concrete path for popping between lit and fullbright as a moving
light or clipping changes the face's intensities. Removed that fallback;
explicit fullbright handling and render caches are retained.

`test_zero_light.py --baseline` compiles the previous production conversion
and fails at mode 1/input 0 with brightness 1.0. The current production block
passes all integer levels 0..255 for NOTLIT, DIFFUSE and GOURAUD, including
continuity through zero. Xbox rebuild/runtime validation remains pending.

The preceding stationary four-player run `20260912_235005-issue1-lighting-stationary`
completed normally at 114 seconds. Its late screenshot sequence obtained only
one image before normal cleanup removed the process, so it is not temporal
flicker evidence. That image was inspected. No claim of a six-image pass.


## September 12: zero-light/fullbright report FIXED

The corrected Xbox build passes runtime run
`20260912_235430-issue1-zero-light-fixed`: 144 seconds, five successful RAM
polls, fatal count zero, alive before normal cleanup, Nar Shaddaa followed by
Bespin. Sound remained enabled. Four views were exercised with authored-spawn
traversal and the fire probe. All ten native captures (23:55:40�23:55:51) were
inspected sequentially: dark corridors remain dark; weapon models, bright
panels and firing effects remain visible across the captured positions.
The captures are not continuous video and do not alone prove every transient;
the reproduced source-level zero-to-fullbright discontinuity is fixed and
covered by the failing-before/passing-after production conversion test.
Explicit FULLYLIT mode 0 still takes its original brightness-1 branch.
No transform/color-cache rollback was made. Removed this resolved item from
TO_DO.MD. The audio report remains open: sampled buffer status and cursor
queries still report no failures, but original audible stall is not reproduced.


## September 13: audio investigation awaiting reproduction

The user confirmed they have not checked the latest build audibly. The original
`20260909_121203-issue1-mp-menu` directory contains only an empty poll file,
so no original buffer/cursor trace is available. The current targeted retry
`20260912_235908-issue1-audio-menu-retry` completed 140 seconds with all three
menu cycles, firing resumed, five successful polls, zero fatal signatures and
alive-at-end before normal cleanup. Sound was enabled. Across 56 unique
audio samples, status/cursor failures, missing buffers, capacity drops and
Play failures remained zero. No sampled persistent voice had a stationary
cursor. This supports game-side audio liveness, not an audible-output pass.

The same evidence gap has persisted through successive goal turns: the original
stall is not reproduced, no original trace exists, and native WAV capture is
unavailable for this XEMU audio backend. Water and lighting are now resolved;
there is no justified audio correction left to make from the current evidence.
Further repeated healthy runs would not prove the original symptom fixed.
Leave audio open pending a current-build recurrence or audible-output evidence.
The goal is blocked on that evidence, not complete. The dedicated test emulator
has exited; no test process remains waiting. The existing -AudioProbe harness
is ready for a targeted reproduction without host input.


## 2026-09-14: white loading panel fixed

FIXED in XEMU. The GPU clear was already black. FakeGL deleted a bound texture from its upload table but retained its ID in the texture-stage cache. The allocator immediately reused that ID; glBindTexture skipped the apparent redundant bind, so the replacement uploaded into texture zero and the intended loading quad rendered white. Deletion now unbinds the name from every stage and synchronizes the upload binding with the active stage. Loading artwork and font assets are preserved.

Reproduction: `20260914_131122-white-early-repro` native frames 13:11:32 and 34 are black, 35 shows startup artwork, and all 17 subsequent frames through 57 show the white panel. All 20 captures were inspected sequentially. Diagnostic run `20260914_132115-white-diag2` shows valid RGB menu pixels, then menu texture 14 being released and reused at draw 11.

Verification: `20260914_132412-white-binding-fixed`, all 12 native captures inspected sequentially: 13:24:34 retains startup artwork, 38 through 55 show the authored Nar Shaddaa loading background/progress, and 59 through 13:25:19 show gameplay with the multiplayer HUD. No white panel occurs in those captures. These are interval captures, not every rendered frame. Build `white_binding_fix_build_log.txt` succeeded; `test_texture_binding_lifecycle.py` exercises the production texture classes and bind/delete methods across two stages, immediate ID reuse, unrelated bindings and repeated deletion. Blank clears remain black. Hardware verification remains the consolidated later run.


## 2026-09-14: damage fade timing verified

FIXED/verified in XEMU, closing the damage-fade subitem of the original to-do #3. No production fade change was needed. A 25-health hit contributes full red tint; production decay is 0.4 per game second, or roughly 2.5 seconds to neutral. The old one-second repeating probe refreshed the effect before it could clear. The marker-scoped damage probe now supports configurable spacing and logs the recipient's actual tint during decay; normal gameplay is unaffected.

Run `20260914_132937-presentation-damage-4p` uses four players, no bots, no movement/fire injection, and seven-second hit spacing. All 36 native captures were inspected sequentially: frames 0�5 are neutral; 6�12 show the first hit fading; 13�25 are neutral; 26�31 show the second hit fading; 32�35 are neutral again. Only P1 world/weapon tint changes; the HUD and other three views remain unaffected. Logs independently record monotonic decay and zero at approximately 2.5 game seconds. Existing production tests for recipient isolation and per-local-player ticking pass.


## 2026-09-14: falling-death presentation verified

FIXED/verified in XEMU, closing the falling-death camera subitem of original to-do #3. The process-local fall probe uses an authored FALLDEATH sector; its optional delay now permits native capture of the short transition without changing production timing. Run `20260914_133430-presentation-fall-dense` records fall entry/begin, fade completion at game time 21433ms, prompt at 24443ms, and normal save restoration (`SaveLoad: complete`).

Native evidence inspected: pre-fall gameplay samples 0, 30, 50 and sequential frames 51�57; frames 58�62 show the third-person camera following Kyle down the shaft while world brightness fades smoothly; 63�65 and 70 show the completed black fade; 75�80 show the restart prompt over black; 119 shows restored first-person gameplay and firing. The full capture set contains 120 frames at roughly quarter-second intervals; only the listed frames were inspected, including every captured frame across the visible falling transition. This is not a frame-perfect comparison with the 1997 executable. The production fall-sequence regression test passes for camera switching, approximately 1.44-second fade, three-second prompt delay and exclusion of repeated corpse fall starts. No additional production camera change was needed.

## 2026-09-14: SP HUD anchoring, size and ghost readouts

Full-view SP now uses the same viewport-aware physical HUD anchoring as 1P MP.
Stock SP gauges use 4/3 of the saved HUD scale without modifying that preference;
MP's accepted scales remain unchanged. Native SP widescreen captures at
20260914_135604-sp-hud-stock-readout/screenshots/xemu-2026-09-14-13-57-02.png
and 13:57:05 were inspected. Gauges sit at the outer bottom edges and are about
one-third larger. Baked background zero strokes are now cleared before the
live SFT numbers are drawn; see hud-font-rendering.md for root cause and tests.

## 2026-09-14: corrected P1 harness verification

20260914_135741-p1-weapon-final exercised four process-local pads with primary
fire, movement, ammo replenishment and validated authored-spawn traversal through
m2 and m4 (45 game seconds each; 151 wall seconds including loading). All 15 saved
native captures were inspected in order: eight gameplay/four loading captures
in the first sequence, then three further m4 gameplay captures. P1 changes pose
while moving/firing and resumes firing; the repeated holster/select loop was not
reproduced. This is bounded interval-capture evidence, not inspection of every
rendered frame. Logs show fists/pistol initialization, ongoing fire and ammo
consumption, no failed traversal checks and no fatal/exception lines.

The existing corrected harness confines input to raw game pads and excludes
weapon-select input and pickup/actor traversal targets. Stress input, traversal
clearance and per-player tick regression tests pass. The remaining verification
checkbox is closed on this evidence. Audible output was not assessed.

Final release deployed to C:/Games/Emulators/CXBX/openJKDF2x/default.xbe;
SHA256 F9FD5A4A3D9B4753C67FE70836F0850E07572753D26ADE7E566504DA55F3F9A7
matches the tested build. Removed the stale normal-runtime xbox_smoke_mute_audio.txt
marker so normal play does not explicitly mute sound. The audible stall/dropout
report remains open; this marker removal is not a claim to resolve it.
