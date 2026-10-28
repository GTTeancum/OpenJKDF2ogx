# Bot navigation investigation — 2026-10-11

The user authorized a focused 90-minute run, beginning at 17:27 UTC. This
reopens routing only; the accepted lift/performance work remains closed.
The navigation task was **closed at the user's request on 2026-10-11**. The observations below are retained as investigation history, not an open work item. Ordinary flag captures work, but
central ramp and doorway stalls still occur.

## Changes

- Floor queries now start from the ceiling-clipped position returned by
  collision detection, using the same probe radius in both directions. Their
  depth budget remains relative to the original requested position. Previously
  a downward query could begin above a low ceiling and reject a safe passage.
- Static portal support searches extend from 0.50 to 1.0 units, still stopping
  at the first world surface and rejecting unsafe support. Stock `c1` ramp-side
  portals 307/343 were missing the floor 0.58 units below the probe origin.
- Route costs use resolved portal support height when pricing a climb. Two
  doorway samples at the same height can have floors at different elevations.
- Optional weapon, tactical and roaming pickups require a graph route back to
  the current navigation anchor. This prevents choosing a reachable item below
  a one-way drop when the bot cannot navigate out again.
- The short stuck check now compares squared movement with `0.06 * 0.06`,
  rather than `0.06`. The old threshold demanded about 0.245 units per 600 ms,
  misclassifying ordinary slow waypoint approach as a stall. A regression
  replays the measured 0.23-unit central-ramp movement: the old code discards
  its route, while the corrected code retains it. Stationary drift, the time
  gate, destination holds and long-term recovery are also tested.
- Navigation cache version is 44 so the changed floor checks rebuild old
  graphs. The read-only smoke ISO cannot save the regenerated cache; this is
  expected, and each recorded fresh-graph run generated it successfully.
- Opt-in route diagnostics include timestamps and support/attachment details.
  The performance smoke log filter now retains authoritative CTF callbacks and
  lift-fixture arrivals. The smoke runner snapshots the staged executable and
  symbols so a later build cannot invalidate that run's RAM polls.

The added `-bot-ctf-probe` fixture assigns red → yellow → red base goals using
ordinary movement. It resumes normal CTF after those arrivals. An earlier
version held at the final red base; that explains the user's observation of a
red-flag carrier returning to red during the directed test. Normal CTF tests
below do not use this fixture. No new movement boost or teleport was added.

## Reproduced permanent stall

`20261011_140938-bot-nav-ctf-normal-clearance` ran one ordinary CTF bot. After
abandoning a concussion-rifle goal, it selected railgun node 32, descended into
the lower weapon room, and collected it. The graph had an entry route but no
exit. CTF then selected fallback node 223, far from the enemy flag, and stayed
there through the end of the run. The stall counter reported zero because
being near the fallback waypoint was incorrectly treated as progress.

[Previously inspected weapon-room still](images/bot-navigation-20261011/weapon-room-stall.png)

After the return-route restriction, `20261011_141736-bot-nav-ctf-returnable`
completed 205 host seconds, 27/27 valid polls and zero fatal reports. The solo
yellow bot captured the red flag at game times **45.681, 94.243 and 158.118
seconds**, then collected it again. Eight stalls were counted in the 150-second
timed window; they recovered rather than leaving it permanently idle.

[Previously inspected yellow-base approach still](images/bot-navigation-20261011/red-flag-at-yellow-base.png)

All five unique native stills from each solo run were inspected. The before
run's later images show the same weapon-room position; the after run shows
travel, the carried red flag and the yellow base approach. The linked still is
an approach image; the actual capture count is established by CTF callbacks,
not inferred from a single screenshot.

## Verification and limits

All 16 `tools/xbox/tests/test_bot_*.py` production-function tests pass, including
new low-ceiling, portal-cost, pickup-return, stuck-progress and event-filter cases. Existing
route safety, lift contact, landing height, sector containment and team tests
remain passing. The Xbox release build succeeds (`bot_nav_verified_build.log`),
with final XBE SHA-256
`28949BC48B1E612BAF191B44825F1830D69A4E46472252090D24E6B6BEA59C2E`.
The build is saved under `build/xbox/bot_nav_validation/verified_build/`.

Native runs and their executable/symbol snapshots are under
`build/xbox/bot_nav_validation/xemu_smoke_runs/`. Two earlier directed 24-image
sequences (`135107` and `135530`) were inspected frame by frame in order. They
show red-flag transport through low corridors and doorway turns. They are timed
captures, not every rendered game frame.

The original eight-bot baseline already scored twice for yellow. The fresh
floor-fix eight-bot run `20261011_140335-bot-nav-ctf-eight-fresh` scored three
yellow captures and one red capture, with 44 counted stalls in its 290-second
window. Combat varies between runs, so this is not an isolated percentage
improvement. Some earlier diagnostic builds filtered CTF callbacks; their
absence of recorded events must not be interpreted as an absence of captures.

With the pickup restriction but before the short-stall correction,
`20261011_142121-bot-nav-ctf-eight-final` completed 363 host seconds, 39/39 valid
polls and zero fatal reports. It scored three yellow and one red capture, with
58 counted stalls in the 300-second window. All seven unique native stills
were inspected chronologically: flag carrying, combat, bases, corridors and
world/HUD content were present.

After the short-stall correction, `20261011_143105-bot-nav-ctf-progress`
completed 182 host seconds, 23/23 valid polls and zero fatal reports. The solo
bot made four yellow captures at **41.449, 70.341, 99.539 and 127.145 seconds**,
with **zero counted stalls in 140 game seconds**. All six unique stills were
inspected chronologically: the first is loading, followed by gameplay with
changing corridor/ramp positions and intact character/world/HUD rendering.
The meaningful result is repeated completed flag returns as well as the zero
counter; the near-fallback counter blind spot remains a known limitation.

The final-code eight-bot run `20261011_143451-bot-nav-ctf-eight-progress`
completed 367 host seconds, 39/39 valid polls and zero fatal reports. It counted
**45 stalls in 300 game seconds**, versus 58 in the previous 300-second run.
It recorded one red capture at 262.451 seconds, no yellow captures, and many
flag drops/returns during combat. This is not a scoring improvement claim:
the previous match had four captures. All seven unique native stills were
inspected chronologically and show traversal, combat, flags and intact
world/HUD rendering. Contested doorway recovery remains unresolved.

The telemetry-only repeat `20261011_144703-bot-nav-ctf-eight-live` completed
344 host seconds, 37/37 valid polls, zero fatal reports and remained alive
through the end. It recorded **35 stalls in 300 game seconds**, one red capture
at 108.350 seconds and one yellow capture at 184.020 seconds. No screenshots
were captured or inspected. The two final-code matches therefore demonstrate
captures under combat but do not establish reliable stall-free navigation.
The earlier `144546` launch used a mistyped command-line option, remained at
startup and was stopped; it is excluded from gameplay evidence.

The intermediate anchor-trace run `20261011_142753-bot-nav-ctf-anchor-trace`
has 13/16 valid polls and is diagnostic evidence only. All 48 saved sequence
images were inspected sequentially: the bot approaches a yellow-side doorway,
pauses and bobs against its edge, then backs away. This sequence does **not**
prove smooth traversal or depict the capture itself. Temporary route-reset
logging and additional diagnostic collision queries were removed afterward.

The final-build lift regression `20261011_144121-bot-nav-lift-regression`
logged all three arrivals: lower landing, upper landing, then lower landing,
including boarding at both lift stops. However, the emulator exited before
the requested duration: 99/120 host seconds, 9/10 valid polls, zero reported
game fatals and `aliveAtEnd=False`. Its final screenshot connection was
refused; the available stderr does not establish why the process exited.
This is completed round-trip telemetry, not a clean full-duration pass.
Its images were not inspected. At the user's request, subsequent validation
uses live telemetry only, with screenshot capture disabled.

The telemetry-only lift repeat `20261011_145317-bot-nav-lift-live` completed
108 host seconds, 13/13 valid polls, zero fatal reports and remained alive.
It reached the lower and upper fixture goals and rode back down with stable
relative lift height (0.1338), but did **not** log the final lower-goal arrival.
The last samples oscillate near sector 34's lower landing, with goal 4 and
next node 137. Thus the final build has one completed lift round trip in an
early-exit run and one full-duration run with incomplete landing traversal.
Do not treat these changes as a clean lift regression pass or deploy them as
a fully qualified navigation fix. No images were captured or inspected.

Unresolved: central-ramp route reversals, narrow-door recovery, inconsistent
lower lift-landing traversal, and the stall counter's near-fallback blind spot.
Preventing optional pickup entry does not
teach the bot to leave that room if it is displaced there by combat. These
checks are XEMU qualification; original Xbox hardware and audible output are
unverified. Audio was muted. All control and captures used process-local or
native emulator facilities; no host keyboard, mouse or desktop capture.

For a later focused pass, start with doorway nodes 262–264 and the central
326/327 transition in the saved final-code route samples. Separately,
`sithBot_CheckRouteGoalProgress` accepts proximity to a fallback graph goal
without establishing proximity to the actual CTF objective. That is why a
zero stall count must be corroborated by completed objective callbacks.

The investigation ended within the requested 90-minute budget (17:27–18:56
UTC). Changes remain uncommitted and were not deployed to the normal runtime.
