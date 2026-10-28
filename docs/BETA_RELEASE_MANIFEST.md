# OpenJKDF2x Xbox Beta Release Manifest

## v1.0 packaging update (2026-10-28)

For v1.0, use `scripts/xbox/package_release.py`. Its explicit source allowlist
supersedes the historical 0.9b layout below. The folder/archive is named
`OpenJKDF2x-Xbox-v1.0`; it adds `UPDATE_NOTES.md`, current controller glyphs,
the maintained `mods/xbox_patch.gob` gameplay override, and the namespaced
multiplayer HUD with unchanged `jkhud.txt` distribution permission and credits.
These two maintained runtime components are exceptions to the old blanket
mod exclusion. No arbitrary mods or retail archives are permitted.

The six retail-model-derived `xw_mots_*.bm` icons remain local and are excluded.
Retail game archives, saves, compatibility packs, videos, diagnostics, and smoke
markers remain excluded. The script verifies every ZIP entry and hash against
its allowlist, checks forbidden file types, and writes a manifest and checksum.

Build the Xbox executable and converter first. Validate v1.0 at the bottom right
of the actual XEMU main menu over the footer, without loading a level. The old
CXBX-R icon UI step below is superseded by this requested menu-only acceptance
check and the prohibition on host UI automation.

The older 0.9b MotS compatibility instructions below are historical. Native MotS
uses the original resource and episode GOO files supplied by the user; see
`RELEASE_NOTES_v1.0.md` and `MOTS_XBOX_COMPAT.md`.

Date: 2026-08-18

This manifest is the source of truth for the public Xbox beta zip. The beta package is an engine/tools package only: it must not include retail LucasArts game data, generated MotS compatibility archives, user saves, test logs, or mods.

Do not package the CXBX-R staging folder wholesale. `C:\Games\Emulators\CXBX\openJKDF2x` is a test install and can contain retail files, local generated files, profiles, mods, logs, and smoke-test artifacts.

## Required Zip Layout

```text
OpenJKDF2x-Xbox-Beta/
  default.xbe
  TitleImage.xbx
  SaveImage.xbx
  TitleMeta.xbx
  box art.png
  README.md
  LICENSE.md
  OpenJKDF2xCutsceneConverter.exe
  CUTSCENE_CONVERSION_README.txt
  Resource/
    ui/
      bm/
        xbtn_a.bm
        xbtn_b.bm
        xbtn_back.bm
        xbtn_black.bm
        xbtn_lt.bm
        xbtn_rt.bm
        xbtn_start.bm
        xbtn_white.bm
        xbtn_x.bm
        xbtn_y.bm
        xbtn_tc_a.bm
        xbtn_tc_b.bm
        xbtn_tc_back.bm
        xbtn_tc_black.bm
        xbtn_tc_lt.bm
        xbtn_tc_rt.bm
        xbtn_tc_start.bm
        xbtn_tc_white.bm
        xbtn_tc_x.bm
        xbtn_tc_y.bm
        xw_bryar.bm
        xw_conc.bm
        xw_fists.bm
        xw_rail.bm
        xw_repeater.bm
        xw_saber.bm
        xw_seqchg.bm
        xw_strifle.bm
        xw_thermal.bm
        xw_tusken.bm
```

## Source Paths

Copy these files into the zip layout above:

```text
build/xbox/release/default.xbe                         -> default.xbe
build/xbox/release/TitleImage.xbx                      -> TitleImage.xbx
build/xbox/release/SaveImage.xbx                       -> SaveImage.xbx
build/xbox/release/TitleMeta.xbx                       -> TitleMeta.xbx
build/xbox/release/Resource/ui/bm/*.bm                 -> Resource/ui/bm/
box art.png                                            -> box art.png
README.md                                              -> README.md
LICENSE.md                                             -> LICENSE.md
tools/xbox/cutscene_converter/dist/OpenJKDF2xCutsceneConverter.exe -> OpenJKDF2xCutsceneConverter.exe
tools/xbox/cutscene_converter/dist/CUTSCENE_CONVERSION_README.txt -> CUTSCENE_CONVERSION_README.txt
  inputs:
    tools/xbox/cutscene_converter/README.md
    tools/xbox/cutscene_converter/dist/THIRD_PARTY_NOTICES.txt
    tools/xbox/cutscene_converter/dist/FFMPEG_LICENSE.txt
    tools/xbox/cutscene_converter/dist/FFMPEG_GYAN_README.txt
```

## Must Not Ship

Retail files and retail-derived generated files are excluded from the public beta zip. Users supply their own game data.

Do not include:

- `Episode/`
- `docs/`
- `MUSIC/`
- retail `Resource/` content, except the engine-owned `Resource/ui/bm/*.bm` files listed above
- `Resource/VIDEO/*.SMK`
- `Resource/VIDEO/*.SAN`
- converted `Resource/VIDEO/*.XMV`
- `*.GOO`
- `*.GOB`, including `mods/mots_xbox_patch.gob`
- `mods/`, including Glowsaber, RBots, test maps, or any other third-party mod
- `cutscene_converter/`
- `player/`
- save data, profiles, controller profiles, or generated portraits
- `*.jkp`
- `*.bnav`
- `*.phaselink`
- `debug_openjkdf2.txt`
- `build_log.txt`
- `*.log`
- smoke-test folders and harness outputs
- `obj/`
- `openjkdf2_xbox.exe`
- `openjkdf2_xbox.exe.map`
- `xbe_dump_dashboard_icon.txt`
- `ConMan.exe`
- separate cutscene converter notice/license files; their contents belong in `CUTSCENE_CONVERSION_README.txt`
- source art or build inputs such as `box art.psd`, `*.rdf`, and source `*.bmp` dashboard images

## MotS Compatibility

MotS support must remain user-generated from the user's own MotS install. The beta zip can mention the process in release notes or the root `README.md`, but it must not include generated MotS archives, copied MotS resources, or the repo `docs/` folder.

Expected generated local files, not shipped:

```text
Episode/MOTS_SP.GOO
Episode/MOTS_DM.GOO
Episode/MOTS_CTF.GOO
Episode/MOTS_SABER.GOO
Resource/JKMRES.GOO
Resource/JKMsndLO.goo
mods/mots_xbox_patch.gob
```

If MotS setup is advertised in the beta notes, the notes must make clear that these files are created locally from a user-owned MotS install and are not included in the beta package.

## Validation Before Zipping

Before making the public beta zip:

- Build the Xbox target and use fresh `build/xbox/release` outputs.
- Build the cutscene converter with `tools/xbox/cutscene_converter/Build-CutsceneConverterGui.ps1 -Clean`.
- Confirm `OpenJKDF2xCutsceneConverter.exe` is present at the zip root.
- Confirm `CUTSCENE_CONVERSION_README.txt` is present at the zip root and contains the converter usage notes, third-party notices, FFmpeg license, and FFmpeg build readme.
- Confirm `box art.png` is present and `box art.psd` is absent.
- Confirm dashboard sidecars are present: `TitleImage.xbx`, `SaveImage.xbx`, and `TitleMeta.xbx`.
- Open `default.xbe` in CXBX-R without launching and confirm the dashboard icon appears.
- Scan the finished zip contents for forbidden extensions and folders before upload.

Recommended final package scan:

```powershell
$zipRoot = "C:\Path\To\OpenJKDF2x-Xbox-Beta"
Get-ChildItem -LiteralPath $zipRoot -Recurse -File |
    Where-Object {
        $_.Extension -match '^\.(gob|goo|jkl|mat|snd|smk|san|xmv|jkp|bnav|phaselink|log|map|obj|psd)$' -or
        $_.FullName -match '\\(Episode|MUSIC|mods|player|obj|docs|cutscene_converter)\\' -or
        $_.Name -in @('THIRD_PARTY_NOTICES.txt', 'FFMPEG_LICENSE.txt', 'FFMPEG_GYAN_README.txt')
    } |
    Select-Object FullName
```

The scan should return no files.
