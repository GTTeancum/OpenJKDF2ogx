# Xbox Display settings

The read-only resolution/aspect information has been replaced with four live
sliders. Xbox system settings still determine the output aspect ratio.

- Gamma: 50–150%, default 100%. Higher values brighten midtones.
- Contrast: 50–150%, default 100%. Applied about mid-gray before gamma.
- Safe zone X / Y: independent 80–100%, default 100%. Insets the complete image for
  overscan, including menus, the footer and gameplay. It does not change FOV
  or the configured resolution. Smaller values reduce the displayed area.

Four orange L-shaped corner markers show the actual output safe bounds while
Display is open. They track both sliders, including at 100%, and disappear on
leaving Display. They mark the full output, not the menu's narrower 4:3 panel.

Up/down selects a slider; left/right adjusts it. LT/RT switches Setup tabs.
The shared footer provides B Back, X Defaults, START Done and LT/RT Menus.
Settings save when leaving Display, including when switching tabs, under
`xboxDisplayGamma`, `xboxDisplayContrast`, `xboxDisplaySafeZoneX` and
`xboxDisplaySafeZoneY` in the existing
Xbox title-storage registry file. Loaded values are clamped to valid ranges.
Both axes fall back to the previous combined safe-zone setting for migration.

Gamma and contrast use the hardware output lookup table, without texture or
palette edits. Safe-zone viewport edges are rounded consistently across split
views; only the outside margins are cleared. Pretransformed movie quads receive
the same inset explicitly. Default settings preserve the original viewport and
an identity color ramp.

## Validation — September 17, 2026

Release build succeeded: `build/xbox/release/display_settings_build_log.txt`.
XBE SHA256: `36BBDB3D94DCC239C21EA22853ACC53DAB6498B2DC301765BB03B4EDBA98D741`.

The compiled calibration test checks identity, monotonicity over every allowed
gamma/contrast combination, clamping and matching split-screen edges. Existing
Setup navigation and trigger tests pass.

Native XEMU run:
`build/xbox/display_validation/xemu_smoke_runs/20260917_211657-display-calibration-widescreen`.
The process-local `-DisplaySettingsProbe` uses the production focus, slider and
footer handlers; no host input is sent. Snapshot HDD, unbound host controllers,
and system 16:9 were used.

Inspected captures show gamma at 125% (59s), contrast at 125% (72s), the complete
menu and footer inset at safe zone 90% (85s), restored defaults (101s), and return
to the Setup tab strip (120s). Logs confirm switching to Sound and back retains
125/125/90, followed by X Defaults and saving 100/100/100. Loading screens and
the Escape menu were also inspected. These captures show actual color changes,
not merely updated slider values.

Original Xbox hardware, cold-boot persistence, split-screen runtime at a reduced
safe zone, and movie playback at a reduced safe zone have not yet been visually
verified in this change. Their shared transform/math is covered by code review
and calibration tests; no audible-output claim is made by this menu test.

## Separate X/Y controls and narrower highlight

All four slider focus rectangles now use x=150, width=340: 85 pixels removed
from each side of the original 510-pixel box. Slider artwork stays centered at
x=320 with its original size. Rows are spaced to fit four controls above the
existing footer.

Incremental release build: `build/xbox/release/display_xy_build_log.txt`.
XBE SHA256: `C8294180D1E6F9C30FEE6CBBD131EB58B61E6FE402EAA6BE4670F80744624F36`.
Calibration and Setup-navigation tests pass. The calibration test now exercises
the production settings setter with independent X/Y values and invalid inputs.

The revised process-local Display probe resets to defaults before each isolated
change: gamma 150%, contrast 150%, X 80%, then Y 80%. It subsequently exercises
mixed settings, tab switching and restoring defaults. Original native images
are retained under
`build/xbox/display_validation/xemu_smoke_runs/20260917_212551-display-xy-isolated-proof/screenshots/`.

Inspected isolated comparisons (all other settings 100%):

| Native capture | Setting |
| --- | --- |
| `shot_0054s.png` | Baseline, all 100% |
| `shot_0064s.png` | Gamma 150% |
| `shot_0082s.png` | Contrast 150% |
| `shot_0091s.png` | Safe zone X 80%, Y unchanged |
| `shot_0110s.png` | Safe zone Y 80%, X unchanged |
| `shot_0128s.png` | Defaults restored |

The same background-only patch (x600–709, y270–329) has mean RGB
58.83/31.62/16.32 at baseline, 94.53/61.73/38.70 with gamma 150%, and
29.32/1.44/0.43 with contrast 150%. This region contains no changed labels or
selection boxes. The images themselves visibly confirm the color changes.
The X-only image retains full height; the Y-only image retains the baseline
width. No screenshots were recolored or resized to create these proofs.

Mixed 110% gamma / 120% contrast / X90% / Y85% settings remain present after
switching to Sound and back (147s, 165s, 174s captures). Defaults restores all
four to 100% (183s capture), and exit logs confirm those values were saved.
Cold-boot persistence and original hardware remain unverified.

## Safe-zone corner markers

Build and the display-settings, setup-navigation and menu-trigger tests pass.
Installed XBE SHA256:
`E902EBE93184350354D2520FA709DC1347C203462DC33DCABE4F0465699F2CB8`.
Native XEMU run `20260917_214030-display-corner-markers` completed 208 seconds,
21 successful polls, zero fatal reports, with snapshot disk and no host input.
Inspected native screenshots: 54s (100% corners), 105s (X80%), 114s (Y80%),
169s (X90%/Y85%), 160s (Sound, markers absent), 197s (defaults restored),
206s (Setup root, markers absent). The 179s capture caught incomplete menu
content; the adjacent 169s mixed-settings and 197s defaults captures are intact.
Images are under that run's `screenshots` directory in
`build/xbox/display_validation/xemu_smoke_runs/`. Hardware overscan remains
for the consolidated console run.
