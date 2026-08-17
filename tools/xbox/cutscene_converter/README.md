# OpenJKDF2x Cutscene Converter

Place `OpenJKDF2xCutsceneConverter.exe` in the same folder as `default.xbe`,
or place it inside a `cutscene_converter` folder directly under that game
folder. The tool auto-detects the game folder; there is no folder picker.

Run:

```text
OpenJKDF2xCutsceneConverter.exe
```

The converter scans the full game folder recursively for `.SMK` and `.SAN`
videos, writes `.XMV` files beside them, and asks up front whether to delete
originals after each converted XMV passes probe and decode validation.

The GUI includes two startup options:

- Delete original `.SMK` / `.SAN` files after each matching `.XMV` validates
- Rebuild existing `.XMV` files

Command-line fallback:

```powershell
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -KeepOriginals
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -DeleteOriginals
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -Force
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -DryRun
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -GameRoot "C:\Games\Emulators\CXBX\openJKDF2x"
```

`ffmpeg.exe` and `ffprobe.exe` must be packaged beside this tool, under
`ffmpeg\bin`, under `tools\ffmpeg\bin`, or available on `PATH`.

For beta/release packaging, include `THIRD_PARTY_NOTICES.txt`,
`FFMPEG_LICENSE.txt`, and `FFMPEG_GYAN_README.txt` beside the EXE whenever
FFmpeg is bundled.

To build the GUI:

```powershell
.\Build-CutsceneConverterGui.ps1 -Clean
```
