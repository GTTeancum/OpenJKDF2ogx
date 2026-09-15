# Local multiplayer ready/start verification

2026-09-14: The existing ready handler accepts one joined player; P1 Start
advances when joined count is at least one. No extra local player is required.
Added a marker-scoped process-local fixture (xbox_smoke_local_match.txt containing
local-player count and bot count). It injects A/Start states into the ready edge
handler, renders the real ready/menu flow, then chooses the requested bot count
and returns the ordinary setup Begin result. Normal sessions have no marker.

Build local_match_flow_build_log.txt passed.
Run 20260914_123945-local-flow-one-zero used `1 0`:
- Native 12:40:19: one joined player, remaining slots unjoined, footer P1 Start.
- 12:40:39: match setup with Bots: 0 and normal background/options.
- 12:40:59: Blades of Death loading background.
- 12:41:19 and 12:41:39: multiplayer world, weapon and full-width HUD visible.
All five frames were inspected sequentially. Logs confirm multiplayer server,
zero bots and continuing game ticks. Audio was enabled but not audibly checked.
This proves the single-local-player zero-bot ready/setup/start flow; gameplay
input itself was not simulated during this run.

Team-balance production test passes for 1–4 locals and 0–8 bots. Eight-bot menu
flow, multiplayer-to-single-player transition, and regression menu runs remain.

Run 20260914_124233-local-flow-one-eight used 1 8. All five native frames inspected sequentially: 12:43:09 ready with one joined; 12:43:29 setup with Bots: 8; 12:43:49 loading/connecting routes; 12:44:10 and 12:44:30 gameplay with full-width HUD. RAM poll 004 explicitly reports 'start bots=8 activeBots=8'; later slot activity continues. Navigation defects remain deferred. This completes the zero/eight boundary menu flows.

Mode-transition run 20260914_124519-mp-hud-mode-transitions: four-player MP at 12:45:56/12:46:21; campaign loading 12:46:46; stock circular SP HUD at 12:47:11; MP loading 12:47:36; two-player MP with replacement HUD at 12:48:01; next cycle loading 12:48:26. Captures inspected sequentially. Lifecycle scale restoration, HUD anchor geometry and team balance tests pass on current source. Autosave write to the staged runtime failed during the campaign phase; save persistence and audible audio are not qualified by this test.
Final capture 12:48:51 inspected: four-player multiplayer restored on the next cycle. All eight transition captures inspected.
