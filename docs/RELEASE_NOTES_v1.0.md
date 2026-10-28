# OpenJKDF2x Xbox v1.0

The original Xbox release now identifies itself as **v1.0** on the main menu,
in the bottom right over the controller footer.

## Updates since 0.9b

- Revised Xbox setup and controls menus, display calibration, and safe-zone markers.
- Local multiplayer bots, revised multiplayer HUD, and navigation improvements
  for low ceilings, ramps, and optional pickups that would strand a bot.
- Modern saber rendering with rounded tips and attack trails. Boss trails use
  actual attack animation windows and remain disabled with stock saber selected.
- Expanded native Mysteries of the Sith support, including weapon/Force wheel
  handling, multiplayer loadouts, architecture lighting, and translucent surfaces.
- Automatic selection of 720p when enabled and supported by the console, with
  lower-resolution fallback. Uses 32-bit color at 720p and releases menu textures
  before level loading to preserve memory.
- Includes the maintained Xbox Force Jump patch and the multiplayer HUD by
  Quib Mask, with its original permission and credit file.

## Install

Extract `OpenJKDF2x-Xbox-v1.0.zip`, copy its folder to your Xbox installation,
and add your legally owned game data. Launch `default.xbe`.

For Jedi Knight, keep the original resource archives in `Resource/` and episode
archives in `Episode/`. For native MotS, also copy `JKMRES.GOO` and
`JKMsndLO.goo` into `Resource/`, and the original MotS episode `.GOO` files into
`Episode/`. Select MotS through the game menus. Episode files alone are not enough.
The old JK-mode `mots_xbox_patch.gob` compatibility overlay is not required for
native MotS.

Convert your own cutscene files to Xbox XMV using the included converter;
see `CUTSCENE_CONVERSION_README.txt` for instructions and third-party licenses.
Retail archives, videos, saves, and generated MotS compatibility packs are not
included. The six optional MotS weapon-wheel bitmap icons derived from retail
models are also excluded; an existing local installation may retain its icons.

## Verification and remaining limitations

- Passed XEMU main-menu inspection on 2026-10-28: the actual v1.0 label is
  visible at the bottom right over the footer, with the menu background and
  controls intact. No level was loaded. Audio was muted for this check.
- The 720p output revision previously passed bounded console menu, single-player,
  and multiplayer checks. Confirmation of the external OBS/TV picture remains
  outstanding; framebuffer readback alone cannot establish that it is fixed.
- System Link still needs broader real-console and multi-device validation.
- Bot navigation is improved, not guaranteed stall-free. Some CTF doorways
  intentionally lock temporarily after a flag pickup.
- This check does not certify every level, boss animation, audio, or cutscene.

OpenJKDF2x is an engine port. Jedi Knight, Mysteries of the Sith, STAR WARS,
and related properties belong to their respective owners. Supply your own game data.
