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

## q3dm5 Qualification

Build 97 was qualified exclusively on the `q3dm5.gob` Quake 3 remake, as
requested. All emulator runs were muted.

### Visual movement review

The bot-follow capture shows sustained arena traversal, smooth turning,
advance/retreat behavior, cover changes, combat firing, and Force use. It does
not show the old stationary doorway dancing or repeated sub-meter reversals.

```text
build/xbox/recordings/q3dm5-build97-slot1-corridor-review/
```

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
