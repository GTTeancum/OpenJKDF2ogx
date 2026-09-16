# Console controls menu

2026-09-15. Implements the approved JK-themed scrolling list with action names
on the left, Xbox glyphs on the right, and amber/orange selection and scrollbar
colors. It uses the original Setup background and SFT menu fonts.

## Behavior

- Six visible rows scroll through 23 actions and settings. Up/down moves through
  the list; up from its first row reaches the setup tabs.
- A opens the button picker for fire, alternate fire, jump, use, previous/next
  weapon, and previous/next Force power. Choosing an occupied button swaps the
  assignments; all eight actions retain a binding. The wheel tap/hold behavior
  follows the assigned button.
- Stick movement, stick clicks, D-pad shortcuts, Map and Pause are shown as fixed
  controls with glyphs extracted from the existing Xbox button artwork.
- The list retains look X/Y sensitivity, deadzone, invert and vibration. Left/
  right adjusts these settings. Sensitivity stays within the backend's 1-100
  range; deadzone stays within 0-30 percent.
- B cancels a button-picker edit. Back or Done from the main list applies the
  changes through the existing file-backed settings system. X restores defaults.
  Settings remain shared across local players, matching the existing look options.
- The menu calls the existing shared Xbox footer renderer. It does not introduce
  a second footer layout. Fixed rows omit the Change action.
- The orange active-tab box is now applied by the shared tab-selection helper,
  including other setup tabs and the existing single-player/multiplayer tabs.
  Unselected tabs lose the box when selection changes. This is Xbox-only.
- Fixed the Xbox Display page dispatch so selecting another tab actually returns
  that tab, and Done exits instead of reopening Display.

Gameplay remapping happens before the existing action/wheel handlers. Menu A/B/X
remain physical buttons. Previous physical input is retained separately, and
entering menus releases held gameplay fire, jump and use states.

## Validation

- Release build: `build/xbox/release/controls_menu_final_build_log.txt`.
- `test_control_menu.py`: all 64 button swaps, invalid/duplicate map rejection,
  physical menu mapping, scrolling limits, picker selection restoration, defaults
  and setting bounds.
- `test_control_remap_input.py`: compiled production polling code verifies
  remapped fire/jump/use, press edges, menu releases and physical A/B/X behavior.
- Existing `test_menu_triggers.py` remains passing.
- First native XEMU run `20260915_212538-controls-amber-menu` used 4:3 and had
  ten successful polls with no fatal reports. All ten distinct captures were
  inspected in order, including the list, scrolling, options, button picker,
  fire/jump swap, and restored defaults.
- Final native XEMU run `20260915_213658-controls-shared-tabs-wide` used 16:9,
  ran for 167 seconds, and had 17 successful polls with no fatal reports. All
  17 distinct captures were inspected. The original menu background, list,
  stick/D-pad glyphs, button picker, reassignment, restored defaults and shared
  footer rendered correctly. The active box followed Controls, General,
  Gameplay, Display and Sound, then cleared on return to the Setup root.
  Menus retain their intended 4:3 presentation inside widescreen output.
- Final XBE SHA-256:
  `624A1B6074849D176ED6FFB1E21C6599F38D85E166BE7EE286D04CA85E73B566`.
- Native captures under
  `build/xbox/controls_validation/xemu_smoke_runs/20260915_213658-controls-shared-tabs-wide/screenshots/`:
  `shot_0053s.png` (Controls), `shot_0063s.png` (fixed-control glyphs),
  `shot_0081s.png` (button picker), `shot_0091s.png` (reassignment),
  `shot_0109s.png` (General), `shot_0119s.png` (Gameplay),
  `shot_0128s.png` (Display), and `shot_0137s.png` (Sound).

The XEMU fixture is explicitly enabled by `ControlsProbe`; it operates inside
the game and is absent from ordinary play. Runs use a snapshot disk and disable
host input. Physical gamepad operation and reboot persistence on Xbox remain
part of the consolidated hardware pass. Audio was not audibly assessed in these
menu runs. Other tabbed menus use the shared active-box renderer but were not
visually exercised in this pass.

## Setup navigation follow-up (2026-09-16)

Setup opens with General highlighted. Left/right selects a category in the
root strip, A opens it, and B returns from a page to the strip with that category
still selected. Controls retains its accepted list and navigation.

General, Gameplay, Display and Sound put an amber border around the focused
item. Up/down visits selectable items by column, with Gameplay's paired Solo/MP
checkboxes visited Solo then MP per option before moving to the next row. It
skips labels, hidden controls, disabled controls, tabs and footer actions, and
stops at the first/last item. LT/RT switches pages; left/right adjusts sliders.
Existing Back/Done setting-save behavior is preserved.

Display has no adjustable Xbox video settings. Its resolution row is read-only;
the page explains that aspect ratio follows the Xbox system setting instead of
the outdated placeholder text. The shared footer layout is retained.

`test_setup_navigation.py` compiles the production focus helpers and verifies
column transitions in both directions, end stops, hidden-item skipping, tab
switching, slider adjustment delegation, initial focus and Controls exclusion.
The controls mapping, polling and menu trigger regression tests also pass.

Build: full Xbox release build followed by recompilation/relink of the two menu
units after the probe was routed through the real footer B handler. XBE SHA-256:
`ACFAA2E53063ADA583664BE2CB2AC89E072B4627C1484C71DAC91326C666AC46`.

The first navigation run (`20260916_090154-setup-column-navigation`) completed
the four pages and return paths, but its one-pixel borders exposed two menu
presentation defects reported by the user. This run is superseded for visual
acceptance: widescreen point sampling could omit a vertical edge, and the menu
quad sampled on texel boundaries, producing a step at the triangle diagonal.

The follow-up keeps the existing fonts, sizes and point filtering. It aligns the
menu quad to D3D pixel centers with a half-output-pixel offset and gives active
tabs/focused items a two-source-pixel outline. This keeps borders visible in the
480-pixel-wide viewport used for 4:3 menus on a widescreen display. Gameplay/HUD
and cutscene geometry are unchanged. Also corrected the Gameplay scale slider
writing its numeric value over its label; the value now updates its own element.

Follow-up build log: `build/xbox/controls_validation/border_fix_build_log.txt`.
XBE SHA-256:
`73FD399DAD9E6F19CAA6AA0599B401AE24882512B5ECE321B54C35361D01907D`.

Native run `20260916_090702-setup-borders-aligned` completed in 199 seconds:
19/19 successful polls, zero fatal reports. All 19 distinct native captures were
inspected. General and Gameplay now have complete, straight outlines; Sound
shows the border moving from Music to SFX; all four pages return to their selected
root tab with B. Captures: `shot_0080s.png` (General), `shot_0118s.png`
(Gameplay), `shot_0147s.png` (Display), `shot_0177s.png` (Sound).

Display's unused legacy OK button was then hidden to remove its faint text behind
the shared footer. This page has only the B Back footer action because its video
information is read-only. Final build SHA-256:
`FFCB35789D962E38D27B93C2765892D8BACED220A1863A5ECB1C1F4E35756685`.

Targeted native follow-up `20260916_091053-setup-display-footer-final`, capture
`shot_0078s.png`, verifies the clean B-only footer and complete resolution-row
border. The final XBE is installed in `C:/Games/Emulators/CXBX/openJKDF2x`;
the preceding playable XBE is backed up under
`build/xbox/controls_validation/runtime_backup/pre_setup_navigation.xbe`.
This follow-up validates the 16:9 XEMU menu path; original Xbox and fresh 4:3
validation of the pixel-center adjustment remain unverified. No audio claim is
made from these visual menu checks.

## LT/RT and paired options follow-up (2026-09-16)

- LT/RT selects the previous/next Setup category. The footer shows both trigger
  glyphs followed by "Menus", including on Controls outside its binding picker.
  D-pad left/right no longer changes categories from the option list, so slider
  adjustment has no navigation exception. The root strip still supports left/
  right selection and A to enter.
- Trigger navigation uses physical trigger snapshots independently of remapped
  gameplay bindings. Only a new press changes categories; holding a trigger does
  not repeat. Both pressed together do not issue conflicting navigation.
- Gameplay's order is the left options and scale slider, then Auto pickup Solo,
  Auto pickup MP, No danger Solo, No danger MP, and so on. Up reverses this order.
- The six-action General footer fits within the original footer strip; the
  renderer reduces icon slot spacing when needed, keeping existing fonts/layout.
- Leaving General Advanced through a trigger propagates the requested category
  back to Setup instead of swallowing the navigation result.

Validation: production-helper tests cover paired traversal, slider delegation,
trigger threshold/disconnect behavior, and category changes. Existing remapping
and gameplay-trigger/menu-release tests pass. Native run
`20260916_092858-setup-triggers-option-pairs` completed 16/16 polls with zero fatal
reports. Inspected captures show scale 100 -> 120 without leaving Gameplay,
focus indices 20 -> 28 -> 21 -> 29 (Solo/MP per row), and Gameplay -> Display ->
Gameplay through the same category-change handler used by LT/RT. This fixture
does not emulate a physical trigger press. Physical controller validation remains
for hardware.

Final General footer capture:
`build/xbox/setup_footer_validation/xemu_smoke_runs/20260916_093105-setup-general-trigger-footer/screenshots/shot_0081s.png`.
Final XBE SHA-256:
`11ED05DD08BED77055DCFD9C2774D999FD5E52250552DDF43262E97F9114FE03`.

Footer spacing follow-up: LT/RT now advances by the rendered LT bitmap width
plus four pixels, removing the empty label and normal inter-action spacing
between the two triggers. Verified in native XEMU capture
`20260916_093454-setup-trigger-spacing/screenshots/shot_0089s.png` and installed
locally. XBE SHA-256:
`C69C548E89A846896B845223AAB762E2DD7882DB0D8EBC0D10B9904D39295553`.

Second spacing/focus follow-up: LT/RT advances eight fewer menu pixels to halve
the remaining visible gap at the standard footer size. Focused submenu widgets
now share the active header's amber filled background and two-pixel border.
The original background is restored before filling; widget content is drawn
after the fill, so text, checkmarks and slider artwork retain their colors and
old selections do not accumulate tint. Build SHA-256:
`7CA1654270CCBCC14679FCEBE1F6F535B13C6595301194ECBFB36226F9D4FB97`.
Native verification: `20260916_093816-setup-filled-focus-tight-triggers`,
`shot_0053s.png` shows the filled item and tighter trigger pair;
`shot_0062s.png`/`shot_0071s.png` show slider focus and adjustment;
`shot_0080s.png` shows the fill moving onto the Solo checkbox with the slider
background restored. Installed the verified XBE in the normal local runtime.
