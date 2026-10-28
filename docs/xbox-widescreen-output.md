# Xbox widescreen output validation

## External-output corruption follow-up (2026-10-27)

The user reported repeated bands and noise immediately in menus with 720p
enabled. This supersedes the earlier readiness conclusion: the October 26
telemetry smoke check did not qualify external video output. The baseline
native 1280×720 framebuffer image remained coherent, so native readback alone
cannot reproduce or disprove the reported output failure.

The candidate changes 720p color buffers from RGB565 to X8R8G8B8, retaining
D16 depth and automatic dashboard mode selection. Device creation and resets
share the buffer-format policy. The first 32-bit-color candidate reached menus
but exhausted memory during Nar Shaddaa loading (keyframe parse failure with
258,048 bytes free in the preceding sample). That candidate was not accepted.
The follow-up releases previous menu/movie GPU textures before world loading,
using the existing invalidation/re-upload mechanism instead of waiting until
after the world is allocated. CPU bitmaps remain available for repainting.

The revised build loads Nar Shaddaa successfully. The early release removed
12 cached UI textures; at the corresponding late-loading sample, free physical
memory increased from 258,048 bytes to 25,427,968 bytes. Five complete gameplay
windows in the roughly 100-second run reported 182.40–207.47 internal frames/s
and roughly 16–20 MiB free. Traversal arrivals were logged in the intended
sectors, with no failed world load. These are telemetry results, not an external
picture check. The native menu readback inspected for the 32-bit candidate
showed readable player selection, buttons and the correct background.

Mode/buffer-policy and texture-binding lifecycle tests pass, and the Xbox
release build succeeds. The deployed executable was read back and matches
SHA-256 `10BEEFA8182AC06D48169CA9F2BFDAEE899B9A0FD5AA10A3A283C2F612855691`.
Console video flags remain `0x0000000a`; startup logs 1280×720 progressive,
color format `0x7` (X8R8G8B8) and depth `0x2c` (D16).

An additional roughly 80-second `JK1CTF/c1.jkl` run with four requested bots
completed four polls, recorded a red-team capture and reported 178.06–186.73
internal frames/s in four complete gameplay windows. No fatal or world-load
failure was logged. This is one local view, not split-screen qualification.
The temporary launch, traversal and cutscene-disable files were removed and
the final build was restarted normally for the user's external-output check.

Evidence and build logs are under `.local/720p-corruption-20261027/`.
External capture/TV confirmation is required; successful mode logs and native
framebuffer images are not sufficient to close this reported failure.

## Hardware telemetry smoke check (2026-10-26)

At the user's request, removed the confirmed Red Faction installation
`E:\RFCrash` (1,181,803,906 file bytes). Free space increased from
280,215,552 to 1,462,337,536 bytes. Other installations and the separate crash
dump were left intact. Staged this project's build and stock JK assets in
`E:\OpenJKDF2-720p`; final free space was 1,297,006,592 bytes.

The deployed XBE was read back and its SHA-256 matched the local release:
`2CE71B3B838087A1A7EF4FDD3B3C71D68C3E58A304C74DCAF0E5D11CDC4A227C`.
Normal startup, SP and MP all logged `avpack=1 videoFlags=0x0000000a`,
`scan=720p aspect=16:9 backbuffer=1280x720`. No `-probe720p-sp` argument
was used and no console video settings were changed.

- SP: five polls over roughly 100 seconds, loading `06abarons.jkl` and
  exercising the process-local authored-spawn traversal fixture. Recorded
  arrivals matched sectors 35, 47, 308, 269 and 446. Five complete gameplay
  windows reported 71.79–128.40 internal frames/s, with about 19–23 MiB free
  physical memory. Loading was substantially slower and excluded from this
  range; an individual gameplay frame reached 135 ms.
- MP: three polls over roughly 60 seconds, `JK1CTF/c1.jkl` with four requested
  bots and one local view. The two complete gameplay windows reported
  181.90 and 187.30 internal frames/s; the callback log recorded a yellow-flag
  pickup. This was not a split-screen or long-match qualification.
- No fatal or failed-level-load message appeared in these logs. Temporary
  autostart, traversal and cutscene-disable files were removed. A final normal
  boot again selected 720p and reached menu paint callbacks.

Evidence is under `.local/hardware-720p-20261026/`, including all polls,
the deletion inventory, final directory listing and the read-back XBE.
Per the user's preference, no screenshots were taken or displayed. These are
mode-selection, loading and gameplay-telemetry checks, not visual acceptance:
TV image quality, audio, physical input and save persistence remain unverified.
Internal frame rates are not display refresh rates. The consolidated hardware
item remains separate.

## Current default (2026-10-25)

Normal startup now prefers 1280×720/60 Hz progressive output when the console
has 720p enabled and its AV-pack mode list exposes that mode. This applies to
menus, single-player and multiplayer. Otherwise it selects supported,
dashboard-enabled 480p, then 480i. Device-creation failure also retains the
720p → 480p → 480i fallback. No dashboard settings are changed by the game.

720p uses widescreen output and the previously validated RGB565/D16 buffers;
video resets preserve those formats and widescreen presentation. The old
`-probe720p-sp` argument is accepted for compatibility but is no longer needed.
1080i is not implemented; the highest supported mode remains 720p.

The mode-selection, projection, HUD and split-screen layout tests pass. The
Xbox release build succeeds; its log is `.local/video-default/build.log`.
Automatic menu/multiplayer startup at 720p has not received a new live visual
or hardware pass. The console is reserved for another project; no deployment
or console access was performed for this change. Earlier evidence below
records the original 480-line default and opt-in 720p implementation.

## Original 480-line implementation

Mode selection follows the reference behavior in
[UT99 Xbox's renderer](https://github.com/GTTeancum/UT99-Xbox-Releases/blob/main/UT99-Xbox/XboxRender/src/XboxRender.cpp): read the dashboard flags, confirm a progressive mode exists, and retain an interlaced fallback.
[Perfect Dark X's Xbox window manager](https://github.com/GTTeancum/Perfect-Dark-X/blob/port/port/fast3d/gfx_xbox_wm.cpp)
shows the separate higher-resolution path needed for 720p.

The dashboard's aspect setting selects an anamorphic 16:9 Xbox output at
640×480. The game changes its camera projection to reveal more of the world
horizontally; it does not widen a 4:3 game image after rendering. The Xbox
backbuffer remains 640×480 in both aspect modes. XEMU's native 480p captures
here are 640×480 raw samples; its 16:9 480i captures are already displayed at
853×480. Compare physical proportions only after accounting for that capture
difference.

The dashboard's 480p setting selects a 640×480/60 Hz progressive mode only
when the Xbox mode list exposes it. Otherwise the port uses 480i. Both device
creation and later device resets retain the selected scan mode, with a 480i
fallback if a progressive device operation fails. Interlaced output uses the
Xbox flicker filter; progressive output disables it.

## Native XEMU evidence, 2026-10-07

| System video setting | Game result | Evidence |
| --- | --- | --- |
| 4:3, 480i | Original narrower camera view; gameplay reaches the full frame | `build/xbox/widescreen_validation/xemu_smoke_runs/20261007_123501-normal-480i-sp/screenshots/shot_0065s.png` |
| 16:9, 480i | Wider camera view; gameplay reaches the full frame | `build/xbox/widescreen_validation/xemu_smoke_runs/20261007_123129-wide-480i-sp/screenshots/shot_0069s.png` |
| 16:9, 480p | Wider camera view; gameplay reaches the full frame | `build/xbox/widescreen_validation/xemu_smoke_runs/20261007_122801-wide-480p-sp/screenshots/shot_0071s.png` |
| 16:9, 480i, two local players | Two stacked raw 480×240 viewports share one centered 480×480 canvas; the combined displayed canvas is physically 4:3 | `build/xbox/widescreen_validation/xemu_smoke_runs/20261007_134552-wide-480i-2p-canvas43/screenshots/shot_0058s.png` |
| 16:9, 480i, four local players | Four raw 240×240 viewports share the centered 480×480 canvas; each displayed quadrant and the combined canvas are physically 4:3 | `build/xbox/widescreen_validation/xemu_smoke_runs/20261007_134749-wide-480i-4p-canvas43/screenshots/shot_0041s.png` |
| 16:9, 480p, four local players | Four raw 240×240 viewports share the centered 480×480 canvas; XEMU's 853×480 displayed capture shows a physical 4:3 game canvas with equal sidebars | `build/xbox/widescreen_validation/xemu_smoke_runs/20261007_135221-wide-480p-4p-canvas43/screenshots/shot_0044s.png` |
| 16:9, 480p, menu | The existing 4:3 artwork and controls remain centered, with black sidebars | `build/xbox/widescreen_validation/xemu_smoke_runs/20261007_124715-wide-480p-setup/screenshots/shot_0040s.png` |

The corresponding `summary.txt` and `ram_poll_*.txt` files record the selected
scan mode and aspect and show gameplay frames without fatal errors. The 4:3
and 16:9 single-player captures use the same map and startup camera; the 16:9
image includes more of the sign and surrounding architecture at the left.
That difference in scene content is the visual check against stretching.

The loading and menu artwork is still drawn inside a centered 4:3 viewport,
which produces black sidebars on a 16:9 display. With more than one gameplay
viewport, the combined split-screen canvas is physically 4:3. In two-player
mode the individual half-height panes are physically 8:3; in three- or
four-player mode each quadrant is physically 4:3. The `-split-fullwidth` flag remains
test-only. In a same-map 4P comparison without bots, the pillarboxed run
recorded 44.88–46.82 FPS in its steady-state windows versus 42.55–43.37
FPS for the full-width flag.

The tests `test_xbox_video_modes.py`, `test_widescreen_projection.py`, and
`test_widescreen_hud.py` pass. The source and native XEMU checks cover 480i
and 480p, not original Xbox hardware.

## 720p single-player assessment

The opt-in `-probe720p-sp` single-player smoke path now selects a native
1280×720 progressive backbuffer when the dashboard enables 720p and Xbox
enumerates the mode. It keeps the engine's logical 640×480 coordinates for
existing UI while mapping the world viewport to the physical output. The
normal menu and all multiplayer modes still select 480i/480p. Device creation
and reset fall back to 480 lines if 720p fails. This is a prototype, not a
system-selected release option yet.

An initial 32-bit color/depth 720p run reached about 1.2 MiB free physical
memory and failed to load `01narshadda.jkl`. The 720p-only RGB565 color and
D16 depth buffers recovered enough memory for that map and `06abarons.jkl` to
load. D16 rounded distant sky geometry to the cleared depth value, leaving
Barons Hed's sky black. The 720p path now uses a less-or-equal depth test for
sky faces only; the same outdoor and canal views visibly show the sky texture:

| 720p validation | Native capture | Result |
| --- | --- | --- |
| Nar Shaddaa, authored-spawn traversal | `build/xbox/hd_validation/xemu_smoke_runs/20261007_143236-sp-native720p-16bit-narshadda/screenshots/shot_0067s.png` | Level loads; 12 authored spawn points cycle through distinct sectors; steady 10-second windows reached 72.51–89.62 FPS. |
| Barons Hed, outdoor sky after D16 fix | `build/xbox/hd_validation/xemu_smoke_runs/20261007_144600-sp-720p-abarons-outdoor-proof/screenshots/shot_0079s.png` | Sky, world, and HUD render at 1280×720; nine authored spawn points cycle. |
| Barons Hed, sky over water after D16 fix | `build/xbox/hd_validation/xemu_smoke_runs/20261007_144600-sp-720p-abarons-outdoor-proof/screenshots/shot_0091s.png` | Sky remains visible at a second outdoor location. |
| The Maw, authored-spawn traversal | `build/xbox/hd_validation/xemu_smoke_runs/20261007_144848-sp-720p-maw-traverse/screenshots/shot_0066s.png` | Level loads; four authored spawn points cycle; steady 10-second windows reached 67.39–105.39 FPS. |

Excluding loading, the time-weighted XEMU gameplay averages were 80.59 FPS
for Nar Shaddaa, 59.35 FPS for Barons Hed, and 87.27 FPS for The Maw: 75.16
FPS pooled across the three traversal runs. The lowest 10-second gameplay
window was 56.73 FPS. The user accepted the XEMU performance and visual
results on 2026-10-07. Individual teleport frames reached 113 ms and 280 ms;
the averages are not a claim that every frame exceeded 30 FPS. Multiplayer,
including one-player matches with bots, remains 480i/480p.

## Original Xbox 720p validation (2026-10-08)

The Xbox development console was reached through RXDK Neighborhood. Its HDTV
AV pack was present, but its original dashboard video flags were zero, so the
first launch correctly selected 640×480 interlaced. For the test, `xbsetcfg`
enabled only the 720p flag. All three subsequent single-player launches logged
`XboxVideo: created avpack=1 videoFlags=0x00000002 scan=720p aspect=16:9
backbuffer=1280x720`; RXDK `xbcapture` produced native 1280×720 front-buffer
bitmaps. The console's original zero video flags were restored afterward and
verified by a final launch logging 480i, then it returned to XDK Launcher.

| Hardware scene | RXDK front-buffer capture | Observed content | Steady `PerfHW` 10-second windows |
| --- | --- | --- | --- |
| Nar Shaddaa | `build/xbox/hardware_validation/hardware_720p_003.bmp`, `hardware_720p_004.bmp` | City interior, signs, weapon, HUD; traversal logged all 12 authored spawn sectors. | 173.27–231.23 internal frames/s |
| Barons Hed | `build/xbox/hardware_validation/hardware_barons_050s.bmp`, `hardware_barons_070s.bmp` | Outdoor sky, canal water, interior-to-outdoor portal, weapon, HUD. | 87.88–123.86 internal frames/s |
| The Maw | `build/xbox/hardware_validation/hardware_maw_050s.bmp`, `hardware_maw_final.bmp` | Industrial interior and exterior, player model, HUD; all four authored spawn sectors logged. | 161.18–233.33 internal frames/s |

The corresponding hardware logs are `hardware_log_720p_085s.txt`,
`hardware_log_barons_070s.txt`, and `hardware_log_maw_final.txt` in
`build/xbox/hardware_validation`. No fatal, exception, or failed-level-load
message appeared. The first gameplay windows included texture/loading hitches
and are excluded from the steady ranges; individual frames still exceeded
30 ms. A ten-second host-clock check matched a ten-second `PerfHW` interval.
These counters measure frames produced by the game, not the display's 60 Hz
refresh. The captures verify the console front buffer; physical TV output and
game audio were not independently captured. FMV was disabled for these
targeted level tests.

At the user's direction, the TimeSplitters installation in `E:\TSRecomp` was
removed to free development-volume space. The test used unmodified JK resource
archives in an isolated `E:\OpenJKDF2-720p` install, which remains available
on the console. The consolidated original-Xbox hardware run in
[TO_DO.MD](TO_DO.MD) remains separate.
