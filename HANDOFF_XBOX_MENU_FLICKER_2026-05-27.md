# OpenJKDF2 Xbox Handoff - Menu Flicker / Character Menu / Current State

Date: 2026-05-27
Repo: `C:\Programming\GitHub\OpenJKDF2ogx`

## Current Focus

The active issue is Xbox menu rendering, especially:

- Xbox button BM images in the character menu.
- Player profile deletion from the main profile menu.

Resolved during latest hardware test:

- Character menu footer/button text flicker is fixed.

The latest build succeeded with:

```bat
cmd /c build_xbox.bat
```

Latest built XBE:

```text
C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release\default.xbe
```

## Working Tree State

Modified files at handoff:

```text
M XBOX_HACKS.md
M src/Gui/jkGUIBuildMulti.c
M src/Gui/jkGUIPlayer.c
M src/Gui/jkGUIRend.c
M src/Platform/Xbox/xbox_stubs.c
```

Untracked items already existed / are not part of this immediate menu task:

```text
?? build/generated/
?? build/xbox/smoke_logs/
?? data/
?? xbox_hardware_resource_audit.md
```

No commit has been made for the latest menu-flicker/glyph/profile-delete pass.

## Last User-Visible State

User tested the previous build and reported:

- Xbox button images now appear, but only about half of each image showed.
- Button images still had black backgrounds; they should have alpha / no black matte.
- Menu flicker continued.
- Log updated at:

```text
C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release\debug_openjkdf2.txt
```

Screenshot showed oversized LT/RT/white/black BMs clipped inside the character menu.

## Latest Changes After That Report

These changes were made after that latest user test and have built successfully, but have not yet been hardware-tested by the user.

### 1. Button BM Scaling + Matte Transparency

File: `src/Gui/jkGUIBuildMulti.c`

Changed `jkGuiBuildMulti_XboxBlitGlyphRemapped` so character menu Xbox button glyphs:

- Scale from the full source BM into the destination rect.
- Preserve aspect ratio.
- Center the scaled image in the rect.
- Remap source BM palette colors into the active menu palette.
- Treat declared colorkey pixels as transparent.
- Treat near-black source palette pixels as transparent:

```c
pal->r < 10 && pal->g < 10 && pal->b < 10
```

This should fix:

- Only half of glyph showing.
- Black rectangular background around glyphs.

Important caveat:

- This palette-remapped matte-skip logic was applied to `jkGUIBuildMulti.c` only. `src/Gui/jkGUISetup.c` still has its own Xbox button glyph draw path using `stdDisplay_VBufferCopy`; if setup-menu glyphs still look bad, port the same remap/scale/matte transparency logic there or centralize it.

### 2. Removed Fake Per-Tick WM_PAINT on Xbox

File: `src/Platform/Xbox/xbox_stubs.c`

Previous Xbox `Window_MessageLoop` called the menu window handler with a fake `WM_PAINT` every single loop:

```c
xbox_windowHandler(0, 0x000F, 0, 0, &unused); /* WM_PAINT */
```

That caused continuous full menu repaints/full flips. The hardware log showed repeated `MenuFlickerDbg: Paint` calls with stable hover/focus, which made this very suspicious.

Current behavior:

- `Window_MessageLoop` still calls:
  - `jkGuiRend_UpdateController()`
  - `jkMain_GuiAdvance()`
- It no longer synthesizes `WM_PAINT` every tick when a menu handler is installed.
- If no menu handler is installed, it still flips through `stdDisplay_DDrawGdiSurfaceFlip()`.

Rationale:

- Win32 only sends `WM_PAINT` after invalidation. The Xbox stub was repainting continuously. Initial menu paint, focus/click redraws, and character preview redraws already present explicitly.

This has been hardware-validated as fixing the character menu footer/button
text flicker.

### 3. Existing Menu Flicker Logging

File: `src/Gui/jkGUIRend.c`

Current log tags:

```text
MenuFlickerDbg: Paint ...
MenuFlickerDbg: Hover ...
```

Useful interpretation:

- If the latest build is fixed or improved, the hardware log should show far fewer repeated `MenuFlickerDbg: Paint` lines while idling on menus.
- If `Paint` still increments constantly while idle, there is another invalidation/repaint loop.
- If `Hover` toggles constantly, controller focus/hover state is bouncing.

### 4. Existing Character Preview Post-Draw Logging

File: `src/Gui/jkGUIBuildMulti.c`

Current log tag:

```text
MenuFlickerDbg: postDraw ...
```

This tracks the 3D character preview rendering attached to menu flips.

Important suspect:

- Character menu uses `jkGuiBuildMulti_sub_41A120` as an update function.
- It currently calls:

```c
jkGuiRend_UpdateAndDrawClickable(&jkGuiBuildMulti_buttons[6], pMenu, 1);
```

Element 6 is the character model preview. On Xbox, `jkGuiBuildMulti_ModelDrawer` does not directly draw into the 8-bit menu buffer; actual 3D preview draw happens from `jkGuiBuildMulti_XboxPostMenuDraw` during `stdDisplay_DDrawGdiSurfaceFlip`.

If flicker persists after removing fake `WM_PAINT`, inspect this update path. It may still force an animated menu flip every GUI tick.

### 5. Existing Glyph Logging

File: `src/Gui/jkGUIBuildMulti.c`

Current log tags:

```text
MenuGlyphDbg: idx=...
MenuGlyphDbg: draw call=...
```

Useful for confirming:

- Source BM dimensions.
- Palette pointer.
- Destination rect.
- Whether glyph draw keeps firing excessively.

### 6. Player Profile Deletion Fallback

File: `src/Gui/jkGUIPlayer.c`

Added Xbox-specific helper:

```c
jkGuiPlayer_XboxDeleteProfileTree(const char *path)
```

It:

- Calls `stdFileUtil_Deltree(path)`.
- Logs:

```text
PlayerProfileDbg: deltree path='...' result=...
```

- If deltree fails:
  - Enumerates files with `stdFileUtil_NewFind(path, 2, NULL)`.
  - Deletes non-directory entries with `stdFileUtil_DelFile`.
  - Retries `stdFileUtil_Deltree`.
  - Logs retry/deleted file/failure counts.

Next hardware test should check profile removal again. If it still fails, inspect `PlayerProfileDbg`.

Potential deeper issue:

- `stdFileUtil_Deltree` may be using the generic Win32 path implementation on Xbox, while some file ops need Xbox path translation. `src/Platform/Xbox/stdFile_xbox.c` has path translation helpers and implements some file util APIs, but not necessarily `stdFileUtil_Deltree`.

## Last Hardware Log Signal Before Latest Patch

The previous hardware log showed tons of full paint calls with stable menu state:

```text
MenuFlickerDbg: Paint call=91 menu=0014D750 hover=-1 focus=2 down=-1 hintIdx=0
MenuFlickerDbg: Paint call=92 menu=0014D750 hover=6 focus=2 down=-1 hintIdx=0
...
MenuFlickerDbg: Paint call=119 menu=0014D750 hover=6 focus=-1 down=-1 hintIdx=0
```

This strongly suggested constant forced repaint rather than random render corruption. That led to the `Window_MessageLoop` fix.

## Recommended Next Test

Have user copy/test the latest XBE:

```text
C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release\default.xbe
```

Test specifically:

1. Character menu idle:
   - Does animated character preview still display?
   - Do LT/RT/white/black glyphs fit and have transparent backgrounds?

2. Character menu controls:
   - White/black buttons change saber.
   - LT/RT changes character.
   - A on focused buttons does not crash.

3. Player profile removal:
   - Attempt to delete a profile from the profile menu.
   - If it fails, inspect `PlayerProfileDbg`.

4. Menus beyond character screen:
   - Setup-menu Xbox glyphs may still be bad because its draw path has not been ported to the new remap/scale code.

## If Flicker Persists

First inspect the new log:

```powershell
Select-String -Path 'C:\Programming\GitHub\OpenJKDF2ogx\build\xbox\release\debug_openjkdf2.txt' -Pattern 'MenuFlickerDbg|MenuGlyphDbg|PlayerProfileDbg' | Select-Object -Last 200
```

Interpretation:

- Many `Paint` lines while idle:
  - Another path is invalidating/repainting full menu constantly.
  - Search for `jkGuiRend_Paint(` and `jkGuiRend_InvalidateGdi()`.

- Many `postDraw` lines but few `Paint` lines:
  - Character preview animation redraw loop remains active.
  - Inspect `jkGuiBuildMulti_sub_41A120`.

- Constant `Hover` alternation:
  - Controller focus handling is bouncing.
  - Inspect `jkGuiRend_UpdateController`, `jkGuiRend_FocusElementDir`, and `jkGuiBuildMulti_HandleXboxController`.

- Glyph logs show large source sizes but small rects:
  - Scaling should now handle that. If still clipped, inspect math in `jkGuiBuildMulti_XboxBlitGlyphRemapped`.

## Other Open Items From This Thread

These are known but not the current task:

- Split-screen lockup reported; added to `XBOX_HACKS.md` for later.
- Split-screen ready-up screen flickers heavily, but user said to defer because that screen needs a better design anyway.
- Long-run memory/resource cleanup is a concern:
  - Level transitions.
  - Multiplayer match transitions.
  - Returning to menus.
  - Stale asset flushing.
- 720p is a stretch goal after stability/resource cleanup.
- `xbox_hardware_resource_audit.md` exists and should guide future resource tuning.

## Build Notes

Latest build succeeded. Usual warnings remain:

- `engine_config.h(447) warning C4616`
- Various existing compile/link warnings.
- No new build errors.

## Important User Preference

The user is frustrated by shallow symptom-chasing. For the next chat:

- Inspect logs first.
- Make targeted changes with clear rationale.
- Avoid “try one tiny thing” loops unless the log directly justifies it.
- Keep hardware iterations high-signal.
