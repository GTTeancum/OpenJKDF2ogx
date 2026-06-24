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

For now, multiplayer remains on JK force powers. MotS force powers are only enabled for single-player MotS mode.
