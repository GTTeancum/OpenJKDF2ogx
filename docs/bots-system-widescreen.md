# Bots and system widescreen integration

Working branch: `codex/bots-system-widescreen`.

Bot source: local `codex/ja-ut-bots` tip `af6e4d0c`, common ancestor
`6b569b44`. Code integrated with a three-way comparison against the current
working files; accepted dirty fixes preserved. No merge commit made.
Backup: `build/xbox/integration-backups/20260913_001112/`.

Integration decisions:
- Preserve existing text-based Xbox settings persistence and dashboard metadata.
- Preserve timing, lighting, inverse-alpha blending, safe traversal and per-player
  weapon callbacks. Their four regression checks pass after integration.
- Import bot navigation, combat, CTF, loading feedback and Setup frontend.
- Limit bot count to 0-8, default 0; random model order and easy/medium mix.
  Reaction delay, aim spread and firing cadence differ by difficulty.
  Roster persists through respawns and resets on world change.
- Exclude stale runtime mute marker from XEMU staging. Sound remains enabled.

## Widescreen implementation reference and remaining work

Actual Unreal reference: `C:/Programming/GitHub/UT99-Xbox-Releases/`.
`UT99-Xbox/XboxRender/src/XboxRender.cpp` reads `XGetVideoFlags()` and
`XC_VIDEO_FLAGS_WIDESCREEN`, sets `D3DPRESENTFLAG_WIDESCREEN`, and reports
pixel aspect 4/3 for anamorphic 16:9. `Engine/Src/UnCamera.cpp` applies pixel
aspect to horizontal projection while preserving vertical projection.

Implemented the same system-driven behavior here: dashboard selects 4:3 or 16:9,
640x480 framebuffer, correct horizontal FOV and CPU portal clipping, matching GPU
projection, proportionate menus/HUD and all split-screen viewports. No 720p work.
This uses coordinated CPU/GPU projection changes; changing only the output
flag would stretch the world. Implementation and XEMU aspect validation are
complete; the later entries record the captures and window-sizing correction.

## Initial validation

Release build succeeded and produced `build/xbox/release/default.xbe`.
Run `20260913_002358-bots-integration-eight` uses stock m2.jkl, four local
players and eight bots. Logs confirm bots in slots 4-11, combat and kills.
Native screenshot `screenshots/xemu-2026-09-13-00-25-12.png` inspected:
all four local views are inside the map. Audio probe has no reported playback
errors in the observed samples; audible output has not been checked.

The sequence helper initially watched the wrong output directory; XEMU wrote
its capture to the configured `screenshots` directory. The image was retrieved
and inspected there. This was a capture lookup failure, not a game failure.

Setup, team play, difficulty tuning and widescreen qualification remain open.


The initial run completed after 168 seconds, six RAM polls, zero detected
fatals; the owned emulator exited normally. Logs also contain a bot lift-exit
timeout and an unresolved `playsoundthinglocal` call from `pow_shields.cog`.
Investigate these before full bot qualification.

## Projection implementation in progress

`xbox_video.h` exposes the pixel aspect cached from the dashboard video flag
at D3D device creation/reset. The renderer sets the anamorphic presentation
flag and widens the GPU horizontal frustum by 4/3. CPU full and portal clip
frustums apply the same scale to horizontal projection only; menu model
preview clipping retains square pixels.

`test_widescreen_projection.py` compiles the production math blocks and
checks physical square proportions, preserved vertical FOV, CPU/GPU agreement
and unchanged 4:3 for 640x480, 640x240 and 320x240 viewports. It passes.
HUD/menu sizing and runtime validation in both dashboard modes remain open.

The local sound verbs are now registered for Xbox JK as well as MotS;
enhanced registration avoids duplicate entries. Runtime verification pending.
Lift-exit timeouts repeat at the same exit (144, landing 67) across multiple
bots in the initial run, so navigation needs further assessment; no successful
exit was found in that captured window.

## Widescreen HUD and first runtime pass

HUD bitmap/rectangle widths now compensate for physical pixel aspect and keep
per-viewport edge anchors. Menu textures and model previews occupy a centered
4:3 region. `test_widescreen_hud.py` exercises the production transform and
passes for both pixel aspects, including all four viewport positions.

`set_test_eeprom_aspect.py` changes only aspect bits and the user checksum in
the smoke runner's disposable EEPROM copy. The test runner accepts
`-SystemAspect System|4:3|16:9`. The original EEPROM is not modified.
Layout/flags reference: https://github.com/Ernegien/XboxEepromEditor/blob/master/XboxEepromEditor/Types/VideoSettings.cs
Checksum reference: https://github.com/xemu-project/xemu/blob/master/hw/xbox/eeprom_generation.c

Run `20260913_003533-bots-wide-ui` completed in 141 seconds with five successful
polls and zero detected fatals. Startup logged system aspect 16:9 and pixel
aspect 1.333333. All three native captures (00:36:32, :40, :48) inspected in
sequence; XEMU captured 853x480 output and proportionate HUD gauges. They also
exposed missing geometry from the inline portal screen-bounds projection,
which still used horizontal scale for Y. That path is now corrected and
covered by the projection test; runtime confirmation is pending.

The missing playsoundthinglocal warning no longer appears in this run.
An additional force_well.cog dependency on jkgetmultiparam remains to assess.

## Setup qualification and team integration

Run `20260913_004511-bots-setup-4x3` completed after 221 seconds with eight
successful RAM polls and zero detected fatals. It read the EEPROM as 4:3.
All seven native captures were inspected sequentially: gameplay, escape menu,
bot setup at 0, 8, 4, then return to gameplay. Slider/log maximum is 8 and the
persisted/default count is 0. The setup layout and footer fit at 4:3.
The probe uses only in-process menu state and cancels without saving changes.

The earlier portal-correction run completed after 115 seconds, but its capture
started too late and the owned process ended before image acquisition. Visual
confirmation of the corrected 16:9 portal projection is still outstanding.

Ordinary split-screen setup previously hid the team checkbox and cleared its
flag outside CTF. The checkbox is now visible; ordinary team games retain its
selection. Local players alternate teams for ordinary team games; bots choose
the smaller active team (random choice on ties). CTF retains its authored
local-player team entry and bots pass their selected team through CTF entry.
`test_bot_team_balance.py` runs the production selection for 1-4 locals and
0-8 bots: teams differ by at most one and free-for-all stays unassigned.
These latest team changes are awaiting build/runtime qualification.

The force_well.cog jkgetmultiparam implementation already existed; it is now
registered for Xbox JK too, without enabling all MotS compatibility behavior.

## Combined 16:9 run, 20260913_005121-bots-wide-team-setup

Team build succeeded. All nine native captures were inspected in order:
00:52:09, :24 gameplay; :39 setup count 0; :54 and 00:53:09 black;
:24 setup count 4; :40, :55 and 00:54:10 gameplay. The corrected portal
projection no longer shows the earlier missing geometry. Widescreen HUD and
centered setup proportions look correct, including the new Team play checkbox.
However, the two black menu captures need diagnosis before UI qualification.
The 4:3 seven-image sequence did not show this symptom.

The staged TeamMatch registry setting did not activate teams: bot join logs
still show team=0. This run cannot establish team-play correctness. Trace the
autostart/settings route and use a verified team-enabled session for validation.
The latest executable includes team option/assignment changes and force-well
verb registration; sources have not been rebuilt or edited during this run.

## Menu presentation and team settings qualification

Run `20260913_010212-bots-menu-team-settings` completed in 199 seconds,
seven successful RAM polls, zero detected fatals. All ten native captures
(01:03:14 through 01:05:03) were inspected sequentially: gameplay, setup at
0, 8 and 4 bots, then gameplay. Every capture remained visible. The idle
message loop now presents the cached menu at approximately 30 Hz without
repainting controls. Widescreen portal geometry and HUD/menu proportions
remained correct.

Xbox text settings now write to U:\xbox_registry.cfg and read that path,
then the legacy relative path, then D:\xbox_registry.cfg. The test loaded
its staged D: settings; Team play was visibly checked. Eight bots joined
balanced teams (four each), alongside four local players. Of 39 observed
bot-to-bot shots, none targeted a teammate. No unresolved script verbs were
reported; the force-well registration correction is verified. Sound stayed
enabled; this does not establish audible-output quality.

One lift-exit timeout remained. The next build changes the exit's upward
velocity request to detach through the normal jump action first: grounded
physics otherwise projects that velocity onto the floor. This correction
requires active-match qualification. Bot join logs now identify easy/medium
profiles to verify the mixed roster in the same run.

## Two-player widescreen / mixed roster, 20260913_011205-bots-two-wide-lift

Release build succeeded. The run completed in 198 seconds with seven successful
RAM polls, zero detected fatals, and normal owned-emulator cleanup. Join logs
confirm four easy and four medium bots, four bots per team, with two local
players. All five requested native captures (01:12:53, 01:13:08, :24, :39,
:54) were inspected in order; both world viewports and HUD proportions are
correct in system 16:9. Bots actively fought and traversed while the local
cameras remained stationary; this was not a local-player traversal test.

Lift 48 still reports timeouts at exit 144, landing 67. The jump-detachment
change alone is insufficient; do not classify this navigation issue as fixed.
A successful exit at node 137 was also observed, so failure is route-specific.
The last performance sample reports about 27 FPS with eight bots and two
views; bot ticks averaged roughly 6-7 ms in observed intervals. This is new
bot-load evidence, separate from the previously accepted bot-free framerate.
Performance under bot load requires follow-up before final qualification.

## Lift route geometry correction

Inspected the stock m2 level and version-43 navigation cache. Exit node 144
is (-8.30,-0.91,1.34), in upper sector 44. Sector 47 ends at Y=-0.80;
the old radial clearance calculation moved the target to Y=-0.77, in outside
sector 29. Failed bots were below the upper landing in sector 36. Removed
radial target extension and the extra corner offset; navigation landing points
now remain the targets. Arrival requires the target sector, a world-floor
attachment, and height within 0.25 of the original landing. The deadline is
checked before centering/jump early returns. Release build succeeded
(`bots_lift_landing_build_log.txt`); runtime qualification is in progress.

Run `20260913_012004-bots-landing-4x3` completed in 169 seconds, with six
successful polls and zero detected fatals. All three native captures
(01:20:52, 01:21:12, 01:21:32) were inspected sequentially: correct 640x480
two-player layout, proportionate HUDs, and active bot combat. The lower exit
now reports the actual landing sector (34). The upper route still times out:
position (-8.36,-1.34,1.27), target (-8.30,-0.91,1.34), sector 109 to 44.
The target no longer extends into outside sector 29, but physical ledge
clearance remains unresolved. Portal nodes use minZ+0.04, while actor floor
clearance comes from physics height/model insertOffset; compare these next.
Eight-bot runtime cost remains open: the last sample fell to 15.49 FPS with
catch-up overruns, and bot ticks averaged 8.43 ms. No claim of bot-load
performance acceptance is made. The owned emulator stopped normally.

## Actor-clearance correction

The lift exit height now converts floor/portal graph samples (surface+0.04)
to actor-origin height using sithPhysics_ThingGetInsertOffsetZ. The upward
clearance threshold is 0.02 rather than 0.12. Authored spawn/item origins
are not adjusted. test_bot_landing_height.py compiles the production physics
helper and target calculation: model offset, explicit physics height,
minimum collision clearance, and unchanged spawn origins all pass.
Release build bots_lift_height_build_log.txt succeeded. Runtime route
qualification is still required; do not mark the lift fixed from this test.

Run 20260913_012601-bots-height-single-4x3 completed in 200 seconds with
seven successful polls, zero detected fatals, and normal emulator cleanup.
The native sequence was inspected in order: 01:26:46 is entirely white
and cannot support a layout claim; 01:27:06, :27 and :47 show the active
third-person bot observer in different areas, with correct world/HUD 4:3
proportions. Lift boarding/riding occurred later, but no completed upper
exit was recorded, so the height fix remains unqualified at runtime.
Use a repeatable process-local destination probe for the next lift check;
random combat routes have insufficient coverage of this exact landing.

## XEMU host-window letterboxing report

User screenshot shows a roughly 1280x720 game image in XEMU's 1280x960
window. Native 853x480 captures from the same run contain no top/bottom bars.
XEMU v0.8.136 config_spec.yml confirms default startup_size=1280x960 and
aspect_ratio=auto. The smoke runner now selects 1280x720 for widescreen
EEPROMs, 1280x960 for 4:3, retaining automatic aspect and proportional scaling.
This is host-window sizing, not a change to the game's 640x480 framebuffer
or a 720p game mode. No desktop input or window automation was used.
Reference: https://github.com/xemu-project/xemu/blob/v0.8.136/config_spec.yml

## CTF team-entry correction and widescreen window

The first CTF run (20260913_013017-bots-ctf-single-wide) exposed team=0
for all bot joins. ctf_main.cog's join handler clears the team as part of
staging; ActivateSlot previously read this cleared field when entering the
selected team. It now retains selectedTeam across respawn/join and passes
that value through normal CTF entry. The release build succeeded and the
team-balance regression test passed.

The retry (20260913_013715-bots-ctf-team-window) confirms requestedTeam equals
actualTeam for all eight bots, four per team, and ctf-gameplay-started
ready=8 total=8. Logs show attack, defend, intercept, escort, and a carrying=1
capture objective. Both native images (01:38:04 and 01:38:29) were inspected
in order: moving spectator view with proportionate single-view 16:9 output,
no image bars, and a visible red flag carrier. Generated XEMU configuration
selects startup_size=1280x720, aspect_ratio=auto, fit=scale. No desktop window
capture/control was used, so host-window geometry is supported by its config
and XEMU's source, not a new host screenshot.

The retry completed in 148 seconds with five successful polls and zero detected fatals. Final flag-return/scoring evidence remains to qualify.

## Path-search blocked-edge lookup

sithBot_FindPathNextWeighted now builds an owner-specific blocked-edge mask
once per search. The old predicate scanned 128 records for each examined
edge; the search now checks a bit in a local mask. The mask is rebuilt for
every search, preserving expiry and shared/private ownership without a
persistent cache. Existing callers outside path search retain the original
predicate. test_bot_block_masks.py compares 1,689,600 decisions against that
predicate across expiry boundaries, owners, and invalid records; it passes.
The release build bots_path_masks_build_log.txt succeeded. Active-match timing
is being collected; this change does not establish a 60 FPS result.

Run 20260913_014355-bots-path-mask-profile completed normally. Across 24
steady BotPerf windows (excluding the initialization hitch), median bot-update
time was 6.4 ms, with median 287 path calls per window. The earlier two-view
16:9 run had median 8.08 ms and 292 path calls. Randomized combat/routes differ,
so this is observational evidence, not a controlled speedup measurement.
Two lower-lift exits were recorded; no upper exit was established. Performance
is still open and needs timings separating pathfinding, visibility and movement
probes. Source inspection confirms GetSectorLookAt already has a same-sector
endpoint fast path; adding a redundant shortcut there would not address it.

## CTF scoring and flag-return qualification

Added an observational BotCTFEvent log for stock c1_ctfcallback.cog User0
flag events, retaining ordinary COG execution. Release build
bots_ctf_events_build_log.txt succeeded. scripts/xbox/summarize_ctf_events.py
deduplicates RAM polls and reports the level callback's event codes/scores;
combat also awards team points, so score changes alone are not captures.

Run 20260913_014955-bots-ctf-scoring produced event 12 at game time 70871 ms,
playerThing 117: redScore=36, goldScore=13. The preceding recorded red score
was 21. The stock handler removes flag inventory, adds capture points, and
sets the team score before emitting this callback. Slot 7's objective log
also identifies a red-team carrier pursuing its home capture point. Three
manual red-flag returns (event 11) were recorded by the 138-second poll,
alongside pickups, drops, and re-takes. This establishes capture scoring and
flag-return behavior on the stock CTF map with eight active bots.

The completed run also records a gold-team capture (event 22) at 156838 ms,
playerThing 111, goldScore=36 versus 21 at the preceding logged event.
Final observed totals: one capture per team, three manual flag returns,
and 15 distinct flag events. The owned emulator completed and was cleaned up.

## Directed lift route probe

Added -bot-lift-probe, disabled by default and reset when parsing startup
arguments. It applies only to autostart, bot slot 1, stock JK1MP/m2.jkl and a
valid navigation table. The probe assigns node 4 (lower landing), then node
144 (upper landing), using the ordinary route, lift, collision and stuck
controllers. Arrival requires the destination sector, floor attachment and
position within 0.2 of the node. The probe does not write positions, sectors
or attachments. It stops asking for movement after both destinations are
reached. Existing landing-height regression passes; release build
bots_lift_probe_build_log.txt succeeded.

Run 20260913_015742-bots-directed-lift completed in 173 seconds. All four
native images (01:58:28, 01:58:48, 01:59:08, 01:59:28) were inspected in order.
The bot moves but cycles in sector 21 around nodes 47/49/116 before reaching
the lower landing. Probe logs confirm goal 4; neither arrival phase completed.
This is not upper-lift verification. The next fixture prefers authored spawn
4 through sithBot_ChooseSpawnIdx and the existing spawn pipeline, isolating
the lift approach without relocating an already-running player. The corridor
route cycle also remains an observed navigation issue to investigate.

Run 20260913_020246-bots-lift-fixture verified authored spawn 4 was selected.
The bot reached lower node 4 in sector 36, rode lift 48 upward, and exited
into sector 44. BotLiftProbe confirmed node 144 at (-8.339,-1.078,1.420),
with floor attachment, then held there at zero speed. No exit timeout was
observed. Both native captures were inspected: 02:03:35 loses geometry during
the third-person lift view; 02:03:55 clearly shows the bot on the upper walkway.
The final progress logs also retain its correct upper-sector position.
The narrow lift-exit regression is verified; the corridor cycle is separate.
The probe now selects first-person spectator view to avoid the obstructed
third-person lift shot, leaving normal bot-camera mode unchanged. This last
camera-only adjustment is being built in bots_probe_camera_build_log.txt.

## Raised corridor-node reach checks

Route-node reach checks now compare floor/portal samples with the actor's
standing origin before consuming them, including ordinary and avoid-sector
path skipping and committed routes. The short-gap controller keeps steering
until attachment confirms landing, instead of accepting an airborne near-pass.
test_bot_route_floor.py exercises the production reach predicate with the
observed 0.15 raised walkway, including floor/portal nodes and unchanged spawn
nodes; it passes. Build bots_route_floor_build_log.txt succeeded.
-bot-corridor-probe uses the same destinations as the lift probe but retains
normal spawn selection, and both probes use first-person spectator view.

Run 20260913_021047-bots-corridor-floor completed in 133 seconds with five
successful RAM polls and no fatal reports. All three requested native captures
(02:11:51, 02:12:11, 02:12:31) were inspected in order. They are 853x480,
with world rendering extending to all frame edges and no letterbox bars.
The bot passed the original sector 21 corridor, but subsequently stalled and
cycled around nodes 126/57/131 in sector 37. Neither probe destination was
completed; the end-to-end navigation issue remains open. The raised-node
change therefore has only partial runtime validation.

## Standing-height jumps and full lift approach

The route-step jump gate compared floor samples against the actor origin and
ignored rises below 0.30. A 0.30 ledge consequently appeared as 0.22 and never
triggered a jump. GetRouteStandingRise now converts floor/portal samples to
standing origins; route jumps include raised floor nodes and start above 0.10.
The regression covers 0.15 and 0.30 rises and unchanged level ground.
Build bots_standing_rise_build_log.txt succeeded.

Run 20260913_022132-bots-standing-rise completed in 170 seconds. The bot
jumped and landed on node 118 in sector 23, then reached lower lift node 4
from its normal spawn. The full approach subsequently timed out on the upper
exit. All four native captures were inspected in order; the latter three
lose world geometry. Logs place the bot at (-8.41,-1.34,1.53), later z=1.77,
while retaining shaft sector 109 (whose upper bound is 1.30).

A scoped lift-exit repair now changes sector membership only when the old
sector does not contain the actor and the planned landing sector does contain
the unchanged position. It never writes actor coordinates. The regression
test_bot_lift_sector.py passes its containment/unchanged-position checks;
runtime qualification is pending in bots_lift_sector_build_log.txt.

Build bots_lift_sector_build_log.txt succeeded. Run
20260913_022700-bots-lift-sector completed in 160 seconds with seven successful
RAM polls and zero fatal reports. The bot reached lower node 4 from normal
spawn, retried one unsuccessful lift exit, then logged the containment-checked
sector repair from 109 to 44 at (-8.409,-1.345,1.530). It reached upper node
144 at (-8.337,-1.060,1.445), with floor attachment, and remained on the upper
landing at z=1.42. All four native captures were inspected sequentially:
02:27:45 shows the approach/lift area; 02:28:05, 02:28:25 and 02:28:46 show
the correctly rendered upper walkway. The directed normal-spawn-to-upper-
landing route is verified, including the previously failing corridor steps.
This does not claim every possible bot route is verified. Eight-bot performance
profiling remains the open integration item.

## Eight-bot profiling and scoped floor-query reuse

Added opt-in -bot-profile, reset to disabled during argument parsing. It logs
inclusive microseconds for visibility, floor checks, nearest-node lookup and
path search. Nested categories overlap and must not be summed. The Xbox
microsecond timer is now declared in stdPlatform.h; other non-POSIX targets
use millisecond timing for this optional diagnostic.

Run 20260913_023403-bots-profile-two: 136 seconds, six successful polls, zero
fatal reports. Across 17 distinct steady-state windows, median bot tick was
6.96 ms; median five-second totals were 60.81 ms visibility, 499.58 ms floor,
452.52 ms nearest and 8.76 ms path search. This identifies floor/nearest
queries as a more useful target than further path-heap optimization. All
three native captures were inspected. The observer option repeatedly resets
focus under split-screen, so subsequent performance runs omit -botcam.

Nearest-node lookup now owns a 16-entry exact-key floor-query cache for that
single read-only call. Probe actor, sector, coordinates and rise must all
match. Both success and failure results are cached. The previous cache context
is restored on return; no cache survives an action or physics update. Removed
the duplicate visibility trace already performed by IsWalkableSegment.
test_bot_floor_cache.py passes key isolation, failed-result reuse, capacity,
nested-context restoration and per-query lifetime checks. Release build
bots_floor_cache_build_log.txt succeeded.

Run 20260913_023951-bots-cache-profile: 140 seconds, six successful polls,
zero fatal reports, eight bots/two local views with normal cameras. All three
native captures were inspected. Eighteen steady-state windows had median
bot tick 5.695 ms and 3,066.5 avoided floor queries per window. Routes and the
observer setting differ from the preceding run, so this is not a controlled
speedup measurement. Approximately 28 FPS intervals remain.

Run 20260913_024223-bots-final-four: 157 seconds, seven successful polls,
zero fatal reports, four local views/eight bots, detailed profiling and
observer mode disabled. All three captures (02:43:38, 02:44:03, 02:44:28)
were inspected sequentially; all four views render and bots are fighting.
Normal bot ticks average roughly 4-6 ms in the inspected windows, but overall
frame rate falls to roughly 14-23 FPS. PerfSplit attributes about 3-4 seconds
per ten-second window to simulation and about 6 seconds to world rendering,
including roughly 2.7 seconds drawing things and 2.8-3.4 seconds drawing
geometry. Submission is included within rendering and must not be added again.
This is not a near-60-FPS qualification. Further simulation/rendering work is
required; the goal remains active.

## Textured vertex conversion

The renderer already advances rdroid_frameTrue once before the split-view
loop, so joint-matrix construction is shared. Run
20260913_024908-bots-render-profile used the existing XBOX_PERF_SMOKE build:
111 seconds, five successful polls, zero fatal reports. Representative
five-second windows report 767-856 ms processing things, 652-699 ms flushing
them, and 733-897 ms total draw-list submission (overlapping categories).
No native captures were requested for this instrumentation-only run.

Xbox textured triangles consume scalar lightLevel plus packed alpha, but
rdCache also calculated unused palette/tint/filter RGB for every vertex.
Added a scalar textured fast path, gated by a loaded texture, valid texture
binding function, at least three vertices and non-RGB vertex-lighting mode.
It preserves the scalar conversion, alpha byte and colormap flag. Missing
textures, untextured faces, lines and RGB lighting retain the existing path.
test_xbox_scalar_vertex.py verifies 524,288 light/alpha combinations and the
texture fallback gate. Existing zero-light continuity and face-blend tests
also pass. Build bots_scalar_vertex_build_log.txt succeeded with
XBOX_PERF_SMOKE=0, restoring normal release configuration.

Run 20260913_025625-bots-scalar-four: 132 seconds, six successful polls,
zero fatal reports, four local views and eight bots. All three native images
(02:57:07, 02:57:27, 02:57:47) were inspected sequentially. World lighting,
HUDs, damage overlays and the shot-triggered field remain visible without a
new discontinuity in these captures. Nine steady PerfHW windows measured
23.98-28.75 FPS (median 26.45). Matches differ, so this does not establish an
exact percentage gain. Near-60 performance remains unverified and open.

## Routine bot logging without synchronous disk stalls

Routine BotMatch, BotPerf, BotProfile and BotLiftProbe messages now use
xbox_debug_Trace, which writes only to the existing RAM mirror. Startup,
navigation and other diagnostics retain the disk logger. The prior routine
path performed synchronous write-through file writes and flushes per message.
test_bot_trace_routing.py passes routing and formatting checks; release build
bots_ram_trace_build_log.txt succeeded.

Run 20260913_030509-bots-ram-trace-four completed 133 seconds with six
successful polls and zero fatal reports, four local views and eight bots.
RAM records still show fighting, damage, respawns and BotPerf timing. Late
windows report 28.47 and 29.45 FPS and bot ticks around 2.35-3.10 ms.
No new captures were requested for this logging-only change. This randomized
match is not a controlled speedup measurement; near-60 remains open.

## Bound close-player fallback behind each camera

Both split-screen close-player fallbacks previously accepted any negative
camera-space depth, including distant players behind the camera. Added a
conservative negative bound using the existing expanded radius (2.5x for
sector culling, 4x for draw culling). Existing near-camera overlap remains.
test_close_player_cull.py compiles the production predicates and verifies
near overlap, forward behavior and distant rear rejection. Release build
bots_rear_cull_build_log.txt succeeded.

Run 20260913_031243-bots-rear-cull-four completed 134 seconds. Three native
captures at 03:13:22, 03:13:42 and 03:14:02 were inspected in order: all four
viewports render. Logs show active bot combat and respawns. These views do
not establish close-player crossing behavior; targeted visual validation
remains on TO_DO.MD. Steady windows include 18.17-26.87 FPS, so no measured
speedup is claimed for this randomized run. Near-60 remains open.

## Simulation stage attribution

Added opt-in XSIM_CALL timing for sound, bot decisions, actor/physics updates
and script ticks in both fixed-step and normal simulation paths. It uses the
existing -bot-profile flag; normal runs do not call the stage clocks. PerfSim
reports the same window as PerfSplit. These are subsets of simUs, not added
frame costs. The release build bots_sim_stages_build_log.txt succeeded.

Run 20260913_031812-bots-sim-stages-four completed 131 seconds, six successful
polls, zero fatal reports. No captures were requested for instrumentation-only
changes. Nine distinct windows show bot decisions consuming 1.78-2.87 seconds,
actor/physics 0.39-0.59 seconds, sound 0.05-0.37 seconds and scripts 0.01-0.03
seconds. One aligned window has simUs=3237761 and worldUs=6801322, with
botsUs=2285558. Thus bots dominate simulation while rendering remains larger
than all simulation. Next performance work should target bot collision queries
and world geometry/model rendering, rather than script-tick optimization.

## Remove duplicate route-sample floor queries

For ordinary route samples with a probe actor, PositionHasWalkableFootprint
already checks the exact center floor before its edge checks. IsWalkableSegment
no longer asks for that center separately. Assisted vertical segments and
missing-probe behavior retain their existing standalone check. All hazard and
footprint edge checks remain. test_bot_segment_floor.py compiles the production
branch and footprint center check, verifies all 32 mode/support combinations
against the prior acceptance predicate, and requires at most one center query.
Existing floor-cache and raised-floor tests pass. Release build
bots_segment_floor_build_log.txt succeeded.

Run 20260913_032358-bots-segment-floor-four completed 132 seconds with six
successful polls and zero fatal reports. Four views and eight bots with
-bot-profile; no captures requested for this query-only change. Initial
PerfSim botsUs=1584154, with later combat workloads varying. No controlled
percentage gain is claimed. Rendering and overall near-60 qualification
remain open, as does the separate close-player visual crossing check.

## Combined textured vertex submission

Added glXboxLitVertex to submit scalar color, texture coordinates and position
through one ABI call instead of three. It invokes the same FakeGL setters in
the same order, including the existing float-color cache. std3D retains its
world/screen coordinate conversion and alpha decoding. Untextured submission
is unchanged. test_lit_vertex_submission.py checks 131,072 light/alpha/projection
combinations against the prior attribute stream. Release build
bots_vertex_submission_build_log.txt succeeded.

Run 20260913_033024-bots-vertex-submit-four: native captures at 03:31:01,
03:31:21 and 03:31:41 were inspected sequentially. All four viewports,
textures, HUDs and P4 damage overlay remain visible. Observed timing windows
include 27.81 and 27.35 FPS; no controlled speedup is claimed. Rendering
performance remains open.
The run completed 131 seconds with six successful polls and zero fatal reports.

## SSE1 vertex-list transforms

The Xbox float rdMatrix_TransformPointLst34 path now computes the three output
coordinates in SSE1 lanes, loading matrix columns once per list. It uses
three scalar stores so tightly packed three-float vertices are never overrun.
Non-Xbox and experimental fixed-point builds retain the scalar path.
test_sse_transform.py passes 300,000 vertices against a double-precision
reference within a magnitude-based floating-point tolerance, plus guard,
empty-list and in-place checks. Release build bots_sse_transform_build_log.txt
succeeded.

Run 20260913_033530-bots-sse-transform-four: native frames at 03:36:08,
03:36:29 and 03:36:49 were inspected sequentially. All four views render;
player models are visible in P2/P4, including the closer P4 bot in the third
frame. No obvious transform corruption appears. This is not a full
close-camera crossing test. An observed PerfHW window measured 27.87 FPS;
no controlled speedup is claimed, and the overall target remains open.
The SSE run completed 131 seconds with six successful polls and zero fatal reports.

## Withdraw unqualified rear-culling optimization

The bounded rear-depth fallback did not establish a measurable performance
gain, and the ordinary native captures did not qualify close-camera crossing.
Withdrawing only that candidate restores both accepted close-player predicates
exactly (confirmed by the current sithRender.c diff, which no longer changes
those regions). Its candidate-specific test was removed. There is no longer
a new close-player behavior to qualify, so that candidate's verification task
was removed from TO_DO.MD. Bot integration, widescreen and the other validated
optimizations remain. This supersedes the earlier bounded-culling entries.
Release build bots_preserve_close_player_build_log.txt succeeded after restoring the accepted predicates.

## Zero-bot aspect diagnostics

Current release, same stock m2 map and four authored local spawn views:
20260913_034052-bots-zero-wide-baseline completed 94 seconds, four successful
polls, zero fatals. Wide zero-bot windows include 30.31 FPS, with later dips.
20260913_034309-bots-zero-4x3-baseline completed 111 seconds, five successful
polls, zero fatals. A 4:3 zero-bot window measured 35.36 FPS. These held-camera
runs isolate renderer cost; they are not active-play performance qualification.
No native captures requested because rendering source was unchanged.

The zero-bot cost rules out bot decisions as the sole cause of the current
frame rate. Widescreen adds cost, but 4:3 is also below target. A representative
4:3 PerfSplit window attributes 4.80 seconds to submission inside 10.70 seconds
of world work (counterSpan=12.95 seconds; use the recorded counter span, not an
assumed ten-second CPU interval). Next candidate is batching vertex submission
more deeply. The dormant PC DrawPrimitiveUP path cannot simply be enabled:
it needs Xbox API and primitive/attribute compatibility review first.

## Buffered submission compatibility review

Reviewed the installed XDK 5558 headers and the dormant FakeGL PC backend.
D3D8.h provides DrawVerticesUP directly; DrawPrimitiveUP translates primitive
count to vertex count before calling it. BeginPush/EndPush are also exposed,
but the public push constants do not provide the existing floating-point
attribute methods, so do not invent raw register writes.

The PC backend is not a safe drop-in: its final primitive-count switch handles
triangle lists/fans/strips only (points/lines fall through to count minus two),
and its generic vertex-format branch starts writing diffuse at the beginning
of the vertex, overwriting XYZ outside the explicitly handled TEX1/TEX2 cases.
It also replaces floating-point color attributes with packed bytes.

The candidate should therefore be a dedicated textured triangle-list path:
preserve existing state setup, pack XYZ/diffuse/UV explicitly, submit through
DrawVerticesUP, and split only on complete triangle boundaries. Keep other
primitive paths intact. Before enabling it, validate color/alpha quantization,
zero-light behavior, chunk boundaries, state transitions and native captures.
This review changes no rendering behavior; the current release remains the
built version with the accepted close-player fallback.

## Buffered triangle candidate — work in progress

Implemented a dedicated 768-vertex XYZ/diffuse/UV batch using DrawVerticesUP.
The recording-device test covers 768 boundaries, tail discard, packed color
bounds, inherited second UVs and immediate fallback. The first build accepted
only TEX1; FakeGL's capability-driven FVF is TEX2, so run
20260913_035137-bots-buffered-triangles-four did not exercise batching.
It completed 133 seconds, six polls, zero fatal reports; all three native
frames (03:52:16, 03:52:36, 03:52:56) were inspected.

Adding TEX2 enabled the path (LitBatch fvf=0x242), but run
20260913_035632-bots-buffered-tex2-four stalled in the first warmup update.
All three frames (03:57:09, 03:57:29, 03:57:49) were white. This is a failed
candidate regardless of the fatal counter. The owned emulator was stopped.
Removed the empty immediate Begin/End transition before buffered drawing;
bots_buffered_begin_build_log.txt is the next build to validate. Do not
qualify this candidate until actual buffered gameplay and captures pass.

Run 20260913_040027-bots-buffered-begin-four also stalled at the first warmup
update despite removing the empty immediate Begin/End. Both native frames
(04:01:04 and 04:01:24) were inspected and white. The owned emulator was stopped.
Thus the transition hypothesis was insufficient. XBOX_BUFFERED_LIT_TRIANGLES
now defaults to 0, restoring immediate submission in the normal build; the
candidate remains available only behind that compile-time gate for diagnosis.
Do not enable it without resolving and validating the first-draw stall.

Recovery build bots_batch_disabled_build_log.txt succeeded. Run
20260913_040356-bots-batch-disabled-recovery completed 75 seconds, three
successful polls, zero fatal reports and zero LitBatch activation records.
The native capture at 04:04:51 was inspected: gameplay and all four viewports
render again. Bot logs show combat and route movement. Normal submission is
restored; the buffered prototype remains disabled and explicitly unfinished.

## Buffered first-draw stall resolved: color-conversion ABI

Found the legacy Truncate helper declared naked cdecl but ending in ret 4.
Both callee and caller removed its float argument, corrupting the stack when
packed color conversion first ran. Replaced it with an ordinary inline SSE
intrinsic function. The batch regression now compiles the production conversion
helper too, rather than substituting a host cast. It passes. This supersedes
the earlier assumption that DrawVerticesUP itself caused the first-draw stall.

Build bots_batch_color_abi_build_log.txt succeeded, batching enabled. Run
20260913_040859-bots-batch-color-abi-four completed 131 seconds, six successful
polls and zero fatal reports. LitBatch enabled fvf=0x242 confirms activation.
Native frames 04:09:44, 04:10:04, 04:10:24 were inspected sequentially; all
four views render, with visible bot models, texture detail and damage overlays.
Sampled windows are 28.24/28.17 FPS: no meaningful speedup is established.
Menu/model-preview and 4:3 qualification remain before broader batch acceptance.

## Buffered 4:3 setup-menu validation

Run 20260913_041325-bots-batch-menu-4x3 exercised four locals/eight bots,
then the process-local setup-menu sequence and return to gameplay. Six native
frames were inspected sequentially: 04:14:06 gameplay; 04:14:21 and 04:14:36
Bots 0; 04:14:51 Bots 8; 04:15:06 Bots 4; 04:15:21 resumed combat with player
models and damage effects. All images are 640x480 with expected HUD/menu
proportions. The phase logs confirm count 0/8/4, maximum 8, default 0.
The separate character-model preview remains unqualified by this run.
The run completed 153 seconds with seven successful polls and zero fatal reports.

## Accelerated gameplay report: initial clock audit

Added scripts/xbox/summarize_game_clock.py to compare saved clock probes over
long host-monotonic intervals, rejecting game-clock resets and ambiguous wraps.
The existing uninterrupted four-view/eight-bot run
20260913_040859-bots-batch-color-abi-four advances 104.613 game seconds over
104.6073–104.6186 host seconds (ratio 0.999947–1.000054). The earlier
20260913_033530-bots-sse-transform-four advances 104.707 game seconds over
104.4892–104.5009 host seconds (ratio 1.001972–1.002084).
These runs do not show sustained accelerated game time at their approximately
28 host FPS. They do not establish correct physics integration or bot movement,
nor qualify higher frame rates or the user's exact reported run.
Source inspection finds bot thinking throttled to 50–60 game milliseconds per
actor; simulation invokes the bot scheduler within the fixed-step loop. Next:
measure executed physics steps and accumulated physics delta against host time,
then bot movement/acceleration and high-FPS behavior. No movement tuning or
clock-source change was made based on this preliminary evidence.

The V2 clock probe now records executed physics steps, accumulated physics
microseconds, bot scheduler calls and individual bot thinks. The monitor retains
support for older six-word probes. Build bots_simulation_clock_probe_build_log.txt
passed. Run 20260913_042512-bots-actual-simulation-clock completed 91 seconds,
with zero fatal reports. Across 42.4315–42.4488 host seconds it records 2,122
physics steps, 42.440 physics seconds, 42.432 game seconds, 2,122 bot scheduler
calls and 5,269 individual thinks across eight bots. This is approximately 50
physics steps/second with no duplicate bot scheduler invocation per step.

The imported movement code independently forced maxVel to at least 3.0 and
maxThrust to at least 4.0; the m2 walkplayer template inherits maxVel 1.0 and
specifies maxThrust 2.0. Ground steering also directly blends velocity toward
the boosted speed (a runtime log confirms desired=2.76, maxVel=3.00). Removed
these template overrides and the 2.4/2.6 ground/drop minimum target speeds.
The movement correction requires runtime speed and route traversal validation;
the original clock/high-frame-rate report remains open.

Build bots_authored_movement_build_log.txt passed. Initial corrected-speed run
20260913_042812-bots-authored-movement completed 111 seconds, five polls and
zero fatal reports. Logs confirm authored maxVel=1.00 and steering targets
0.92 (travel) / 0.98 (combat), versus the prior 2.76 travel target. Across
83.8545–83.8663 host seconds: 4,198 physics steps, 83.960 physics seconds,
83.965 game seconds, 4,198 bot scheduler calls, 10,445 individual bot thinks.
Two native frames (04:28:46, 04:29:02) inspected sequentially show intact
four-view widescreen gameplay and changed bot positions. Combat/damage/pickup
logs continue. These checks do not qualify gap/lift/CTF traversal at the new
movement speeds; those and higher-frame-rate timing remain on the working board.

## Authored-speed corridor/lift and high-FPS timing qualification

Run 20260913_043105-bots-authored-speed-lift completed 180 seconds, eight
successful polls and zero fatal reports. One bot starts at its normal authored
spawn; -bot-corridor-probe assigns lower-node 4 and then upper-node 144 goals,
without changing actor coordinates or sectors. The bot reaches node 4 in sector
36 at (-8.278,-1.112,0.919), retries one timed-out lift exit, then reaches node
144 in sector 44 at (-8.339,-1.069,1.422). Later logs remain at that landing.
Native frames 04:31:55, 04:32:25 and 04:32:55 were inspected sequentially;
the first is beneath the lift and the later two show the upper landing view.

The first two polling windows cover active traversal: 21.989 host seconds,
1,890 rendered frames (85.95 FPS), 1,099 physics steps, 21.980 physics seconds,
21.977 game seconds, and 1,099 bot scheduler calls. Thus this above-60-FPS
workload does not accelerate simulation. The full sample, including the
intentional destination hold, records 151.415–151.449 host seconds, 151.420
physics seconds, 151.414 game seconds and 7,571 steps/scheduler calls; average
rendering is 103.49–103.52 FPS. The held portion is timing evidence, not an
active-play performance benchmark. Gap and CTF validation remain after the
movement-speed change. High-FPS timing checks pass; the lift result below was
subsequently contradicted by the user's mesh-intersection report.

## Navigation deferred by user

The user clarified that this task is integration, not navigation repair, and
explicitly deferred navigation until other tasks are done. Stopped the owned
CTF test emulator in response; no navigation fix was made during that run.
Run 20260913_043528-bots-authored-speed-ctf shows repeated route stalls and
no flag events in the inspected polls. Track this as deferred work, not an
integration completion gate. Prior corrected-speed run
20260913_042812-bots-authored-movement already records paired short-gap
jump/landing events for slot 6 (126 to 56, sector 34) and slot 9
(155 to 156, sector 84). Further navigation investigation is paused.

The user subsequently reported bots becoming stuck inside the lift 3DO mesh,
rather than riding atop it. This invalidates the earlier claim of correct lift
traversal: route-node arrivals only establish endpoints, not collision/support
through the ride. The defect is explicitly recorded in TO_DO.MD as deferred
navigation work. No navigation repair is authorized as part of this integration
pass. Performance optimization is also retained as follow-up work under the
user's clarified integration-only scope.

## Integration-only preview check

Current team-balance, CPU/GPU widescreen projection, and HUD/anchor tests pass.
Added an explicit staging-marker frontend preview fixture; it selects an existing
profile/character, leaves the model visible for 45 seconds, then returns without
saving the character. It sends no host input. Initial run
20260913_044219-bots-model-preview-wide reached the hook before profile selection
and logged no existing character; the two captured frames show the player list,
not the model preview. This is not qualification evidence. Moved the hook after
profile selection and added marker-scoped selection of an existing staged profile
for the next run. Navigation and performance repair remain deferred.

## Integration completion audit (navigation repair excluded by user)

Build bots_model_preview_frontend_build_log.txt succeeds with the final frontend
fixture. Widescreen run 20260913_044554-bots-model-preview-profile-wide completes
92 seconds/four polls/zero fatal reports; 4:3 run
20260913_044754-bots-model-preview-profile-4x3 completes 91 seconds/four polls/zero
fatal reports. Both log ModelPreviewProbe visible/returned and enabled buffered
triangles. Wide captures 04:46:35, 04:46:51, 04:47:06 and 4:3 captures 04:48:44,
04:48:59, 04:49:14 were inspected sequentially: animated textured character and
saber previews render with proportionate menus, then return to the main menu.
The fixture is marker-scoped and does not save the edited character.

Final source checks pass: team balance for 1–4 locals plus 0–8 bots; CPU/GPU
projection for 1/2/4 views in both aspects; HUD physical sizes/anchors; zero-light
continuity; opaque/cutout/inverse-alpha blending; nested local weapon callbacks;
buffered triangle layout/color/boundaries/fallback. No unresolved Git conflict
entries remain. No test emulator remains running.

Integration requirements are satisfied in the working tree: incoming source and
dependencies were assessed and reconciled (history/backup above); Setup exposes
0–8 bots with default 0; shuffled models and easy/medium roster persist through
respawns; team selection balances local players and bots. Existing gameplay and
setup capture runs cited above cover 1/2/4 view and bot-count behavior. System
aspect follows Xbox video flags using the local UT reference, with 4:3 and 16:9
projection/menu/HUD checks and native captures. No merge commit or push was made;
the integrated changes and pre-existing work remain uncommitted.

This completes integration, not navigation quality or optimization. Lift mesh
intersection, CTF route stalls, eight-bot performance, audio reproduction, HUD
cleanup and later hardware/720p work remain on the unfinished-only TO_DO.MD board.
