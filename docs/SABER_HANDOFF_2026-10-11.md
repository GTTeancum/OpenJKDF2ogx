# Saber handoff — October 11, 2026

## User direction

Continue TODO #3 (boss attack fans with Modern saber), plus the latest request to round the flat blade tip with a cap. User authorized XEMU. Do not use GLOW3 for acceptance. The latest instruction is: **too many images; spin out a new chat; no image content.** Continue with text-only communication. Do not emit screenshots, image attachments, contact sheets or image tool results. Existing written visual reviews are evidence for the frames they explicitly cover; do not claim unseen captures were inspected.

No Computer Use, capture app, OS input or desktop automation. Terminal process management and process-local game fixtures are allowed. Native XEMU capture scripts are available. No subagents authorized. No commit/push requested. Preserve the many unrelated uncommitted changes. The closed colored-lighting exploration stays closed.

## Implemented and built

`src/Gameplay/jkSaberModern.inc`:
- Boss-only 50 ms history of real relative blade poses, interpolated without extrapolation (8 samples spaced >=8 ms).
- Live puppet keyframe damage-marker 4/5 window check, independent swing windows, and interruption/death/retraction guards. Player history and fan shape unchanged.
- Rounded camera-space semicircular tip cap matching the white core, colored feather and transparent edge. Exactly end-on blades get a full disk. Collision geometry unchanged.
- Bounded opt-in BossSaber/BossTrack diagnostics.

Evidence supports render-rate-dependent narrow one-frame fans, not missing retail damage markers. Yun/Boc/Jerec keys do have 4/5 markers, and original traces had changing poses, valid history and fan submissions. Do not present this as proof that every reported missing fan had this cause. Traced bosses did not expose a 64-bucket history collision; that preexisting bucket layout remains.

`src/Main/jkMain.c` opt-in fixture:
- Boss camera uses manually aimed camera perspective 16 (perspective 0 incorrectly hides its focus actor; normal chase camera was aimed incorrectly).
- Keeps player alive/invulnerable; no boss animation or input synthesis.
- Optional player/boss teleport to authored checkpoint thing IDs.
- Correct JK/MotS saber inventory slots (10/131) in existing modern-saber fixture.

`tools/xbox_xemu_smoke.ps1`:
- StockSaberAssets strips GLOW3 only from disposable stage, and strips MotS compatibility pack for JK runs. MotS keeps required compatibility pack.
- Content manifests with XBE/mod hashes.
- BossSaberCamera, BossSaberWarpThing, BossSaberBossWarpThing; timed native sequences.
- New summary fields record fixture/sequence settings.
- Native sequence helper archives screenshots before native filename reuse; samples roughly every 0.15 s, NOT every rendered frame.

Latest successful release: `build/xbox/release/default.xbe`, SHA256 `C1B2452E5CF94A91EFEA2899E53CDDCDE7334155B317D5247CD914C927884890`.
Build log `build/xbox/boss_saber_mots_fixture_build.log`. This was not deployed to the installed runtime or hardware. Earlier JK evidence used SHA `31687DBC8B5AFC4225A8E5CF5A11FD1EA3970BA3A5E8630925F0A3275679D762`; subsequent changes were fixture-only MotS support.

## Tests

Passed: test_modern_saber.py, test_modern_saber_light.py, test_modern_saber_menu_depth.py, test_modern_saber_model_light.py, test_lit_batch.py. Geometry tests now cover cap boundaries/end-on/near rejection, boss windows, interruption/death/retraction, signature/gap reset, both blades, local views and 8/16/32 ms pose sampling. Counts: normal blade50, end-on90, blade+fan113. Lighting test fixture constants were aligned with already accepted .68/.95; production lighting was not changed. Modern geometry and menu/depth tests rerun after final fixture changes.

## Native evidence already visually inspected

All runs below live under `build/xbox/boss_saber_validation/xemu_smoke_runs/`. Existing `visual_review.txt` files give specifics.

- `20261011_094434-stock-boc-cap-visible`: ALL 30 captures (000-029) inspected sequentially. Both purple fans clearly visible in 019 and 022, clear between swings/recovery. Player green saber independently visible. Stock assets, no GLOW3. Median traced draw gap20ms, 255 fan submissions/281 retained trace entries. Native sample spacing .153-.182 seconds.
- `20261011_094612-stock-jerec-cap-visible`: ALL30 inspected. Red fans in000/027, narrower wedge003, clear during recovery/idle. Player green blade visible.
- `20261011_094749-stock-yun-cap-visible`: ALL30 inspected. Foreshortened yellow wedge006/019, narrow adjacent007/020. Clear during recovery/idle021-028. Blade properly occluded by actors in forward portion.
- `20261011_095237-stock-boc-modern-off`: ALL30 inspected. Thin stock textured purple/green blades restored, no modern solid fan. Player partly occludes boss004-020, unobstructed022-029. Same map/camera but not frame-matched AI phases.
- `20261011_100006-stock-player-pov-cap-720p`: ALL40 inspected. Despite label, ACTUAL640x480. Rounded white tip with continuous green edge clearly visible003-021,025-026,031. Player fans023/033 and normal swings. Fallen enemies clutter foreground; not comprehensive hand-depth proof. Review file written.
- `20261011_100136-stock-player-third-cap-720p`: ACTUAL640x480. ONLY000-023 inspected sequentially so far. Rounded green/white tip clearly visible009-023; early swings and sparks visible, blade sometimes leaves top of frame. Camera close due nearby wall. Remaining024-039 not inspected. No review file yet. Do not emit more images to finish this review under latest user restriction.

Earlier GLOW3-inherited Oct9 captures are EXCLUDED acceptance. `20261011_093142-stock-boc-candidate` and `20261011_094126-stock-boc-cap` had bad fixture camera and are excluded. First clean Yun baseline `20261009_230654-stock-yun-baseline` was visually inspected but poor/occluded; not decisive A/B proof.

## MotS and final queued runs

`20261011_095014-stock-mots-kyle-cap`: wrong player fixture slot and boss still in off-map holding room. Only first frame inspected. Excluded combat proof.

`20261011_095739-stock-mots-kyle-arena`: player moved to authored thing90, boss91 moved to authored thing92 in final arena. ALL30 captures inspected, but mostly idle/power recovery, not a conclusive full attack sequence. Trace confirms normal kysnap3.key with fan=1. Kyle can use authored invisibility/force powers. Do not count this as conclusive visual fan acceptance.

Most recent terminal session94010 sequentially ran:
1. `20261011_100442-stock-mots-kyle-attacks`: same fixture (player90, boss92), 105s run, capture65s,60 frames. **NOT visually inspected yet.** Logs `build/xbox/boss_stock_mots_kyle_attacks.log`.
2. `20261011_100631-stock-cap-native720p`: uses required `-probe720p-sp` plus 720p EEPROM and16:9. Nar Shaddaa, ModernSaberPovProof, no fire input.75s run, capture50s,12 frames. **NOT visually inspected yet.** Logs `build/xbox/saber_cap_native720p.log`. Verify actual PNG dimensions and XboxVideo logs before calling it720p. Merely setting SystemScanMode720p only enables capability; earlier misnamed runs were480p.

Check session/process state read-only before doing anything else. No additional runs are necessary just to duplicate passing checks. All capture runs muted host audio. Hardware and audio for these changes remain untested.

## Documentation and finishing work

`docs/TO_DO.MD` still has #3 open with six-step plan and October11 progress, plus added cap requirement. `docs/modern-saber.md` has a new October11 implementation/test section but ends with a placeholder for encounter results and final build. Reconcile it with the verified evidence above, latest logs and metadata. Save concise final outcome, limitations and build paths. Do not mark unseen MotS captures as a pass or falsely close remaining visual acceptance work. User's requested rounded cap IS implemented and directly visually confirmed in both player views at480p. JK boss fans directly confirmed as above. New chat should finish text-only and avoid flooding the user with images or repeated run narration.

## Text-only follow-up completed

The earlier finishing-work paragraph is superseded: docs/modern-saber.md now records
prior visual reviews, final build and remaining limitations; docs/TO_DO.MD retains
#3 with outstanding acceptance work. No images were opened, no new visual review
was claimed, and no source changes, duplicate test runs, deployment, commit or push
were performed.

Both queued runs finished. No matching XEMU/smoke process remained at the read-only
process check. MotS: 109 seconds, 60/60 successful captures, all PNG headers 640x480,
11/12 successful polls (poll 010 incomplete memory reply; later polls recovered),
zero recorded fatal events. True 720p: 84 seconds, 12/12 successful captures, all
headers 1280x720, 10/10 successful polls, zero recorded fatal events. XboxVideo
created-mode logs confirm 720p, 16:9 and 1280x720. Both reached gameplay and were
alive at the final poll. Neither sequence is visually accepted. The 720p log also
records an autosave write failure; save behavior was not validated. Both manifests
match the rechecked final XBE hash above and exclude GLOW3. MotS retains its
required compatibility archive. Third-person captures 024-039 remain unreviewed.
