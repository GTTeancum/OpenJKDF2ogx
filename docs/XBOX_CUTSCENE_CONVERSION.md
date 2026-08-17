# Xbox Cutscene Conversion

OpenJKDF2x plays Xbox cutscenes as `.xmv` files for streaming performance. The
beta package should include `OpenJKDF2xCutsceneConverter.exe` next to
`default.xbe`, or inside a `cutscene_converter` folder directly under it.

## User Flow

1. Extract the beta package into the game root containing `default.xbe`.
2. Ensure the original JK/MotS game files are already copied into the same tree.
3. Run:

   ```text
   OpenJKDF2xCutsceneConverter.exe
   ```

The GUI auto-detects the game folder and does not offer a folder picker. It
scans the full tree recursively for `.SMK` and `.SAN` files and writes matching
`.XMV` files beside each original. It asks up front whether to delete original
videos after each generated XMV validates.

## Non-Interactive Options

```powershell
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -GameRoot . -KeepOriginals
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -GameRoot . -DeleteOriginals
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -GameRoot . -Force
.\cutscene_converter\Convert-CutscenesToXmv.ps1 -GameRoot . -DryRun
```

## Packaging Notes

Build the GUI with:

```powershell
.\tools\xbox\cutscene_converter\Build-CutsceneConverterGui.ps1 -Clean
```

The onefile build bundles `ffmpeg` and `ffprobe` when the build script can find
them. If they are not bundled, the converter expects them either beside the tool,
under `tools/ffmpeg/bin`, under `ffmpeg/bin`, or on `PATH`. For a packaged beta,
include `THIRD_PARTY_NOTICES.txt`, `FFMPEG_LICENSE.txt`, and
`FFMPEG_GYAN_README.txt` beside the EXE when FFmpeg is bundled.

The XMV writer is narrow by design:

- WMV2 video
- 640x480 letterboxed output by default
- 15 FPS by default
- Stereo 22050 Hz PCM audio when the source has audio
- One XMV packet per video frame
- `xmvtool`-compatible first-packet sizing and alignment

Generated files are validated with FFmpeg's XMV demuxer and a full decode pass
before originals are deleted.

## Verification Notes

The converter output must be validated beyond desktop decoding. A prior writer
produced files that FFmpeg and `xmvtool -d` could inspect, but the Xbox runtime
decoder stalled before returning an XMV descriptor. The fixed writer pads the
first packet as one aligned packet while preserving the expected packet-size
word at the start of packet data.

Current XEMU proofs:

- JK intro `01-02a.XMV`: opened through `01-02a.smk`, audio stream enabled,
  smoke skip completed, and the menu repainted.
- MotS intro `JKMINTRO.XMV`: opened through `jkmintro.san`, smoke skip
  completed, and the menu repainted.
