# JA/UT Bot Prototype Handoff

## What This Prototype Does

This branch adds an engine-side multiplayer bot prototype, enabled by command line instead of map editing:

```text
-bots 6 -botmatch-seconds 90
```

The shape is closer to Unreal Tournament bot control than RBots map authoring. The engine owns the bot player slots, the map graph, target choice, movement, weapon choice, melee damage, respawn, and timed smoke-test score logging.

## How The Map Is Read

On map load, the bot system builds a simple movement graph from information already in the level:

- Spawn points become nodes because they are known valid player starts.
- Item pickups become nodes because they are useful destinations and are usually reachable play space.
- Floor surfaces become nodes when the surface is flagged as floor or AI-walkable and has an upward-facing normal.
- Nodes are linked only when the engine collision ray can travel from one node's sector to the other node's sector.

For `GEMPFAC.jkl`, the corrected generator produced:

```text
BotNav: generated nodes=182 directedEdges=625 map='gempfac.jkl' episode='GEMPFAC'
```

## What The Bot Brain Does

The brain is deliberately "medium" and engine-side:

- Join unused multiplayer player slots as real player things with bot net IDs.
- Keep bot player slots alive from the server's point of view.
- Pick visible enemies first, otherwise hunt toward the nearest active opponent's graph node.
- Move by facing the target, applying normal player-style acceleration, and jumping when the target is higher.
- If stuck, face the goal, press use, jump, and then choose a new goal if still blocked.
- Carry fists, Bryar pistol, saber, energy, shields, and force mana.
- Use Bryar at range and saber at close range.
- Apply saber damage through the normal `sithThing_Damage` path, so normal death and multiplayer scoring code credits kills.
- Respawn bots after death.

## Doors, Switches, And Elevators

This prototype does not require authored call spots. The current behavior is opportunistic:

- If a bot reaches a blocked goal and stops making progress, it presses use and jumps.
- That gives it a chance to trigger nearby switches, doors, and elevator panels without custom map files.
- It does not yet understand "this switch controls that elevator" as a semantic relationship.

That is the main assessment point: the basic player-slot, navigation, combat, respawn, and scoring loop now works; smarter mover/switch reasoning can be judged against this running baseline.

## Smoke Evidence

All runs below used:

```text
-autostart -mp -episode GEMPFAC -map GEMPFAC.jkl -bots 6 -botmatch-seconds 90
```

| Run | Result | Final Bot Scores |
| --- | --- | --- |
| `20260721_142327-botmatch-gempfac-final-1` | `fatalCount=0`, `emulatorFatalCount=0`, reached `fmv,botnav,botmatch-final` | Bot 1: 4, Bot 2: 3, Bot 3: 2, Bot 4: 3, Bot 5: 3, Bot 6: 5 |
| `20260721_142624-botmatch-gempfac-final-2` | `fatalCount=0`, `emulatorFatalCount=0`, reached `fmv,botnav,botmatch-final` | Bot 1: 2, Bot 2: 5, Bot 3: 2, Bot 4: 5, Bot 5: 7, Bot 6: 4 |
| `20260721_142921-botmatch-gempfac-final-3` | `fatalCount=0`, `emulatorFatalCount=0`, reached `fmv,botnav,botmatch-final` | Bot 1: 5, Bot 2: 9, Bot 3: 3, Bot 4: 3, Bot 5: 5, Bot 6: 4 |

The smoke logs are under `build/xbox/smoke_runs/` in this worktree.
