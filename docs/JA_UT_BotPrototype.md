# JA/UT Multiplayer Bots

## Scope

This branch adds engine-side multiplayer bots without requiring map authors to
place navigation nodes. The intended behavior is a Jedi Knight player driven by
UT-style decisions: normal JK movement and combat rules, persistent enemy and
route choices, weighted item goals, combat repositioning, and recovery when a
route stops making progress.

Bots are currently enabled through the Xbox autostart arguments used by the
qualification tools:

```text
-autostart -mp -episode q3dm5 -map q3dm5.jkl -bots 6
```

UI work is outside the current scope.

## Automatic Navigation

The engine builds a graph during the map loading screen from information already
present in the level:

- Spawn points identify safe player starts.
- Item locations provide useful destinations.
- Walkable floor surfaces provide general movement coverage.
- Adjoin portals connect rooms and place approach nodes on both sides of
  doorways and corners.
- Collision and floor probes reject links that a standing player cannot safely
  traverse.
- Jump-pad COGs and thrust sectors add launch and landing links.

The generated graph is saved as a versioned `.bnav` file. Later loads validate
the map geometry and gameplay metadata before using the cache. A stale or
incomplete cache is rejected and rebuilt automatically.

## Bot Decisions

The brain is engine-side C code. It:

- Joins unused multiplayer slots as normal player things.
- Starts with the Bryar pistol and normal ammunition.
- Uses distinct available character models.
- Chooses and remembers enemies instead of changing targets every frame.
- Requires a clear combat line of sight before firing.
- Remembers a recently hidden enemy and routes toward the last seen position.
- Uses weighted A* routes with penalties for awkward climbs, hazards, and
  temporarily blocked links.
- Commits to movement and combat destinations long enough to avoid twitching.
- Advances, retreats, strafes, changes cover, and selects range appropriate for
  its current weapon.
- Diverts for valuable weapons, ammunition, health, and shields when worthwhile.
- Uses ranged weapons, close melee, Force Heal, Force Push, and Force Lightning.
- Learns short-lived danger areas from environmental and explosive damage.
- Detects stalled routes, blocks the failed edge temporarily, and replans.
- Respawns through multiplayer state with its model, animation, and loadout
  restored.

Doors and nearby switches are handled opportunistically while following a route.
Full semantic elevator behavior, including remote call switches, waiting,
boarding, riding, and exiting, remains Phase II and is not part of the current
qualification claim.

## JK And MotS Compatibility

The bot brain selects the correct JK or MotS inventory bins at runtime. Core
Bryar, stormtrooper rifle, repeater, rail detonator, concussion rifle, saber,
ammunition, model, respawn, and Force paths are compatibility-aware.

The MotS source audit also maps the stock Tusken prod, BlasTech pistol, and
scoped stormtrooper rifle primary fire directly from `weap_crossbow_m.cog`,
`weap_blastech_m.cog`, and `weap_stscope_m.cog`, including their inventory
bins, ammunition costs, projectile templates, timing, and weapon meshes.
These additions pass the Xbox compiler and pre-build audit. Runtime qualification
remains intentionally limited to q3dm5 until the current test-map restriction is
lifted, so MotS runtime behavior is not yet part of the beta-readiness claim.

## q3dm5 Qualification

Build 97 was qualified exclusively on the `q3dm5.gob` Quake 3 remake, as
requested. All emulator runs were muted.

### Visual movement review

The bot-follow capture shows sustained arena traversal, smooth turning,
advance/retreat behavior, cover changes, combat firing, and Force use. It does
not show the old stationary doorway dancing or repeated sub-meter reversals.

```text
build/xbox/recordings/q3dm5-build97-slot1-corridor-review/
build/xbox/recordings/q3dm5-build99-slot4-independent-review/
```

The independent slot 4 sample covered another 60 seconds and another followed
bot. Its saved log recorded six distinct models, 13 deaths and respawns, 76
Force actions, 280 pickup actions, one successful jump-pad transit, five
recovered route stalls, and no attempts to fire without line of sight.

### 300-second single-screen soak

```text
build/xbox/smoke_runs/20260726_114944-q3dm5-sixbot-300-97-weighted-combat-muted/
```

- 40 bot kills, zero suicides
- 9 jump-pad launches, 9 landings, zero retries or failures
- 17 recovered route stalls across six bots
- Zero attempts to fire without line of sight
- Bryar, crossbow, repeater, rail detonator, and concussion rifle used

### 300-second two-player split-screen soak

```text
build/xbox/smoke_runs/20260726_115616-q3dm5-splitscreen2-sixbot-300-97-weighted-muted/
```

- 40 bot kills, zero suicides
- 22 jump-pad launches, 22 landings, zero retries or failures
- 25 recovered route stalls across six bots
- Zero attempts to fire without line of sight
- Both local player slots participated correctly in damage and respawn state

The Xbox build completed with `audit_xbox.py: OK`, no bot compiler warnings,
and no build errors.

## Repeatable Quality Gate

Saved smoke runs can be checked without relaunching the emulator:

```powershell
powershell -NoProfile -ExecutionPolicy Bypass -File `
  .\tools\xbox_bot_quality_report.ps1 -RunDir <smoke-run-directory>
```

The default q3dm5 gate requires a complete muted match, no game or emulator
fatals, no suicides, no firing without line of sight, no failed jump-pad
transits, Bryar loadouts and distinct models for every bot, multiple ranged
weapons, successful pickups and Force use, and no more than 0.90 recovered
route stalls per bot-minute. Thresholds can be overridden when qualifying maps
with materially different geometry.
