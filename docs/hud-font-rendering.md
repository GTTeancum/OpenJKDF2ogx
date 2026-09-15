# Stock HUD font rendering — 2026-09-13

Scope: engine rendering only. Keep the original SFT artwork, colors, glyph
metrics, HUD scale and placement. The earlier replacement-font experiment is
withdrawn; its files are backed up under ignored build/hud-font-cleanup/disabled-replacements.
No loose replacement SFTs remain in the runtime or release font directories,
and the build/staging scripts no longer install them. Stock Res1hi.gob was not edited.

The Xbox ASCII font draw path marks numeric HUD glyphs (charset 0 through :).
Their bitmap renderer selects linear minification only when the final physical
quad is smaller than the source glyph. Native-size and magnified glyphs retain
nearest sampling. Anamorphic widescreen compresses glyph width before display;
point sampling can miss a one-texel stroke. This change reduces that loss without
changing geometry, advances, artwork, tint or other UI textures.

Validation:
- Xbox release build passed: build/xbox/release/hud_stock_sampling_build_log.txt.
- test_ui_quad.py and test_widescreen_hud.py passed (UVs, triangles, tint,
  physical dimensions and viewport anchors).
- XEMU 20260913_110343-hud-stock-sampling-four: 92 seconds, four polls, zero
  fatal reports. Both native captures (11:04:23 and 11:04:38) inspected in order.
- XEMU 20260913_110538-hud-stock-sampling-4x3: native capture 11:06:23 inspected.
  Original 4:3 font appearance and HUD placement retained.
- Compared with the earlier stock widescreen capture from
  20260913_040859-bots-batch-color-abi-four (04:09:44), ammo strokes appear more
  continuous. This is a modest visual improvement, not user acceptance or a
  claim that all text-readability issues are resolved. Message/scoreboard font
  rendering is unchanged.

These were static font visual checks, not gameplay/performance qualification.
Sound remained enabled and no host interactive input was used.

## 2026-09-14: stock readout ghost digits and SP placement

The stock statusLeft16.bm contains authored red and green `000` readouts.
The GPU HUD overlays live SFT digits on that bitmap; separately rounded glyph
advances at fractional HUD scale leave part of the baked zero at the right edge.
This is the trailing stroke reported after both `100` values. An initial UV-inset
experiment did not remove it and was withdrawn.

The Xbox SP draw path now covers those two original readout rectangles in black
before drawing the live digits, rounding coverage outward. No SFT, BM, glyph UV,
character size or advance changes were made for this correction. The separate
user-requested one-third SP gauge enlargement and full-viewport widescreen edge
anchoring are retained. Multiplayer keeps its approved artwork and scales.

Validation: release hud_stock_readout_build_log.txt succeeded. The readout coverage
regression test checks scales 0.25 through 3.00; UI quad, widescreen anchors and
MP/SP scale lifecycle tests pass. Native XEMU captures from
20260914_135604-sp-hud-stock-readout at 13:57:02 and 13:57:05 were both inspected:
stock gauges correctly anchored, both live `100` values present, no trailing
background strokes. The two captures are a static HUD check, not an active-play
or audio qualification. Font assets remain untouched.
