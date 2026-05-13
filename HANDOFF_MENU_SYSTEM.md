# Xbox Menu System Plan

## Goal

Run the original OpenJKDF2 menu flow on Xbox instead of jumping straight into a level. The Xbox port should use the same GUI/menu state transitions, level selection, pause menu, force menu, objectives, tally, and save/load entry points that the PC source uses. Xbox-specific code should only replace the platform layer: display buffers, controller input, and unsupported menu destinations.

## Source Of Truth

- Original reference tree: `C:\Programming\GitHub\OpenJKDF2ogx\_ORIGINAL_READ_ONLY`
- Active port tree: `C:\Programming\GitHub\OpenJKDF2ogx`
- Keep behavior aligned with the upstream GUI files whenever possible. Prefer compiling real `src\Gui\*.c` modules over rewriting Xbox-only menu logic.

## Phase 1 - Real Menu Boot

Status: implemented enough to build.

- Compile the core GUI modules into the Xbox build:
  - `jkGUI`
  - `jkGUIRend`
  - `jkGUIMain`
  - `jkGUISingleplayer`
  - `jkGUIDialog`
  - `jkGUIEsc`
  - `jkGUIForce`
  - `jkGUIObjectives`
  - `jkGUISaveLoad`
  - `jkGUISingleTally`
  - `jkGUITitle`
- Remove the old `jkGuiMain_Show` autostart path from Xbox stubs.
- Present the GUI's 8-bit software menu buffer through the Xbox renderer.
- Drive the original GUI message/update loop from controller input.
- Keep unsupported entries visible where requested, but make them return harmlessly.

Unsupported entries for now:

- Multiplayer
- Setup
- 3D Map from pause

## Phase 2 - Controller Navigation

Target behavior:

- D-pad and left analog move through menu selections.
- A activates the selected item.
- B/Escape backs out where upstream supports cancel.
- Start opens pause during gameplay through the original pause path.
- R3 remains first/third-person camera toggle in gameplay.

Implementation notes:

- Keep input translation at the Xbox platform/control boundary.
- Feed the original `jkGUIRend` key/mouse-style navigation path rather than adding per-menu controller code.
- Add repeat timing for held directions only if the upstream menu does not already handle it cleanly.

## Phase 3 - Menu Rendering QA

Verify on hardware/emulator:

- Main menu background, buttons, text, and selection state render with the right palette.
- Dialog boxes composite correctly over the previous menu frame.
- Pause menu overlays gameplay without corrupting the 3D scene.
- Force power menu shows stars, descriptions, and selection changes.
- Single-player episode/level selection launches through `jkMain` like PC.
- Unsupported no-op entries do not crash and leave the menu state usable.

Known risk:

- GUI rendering still depends on a software 8-bit buffer copied into a texture every frame. This is faithful to the PC path, but it needs runtime validation for palette changes, alpha/color-key behavior, and scene begin/end ordering on Xbox.

## Phase 4 - Finish Unsupported Menus

Bring these online only after the core menu loop is stable:

- Setup menu: map options that matter on Xbox, hide or disable PC-only settings.
- Player setup: confirm profile name, character, and saber color persistence.
- Multiplayer: leave disabled unless networking becomes an active milestone.
- 3D Map: either wire the original overlay-map menu path or keep the item disabled if the runtime map subsystem is incomplete.

## Phase 5 - Save/Load

Final phase for this menu milestone.

- Replace the temporary Xbox save path shim with a real `sithGamesave_GetProfilePath` implementation.
- Use Xbox-safe storage paths and create missing directories.
- Wire save list population, overwrite/delete behavior, and load confirmation through the real `jkGUISaveLoad` menu.
- Confirm save files round-trip on the Xbox target, not just in the emulator.
- Keep the PC save/load data format unless an Xbox storage constraint forces a wrapper.

Acceptance checks:

- Save menu lists existing saves.
- New save writes to the expected Xbox storage location.
- Load restores the selected save through the original game transition path.
- Delete removes only the selected save file.
- Failed storage operations show a menu-safe error or return cleanly without crashing.
