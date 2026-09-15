# Multiplayer HUD

JK HUD Revamp by Quib Mask, approved at one-third scale on 2026-09-14.
Source: https://www.moddb.com/mods/jedi-knight-enhanced/addons/jk-hud-revamp
Original permission/credits text is retained unchanged in jkhud.txt.

The eight BM and six SFT files are unchanged from the download (archive MD5
8b00600244779326b2c2dd4bc1649a26). They live under Resource/ui/mp_hud and are
selected by jkHud_Open only when sithNet_isMulti is true. Stock paths remain in
use for single-player. The prior scale is restored by jkHud_Close.

The original mod's pow_supershield.cog is not installed as a global replacement:
this integration replaces HUD visuals; the port retains its existing powerup
and per-player supershield handling. No gameplay scripts are overridden.

Build and XEMU staging include the namespaced assets and original credit file.
The preview marker is no longer used.

Validation: release build and test_mp_hud_lifecycle.py passed; all 14 packaged assets match the archive byte-for-byte. Native four-player capture 20260914_122532/12:26:23 matches the approved layout. Dedicated single-player run 20260914_122900 shows the level loading background (12:29:35) and stock circular HUD in gameplay (12:30:09). Earlier 20260914_122716 used multiplayer autostart incorrectly and is excluded from single-player validation. Mode transitions, one-local-player ready/start, and its dedicated HUD layout remain in the active goal. Audible output was not assessed.

One-local-player follow-up: source inspection confirms ready accepts one joined player and host setup permits zero bots. End-to-end ready/setup/start is still unverified. Run 20260914_123344 confirmed a one-player/zero-bot MP world but exposed the 4:3-centered HUD path and default requested-count mismatch. Current change selects 2/3 scale for a full view and uses viewport edge anchoring in the ordinary multiplayer renderer; 2–4 divided views retain 1/3. Lifecycle tests now include both ordinary and split-requested one-player paths. Native confirmation of this correction is pending.
Native confirmation: 20260914_123621-mp-hud-one-anchored at 12:37:02 inspected; one-player MP now uses full-width edge anchoring with the dedicated scale. Gameplay/background and both HUD panels are visible. This autostart run does not validate ready/setup menu actions.
