# MotS Xbox Compatibility Files

MotS uses `.GOO` files with the same `GOB ` container header this port already reads. The Xbox runtime should select MotS mode explicitly through the MotS launch path or `-motsCompat`; no CD key sidecar is generated or consulted.

Generate local Xbox/CXBX compatibility files from an installed MotS copy:

```powershell
python scripts\assets\mots_compat_pack.py --source "C:\Games\Star Wars Jedi Knight - Mysteries of the Sith"
```

The default output is:

```text
build\generated\mots_xbox_compat
```

It creates two kinds of output:

- `Episode\MOTS_SP.GOO` from `JKM.GOO`
- `Episode\MOTS_DM.GOO` from `JKM_MP.GOO`
- `Episode\MOTS_CTF.GOO` from `JKM_KFY.GOO`
- `Episode\MOTS_SABER.GOO` from `JKM_SABER.GOO`
- `Resource\JKMRES.GOO` and `Resource\JKMsndLO.goo`
- `mods\mots_xbox_patch.gob`

Copy the generated `Episode` and `Resource` directories into a MotS-specific Xbox/CXBX game folder. Copy `mods\mots_xbox_patch.gob` into the JK Xbox/CXBX game folder when JK should expose MotS compatibility assets. Do not commit the generated `.GOO` or `.GOB` files; they are local compatibility artifacts built from the user's installed game assets. The old CD key sidecar is intentionally left out so it cannot act as a hidden mode switch.

The patch archive is GOB format with a `.gob` extension because it is a JK compatibility overlay. JK mode loads `mods\*.gob`; MotS mode loads `mods\*.goo` and should use the explicit MotS resource/episode packs instead.

The patch is generated as a curated dependency set from MotS resource archives. It seeds MotS weapon, force, powerup, personality, saber, model-list, item-list, and static-template resources, then recursively follows referenced models, mats, keys, puppets, sprites, soundclasses, and sounds. It must not be a repacked copy of the full `JKMRES.GOO`.

Expanded MotS Force powers are scoped exclusively to the MotS single-player
campaign and MotS single-player mod maps. Multiplayer keeps JK Force powers,
including MotS multiplayer maps and enhanced multiplayer. This is the intended
scope, not a temporary limitation or an unfinished expanded-MP-Force feature.
The expanded Force wheel follows that same single-player MotS scope.

## Always-available Xbox content APIs

Xbox JK now registers a reviewed additive subset of the existing enhanced COG
library without requiring `-enhancedCogVerbs` or MotS gameplay mode:

- Vector equality, sine/cosine/tangent, system date/time.
- Sector lookup and sphere containment; sector ambient-light access and
  explicit all-sector light changes.
- Scalar and RGB surface-vertex lighting accessors.
- The four `playvoice*` aliases for existing sound functions.

These 19 names add capabilities for scripts that explicitly call them. They
do not replace stock verbs, run MotS scripts automatically, change inventory
bins/classes, or grant additional Force powers. MotS keeps its normal native
registrations. Existing local-sound verbs and `getweaponbin` were already
available to Xbox JK.

The existing `JKM_LIGHTING` map parser already accepts authored RGB vertex
data independently of gameplay mode. The RGB accessors operate only on surfaces
carrying that data; an ordinary grayscale surface is not converted by calling
them. Xbox now carries authored surface RGB through clipping and textured GPU
submission in JK and MotS. Opaque and translucent surfaces select RGB from
their authored data flags; ordinary surfaces retain scalar lighting. No
gameplay-mode switch is required. This does not add colored dynamic lights.

The broader optional enhanced library remains optional: it also includes
gameplay hooks, compatibility aliases, and unfinished Droidworks stubs. Do not
enable the entire library merely to expose these content APIs.

The ambient-light getter's inverted null check was corrected before exposing
it: valid sectors now return their ambient value, and invalid sectors return 0.
`tools/xbox/tests/test_content_capabilities.py` executes the production
registration helper and getter to check JK availability, avoidance of duplicate
MotS registration, and valid/null sector behavior.

Validation: full Xbox build and the compiled capability test passed.
XEMU run `20260917_220203-jk-content-capabilities` booted ordinary JK with no
enhanced-verbs switch, using snapshot disk and no host input. The native 54s
capture was inspected: Nar Shaddaa world, player weapon and HUD are present.
This startup regression check does not validate every verb in a live mod or
colored-light rendering; see the separate static-RGB validation below.


## Authored static RGB rendering (September 17, 2026)

Xbox selects the RGB clipping/cache path for surfaces carrying authored RGB
lighting, regardless of JK/MotS mode. Cache batches flush when switching between
scalar and RGB interpretations. The GPU receives raw RGB light modulation,
not palette colors multiplied over already-expanded texture colors. The existing
40-byte vertex layout and ordinary scalar lighting path remain intact. Buffered
and immediate submission both accept RGB, including valid all-black lighting;
alpha remains independent. Translucent surfaces use the RGB clipper too.

Validation:
- Full build (`static_rgb_build.log`) and follow-up renderer build passed.
- Scalar vertex test covers the existing 524,288 cases plus RGB clamp, black,
  alpha and fullbright behavior; batch test covers RGB-to-scalar transitions in
  buffered and immediate paths. Existing vertex submission tests pass.
- Native XEMU `20260917_222820-authored-static-rgb-proof` uses a temporary JK
  level fixture with red/green/blue authored values interleaved with unchanged
  scalar surfaces. Inspected 52s and 71s captures show colored world surfaces
  alongside gray surfaces, with normal weapon and HUD colors. No screenshot
  recoloring was used; no modified map is installed or shipped.
- This verifies world-surface static lighting. It is not validation of RGB
  architecture lighting on 3DO models, colored dynamic lights, or every
  translucent surface/material combination. Original hardware remains untested.
- Unmodified-map regression `20260917_223022-static-rgb-stock-regression`
  completed 82 seconds; inspected native 52s capture retains stock lighting,
  HUD and weapon appearance. The fixture was absent from this run.
- Installed XBE SHA256:
  `61A7CC3CACED41CB7495F46A06F92E425C0E2D8A378F45A6DB1C4F368B89992F`.

## Xbox menu round-trip validation (2026-10-07)

The opt-in `-MotsMenuNavigationProbe` follows the visible Players → Single
Player → New Game → episode list flow. It advances the game's normal menu
callbacks from inside the guest process, without host keyboard, mouse, or
controller input. It then uses Pause → Abort → Yes to return to the menu.
It does not use quick load or autostart.

Native XEMU run `build/xbox/mots_validation/xemu_smoke_runs/20261007_192015-mots-menu-roundtrip-final`
passed the JK → MotS → JK → MotS sequence in one process, with zero fatal log
events. The probe requires gameplay-stage confirmation for each selection:
`phase=1 episode=JKM mode=MotS level=s1l1_RebelBase.jkl`,
`phase=2 episode=JK1 mode=JK level=01narshadda.jkl`, and
`phase=3 episode=JKM mode=MotS level=s1l1_RebelBase.jkl`.
Inspected native captures include the episode list (`shot_0031s.png`), JK
gameplay (`shot_0105s.png`), and the returned MotS gameplay
(`shot_0154s.png`). The menus and level content matched each selected mode.

New Game now reports a resource-switch failure instead of continuing to load
an episode under the previous mode. This XEMU proof covers normal menu
switching and rendered gameplay, not original Xbox hardware or every save/load
path.

## MotS wheel and multiplayer checks (2026-10-07)

The opt-in full-wheel probe waits for campaign startup, grants all portable
weapons and level-1 Force powers, then uses the normal guest wheel hold/stick/
release path. It assigns distinct ammo quantities so an incorrect ammo-bin
label fails. Native campaign run `20261007_213403-mots-full-wheel-campaign-final`
and disposable SP mod run `20261007_213622-mots-full-wheel-sp-mod` each passed
16 weapon and 17 Force selections with zero bin, ammo, or level mismatches.
Inspected campaign captures show the 16-slot weapon ring, Blastech labeled 47,
and the 17-slot Force ring labeled level 1. The mod capture `shot_0099s.png`
shows that Force ring over its red-lit 3DO architecture fixture. EWEB is a
stationary turret and has no quick slot.
The final build-24 campaign run `20261007_223124-mots-full-wheel-build24`
repeated all 33 selections against fixed expected ammo quantities and passed
with zero failures and zero fatal logs.

Retail MP archive audit `build/xbox/mots_validation/mp_map_audit.json` lists
all 25 episode entries across `JKM_MP`, `JKM_KFY`, and `JKM_SABER`, each with
32 authored spawn records and its authored pickup templates. Native Freezer,
KFY k1, and Saber Home loads reached gameplay. KFY and Saber captures show the
14-slot JK Force ring; the Saber run logged neutral personality 0. The
remaining 21 maps passed the native load matrix in
`build/xbox/mots_validation/mp_map_matrix.json`; together with the four
earlier representative loads, all 25 authored MP maps reached gameplay.
Each matrix row has a post-gameplay native capture inspected for scene content.
The audit cross-checks placed weapon templates against their retail pickup
COGs and records the bins they grant. A railgun pickup grants standard and
seeking rail bins 127/137; a sequencer pickup grants charge and manual bins
128/138. Scoped rifle, flash bomb, Tusken prod, carbonite gun, and Blastech
have their own pickup COGs; the Blastech pickup is named `bryarpistol` in the
Spaceport map. These are authored placements, so not every map gives every
weapon.

The opt-in pickup probe invoked placed retail pickup COGs in the guest.
Native `20261007_222824-mots-palace-pickups` awarded all six expected Palace
weapon bins, including railgun and sequencer variants, with zero failures.
Native `20261007_223003-mots-spaceport-pickup` awarded Blastech bin 132 from
the Spaceport `bryarpistol` placement. Both runs exited cleanly and their
gameplay captures show rendered MP scenes. This validates representative
authored pickups alongside the complete static placement and COG audit.

Retail SP material audit `build/xbox/mots_validation/translucent_audit.json`
found 535 authored RGB translucent world faces using 24 material names across
14 JKL entries. The authored water boundary has the native visual comparison
described below. Native `20261007_222240-mots-glass-by-name` validated a
`00wglas1.mat` face in Asteroid Crust: inspected 53s/62s/71s captures show
scene detail through authored glass, that detail blocked when the probe
temporarily removes translucency, and the authored view restored. The weapon
and HUD remain visible. The probe changes only guest memory, not the retail
archive. Native `20261007_222500-mots-red-translucent` compared an authored
`00red.mat` face against its opaque phase and restored phase. Captures at
52s/60s/77s show the view changing from a red overlay to opaque white and
back to a translucent view through the window. Native
`20261007_223339-mots-warning-translucent` compared `00wwarny.mat` at
51s/60s/69s; the small striped panel changes from muted authored color to
bright opaque color and back. The 40-pixel panel was inspected in a crop of
the native captures at `build/xbox/mots_validation/warning_panel_compare.png`.
These native checks cover water, glass, red overlay, and warning panel cases
through the shared RGB translucent rendering path. The retail audit covers
all 535 authored RGB translucent faces; the other material names were not
individually visualized. A separate `00wwarn.mat` probe could not find a
clear camera view, so it contributes no visual evidence.

## Underwater POV lighting-state fix (2026-10-07)

The swamp water probe exposed a freeze after moving underwater. Bounded tracing
in `20261007_211515-mots-water-pov-trace` showed simulation and world rendering
completing, then first-person weapon rendering stopping after its depth clear.
The storm-only control `20261007_211338-mots-swamp-storm-control` continued
rendering for 91 seconds without the water warp. The user's observation that
the freeze followed lightning was retained during this comparison.

Translucent world faces can leave RGB vertex-lighting mode active. POV meshes
do not receive the RGB array assignment performed by the world architecture
pass. `jkPlayer_DrawPov` now flushes any pending world batch and restores scalar
lighting before submitting the weapon model. The opt-in water probe also
refreshes camera focus after warping and resets pitch when returning to spawn.

Build 16's native `20261007_211806-mots-water-pov-rgb-fix` run confirms the
inherited RGB reset and advancing underwater frames. Inspected native 75s and
88s captures compare the probe's temporary opaque water face against authored
translucency: opaque blocks the background, while translucency blends the water
texture over the view. The weapon, HUD, underwater tint and bubbles render.
The probe logs a return to dry sector 505 with FOV 90, aspect 0.75 and zero tint,
followed by another underwater entry. This comparison changes only a temporary
in-memory face flag; it does not alter the shipped map.

After removing diagnostic traces, build 17 passed native run
`20261007_212130-mots-water-final-authored` (142 seconds, exit 0, zero fatal logs)
with authored material flags unchanged. Inspected 69s and 101s captures show the
underwater view and the restored dry spawn respectively. Both entry and return
complete without freezing. Release XBE SHA256:
`D2428B80AF3F17E4F3788B1D3EC87168509CF4F929067BA6399B565FB89CA4F2`.

Host lighting-batch, modern saber, and modern saber model-light tests passed.
This evidence does not establish every MotS material or original Xbox behavior.
