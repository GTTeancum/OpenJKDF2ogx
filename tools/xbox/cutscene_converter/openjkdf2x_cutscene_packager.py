#!/usr/bin/env python3
"""Convert Jedi Knight/MotS SMK/SAN cutscenes to Xbox XMV files.

This is intentionally narrow: it writes the XMV profile used by OpenJKDF2x on
Xbox, with one WMV2 video stream and optional stereo PCM audio.
"""

from __future__ import annotations

import argparse
import json
import os
import shutil
import struct
import subprocess
import sys
import tempfile
from dataclasses import dataclass
from pathlib import Path


VIDEO_EXTENSIONS = {".smk", ".san"}
DEFAULT_WIDTH = 640
DEFAULT_HEIGHT = 480
DEFAULT_FPS = 15
DEFAULT_VIDEO_BITRATE = "1200k"
DEFAULT_AUDIO_RATE = 22050
PACKET_ALIGN = 4096


@dataclass
class AviVideo:
    width: int
    height: int
    extradata: bytes
    frames: list[bytes]


@dataclass
class WaveAudio:
    channels: int
    sample_rate: int
    bits_per_sample: int
    data: bytes


def log(message: str) -> None:
    print(message, flush=True)


def run(cmd: list[str]) -> None:
    proc = subprocess.run(cmd)
    if proc.returncode != 0:
        raise RuntimeError(f"command failed with exit code {proc.returncode}: {' '.join(cmd)}")


def find_executable(name: str, roots: list[Path]) -> Path | None:
    candidates: list[Path] = []
    if getattr(sys, "frozen", False):
        candidates.append(Path(sys.executable).resolve().parent / name)
    pyinstaller_root = getattr(sys, "_MEIPASS", None)
    if pyinstaller_root:
        candidates.append(Path(pyinstaller_root) / name)

    for root in roots:
        candidates.extend(
            [
                root / name,
                root / "tools" / name,
                root / "tools" / "ffmpeg" / "bin" / name,
                root / "ffmpeg" / "bin" / name,
                root / "cutscene_converter" / name,
            ]
        )

    for candidate in candidates:
        if candidate.is_file():
            return candidate

    found = shutil.which(name)
    if found:
        return Path(found)

    winget = Path(os.environ.get("LOCALAPPDATA", "")) / "Microsoft" / "WinGet" / "Packages"
    if winget.is_dir():
        matches = sorted(winget.rglob(name))
        if matches:
            return matches[0]

    return None


def iter_riff_chunks(data: bytes, start: int, end: int):
    pos = start
    while pos + 8 <= end:
        chunk_id = data[pos : pos + 4]
        size = struct.unpack_from("<I", data, pos + 4)[0]
        payload = pos + 8
        chunk_end = payload + size
        if chunk_end > len(data):
            raise ValueError(f"RIFF chunk {chunk_id!r} runs past end of file")
        yield chunk_id, payload, size
        pos = chunk_end + (size & 1)


def find_list_ranges(data: bytes, list_type: bytes):
    for chunk_id, payload, size in iter_riff_chunks(data, 12, len(data)):
        if chunk_id == b"LIST" and data[payload : payload + 4] == list_type:
            yield payload + 4, payload + size


def parse_avi(path: Path) -> AviVideo:
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"AVI ":
        raise ValueError(f"{path} is not an AVI file")

    width = 0
    height = 0
    extradata = b"\x00\x00\x00\x00"

    for start, end in find_list_ranges(data, b"hdrl"):
        for chunk_id, payload, size in iter_riff_chunks(data, start, end):
            if chunk_id != b"LIST" or data[payload : payload + 4] != b"strl":
                continue

            strl_start = payload + 4
            strl_end = payload + size
            stream_type = None
            stream_strf = None

            for cid, cpayload, csize in iter_riff_chunks(data, strl_start, strl_end):
                if cid == b"strh" and csize >= 8:
                    stream_type = data[cpayload : cpayload + 4]
                elif cid == b"strf":
                    stream_strf = data[cpayload : cpayload + csize]

            if stream_type == b"vids" and stream_strf:
                header_size = struct.unpack_from("<I", stream_strf, 0)[0]
                if header_size < 40 or len(stream_strf) < header_size:
                    raise ValueError("invalid AVI video format header")
                width = struct.unpack_from("<i", stream_strf, 4)[0]
                height = abs(struct.unpack_from("<i", stream_strf, 8)[0])
                compression = stream_strf[16:20]
                if compression != b"WMV2":
                    raise ValueError(f"AVI video stream is {compression!r}, expected WMV2")
                if header_size >= 44 and len(stream_strf) >= header_size:
                    extradata = stream_strf[40:44]
                break

    frames: list[bytes] = []
    for start, end in find_list_ranges(data, b"movi"):
        for chunk_id, payload, size in iter_riff_chunks(data, start, end):
            if len(chunk_id) == 4 and chunk_id[:2].isdigit() and chunk_id[2:] in (b"dc", b"db"):
                if size:
                    frames.append(data[payload : payload + size])

    if not width or not height:
        raise ValueError("could not find AVI WMV2 stream header")
    if not frames:
        raise ValueError("could not find AVI video frames")

    return AviVideo(width=width, height=height, extradata=extradata, frames=frames)


def parse_wave(path: Path) -> WaveAudio:
    data = path.read_bytes()
    if data[:4] != b"RIFF" or data[8:12] != b"WAVE":
        raise ValueError(f"{path} is not a WAVE file")

    fmt = None
    pcm = None
    for chunk_id, payload, size in iter_riff_chunks(data, 12, len(data)):
        if chunk_id == b"fmt ":
            fmt = data[payload : payload + size]
        elif chunk_id == b"data":
            pcm = data[payload : payload + size]

    if not fmt or pcm is None:
        raise ValueError(f"{path} is missing WAVE fmt/data chunks")
    if len(fmt) < 16:
        raise ValueError("WAVE fmt chunk is too short")

    format_tag, channels, sample_rate, _byte_rate, _block_align, bits = struct.unpack_from("<HHIIHH", fmt, 0)
    if format_tag != 1:
        raise ValueError(f"WAVE audio must be PCM, got format tag 0x{format_tag:04x}")
    if channels not in (1, 2):
        raise ValueError(f"WAVE audio must be mono/stereo, got {channels} channels")
    if bits != 16:
        raise ValueError(f"WAVE audio must be 16-bit PCM, got {bits}-bit")

    return WaveAudio(channels=channels, sample_rate=sample_rate, bits_per_sample=bits, data=pcm)


def std_wmv2_extra_to_xmv(extra: bytes) -> bytes:
    extra = (extra + b"\x00\x00\x00\x00")[:4]
    data = int.from_bytes(extra, "big")

    xmv = 0
    xmv |= ((data >> 15) & 1) << 0
    xmv |= ((data >> 14) & 1) << 1
    xmv |= ((data >> 13) & 1) << 2
    xmv |= ((data >> 12) & 1) << 3
    xmv |= ((data >> 11) & 1) << 4
    xmv |= ((data >> 10) & 1) << 5
    xmv |= ((data >> 7) & 7) << 6
    return xmv.to_bytes(4, "little")


def swap_words_for_xmv(frame: bytes) -> bytes:
    padded = bytearray(frame)
    while len(padded) % 4:
        padded.append(0)
    for i in range(0, len(padded), 4):
        padded[i : i + 4] = padded[i : i + 4][::-1]
    return bytes(padded)


def packet_time_ms(frame_index: int, fps: int) -> int:
    if frame_index == 0:
        return 0
    prev = round((frame_index - 1) * 1000 / fps)
    cur = round(frame_index * 1000 / fps)
    return max(1, cur - prev)


def align_size(size: int, alignment: int) -> int:
    return (size + alignment - 1) // alignment * alignment


def write_xmv(path: Path, video: AviVideo, audio: WaveAudio | None, fps: int) -> None:
    if fps <= 0:
        raise ValueError("fps must be positive")

    audio_tracks = 1 if audio and audio.data else 0
    audio_frame_bytes = 0
    if audio_tracks:
        bytes_per_sample = audio.channels * audio.bits_per_sample // 8
        audio_frame_bytes = int(audio.sample_rate * bytes_per_sample / fps)
        audio_frame_bytes -= audio_frame_bytes % bytes_per_sample
        if audio_frame_bytes <= 0:
            raise ValueError("computed audio frame size is zero")

    xmv_extra = std_wmv2_extra_to_xmv(video.extradata)
    packets: list[bytes] = []
    audio_offset = 0

    for index, raw_frame in enumerate(video.frames):
        frame = swap_words_for_xmv(raw_frame)
        if len(frame) > (0x1FFFF * 4 + 4):
            raise ValueError(f"video frame {index} is too large for the narrow XMV packet profile")

        frame_header = ((len(frame) - 4) // 4) | (packet_time_ms(index, fps) << 17)
        video_payload = struct.pack("<I", frame_header) + frame
        if index == 0:
            video_payload = xmv_extra + video_payload
        stored_video_size = len(video_payload) + audio_tracks * 4

        audio_payload = b""
        if audio_tracks and audio:
            if index + 1 == len(video.frames):
                audio_payload = audio.data[audio_offset:]
            else:
                audio_payload = audio.data[audio_offset : audio_offset + audio_frame_bytes]
            audio_offset += len(audio_payload)

        header = bytearray()
        header += b"\x00\x00\x00\x00"
        video_header = stored_video_size | (1 << 23)
        if index == 0:
            video_header |= 0x80000000
        header += struct.pack("<I", video_header)
        header += b"\x00\x00\x00\x00"
        if audio_tracks:
            header += struct.pack("<I", len(audio_payload))

        packet = bytes(header) + video_payload + audio_payload
        packet += b"\x00" * (align_size(len(packet), PACKET_ALIGN) - len(packet))
        packets.append(packet)

    for i in range(len(packets)):
        next_size = len(packets[i + 1]) if i + 1 < len(packets) else 0
        packets[i] = struct.pack("<I", next_size) + packets[i][4:]

    header_size = 36 + audio_tracks * 12
    first_packet_total = align_size(header_size + len(packets[0]), PACKET_ALIGN)
    max_packet_size = max([first_packet_total] + [len(packet) for packet in packets[1:]])
    duration_ms = round(len(video.frames) * 1000 / fps)

    global_header = bytearray()
    global_header += struct.pack("<I", len(packets[1]) if len(packets) > 1 else 0)
    global_header += struct.pack("<I", first_packet_total)
    global_header += struct.pack("<I", max_packet_size)
    global_header += b"xobX"
    global_header += struct.pack("<I", 4)
    global_header += struct.pack("<I", video.width)
    global_header += struct.pack("<I", video.height)
    global_header += struct.pack("<I", duration_ms)
    global_header += struct.pack("<H", audio_tracks)
    global_header += b"\x01\x00"

    if audio_tracks and audio:
        global_header += struct.pack("<H", 1)
        global_header += struct.pack("<H", audio.channels)
        global_header += struct.pack("<I", audio.sample_rate)
        global_header += struct.pack("<H", audio.bits_per_sample)
        global_header += struct.pack("<H", 0)

    with path.open("wb") as f:
        f.write(global_header)
        f.write(packets[0])
        f.write(b"\x00" * (first_packet_total - header_size - len(packets[0])))
        for packet in packets[1:]:
            f.write(packet)


def source_has_audio(ffprobe: Path, source: Path) -> bool:
    proc = subprocess.run(
        [
            str(ffprobe),
            "-v",
            "error",
            "-select_streams",
            "a:0",
            "-show_entries",
            "stream=index",
            "-of",
            "csv=p=0",
            str(source),
        ],
        capture_output=True,
        text=True,
    )
    return proc.returncode == 0 and bool(proc.stdout.strip())


def encode_intermediates(ffmpeg: Path, ffprobe: Path, source: Path, avi: Path, wav: Path, width: int, height: int, fps: int, bitrate: str) -> bool:
    vf = (
        f"scale=w={width}:h={height}:force_original_aspect_ratio=decrease,"
        f"pad={width}:{height}:(ow-iw)/2:(oh-ih)/2:black,format=yuv420p"
    )
    run(
        [
            str(ffmpeg),
            "-y",
            "-hide_banner",
            "-loglevel",
            "error",
            "-i",
            str(source),
            "-map",
            "0:v:0",
            "-vf",
            vf,
            "-r",
            str(fps),
            "-an",
            "-c:v",
            "wmv2",
            "-b:v",
            bitrate,
            "-g",
            str(fps),
            "-bf",
            "0",
            "-f",
            "avi",
            str(avi),
        ]
    )

    if not source_has_audio(ffprobe, source):
        return False

    audio_cmd = [
        str(ffmpeg),
        "-y",
        "-hide_banner",
        "-loglevel",
        "error",
        "-i",
        str(source),
        "-map",
        "0:a:0",
        "-vn",
        "-ac",
        "2",
        "-ar",
        str(DEFAULT_AUDIO_RATE),
        "-c:a",
        "pcm_s16le",
        str(wav),
    ]
    proc = subprocess.run(audio_cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"ffmpeg audio encode failed for {source.name}: {proc.stderr.strip()}")
    return proc.returncode == 0 and wav.is_file() and wav.stat().st_size > 44


def ffprobe_validate(ffprobe: Path, xmv: Path) -> None:
    cmd = [
        str(ffprobe),
        "-v",
        "error",
        "-show_streams",
        "-print_format",
        "json",
        str(xmv),
    ]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    if proc.returncode != 0:
        raise RuntimeError(f"ffprobe could not read generated XMV: {proc.stderr.strip()}")

    parsed = json.loads(proc.stdout)
    streams = parsed.get("streams", [])
    if not any(stream.get("codec_name") == "wmv2" for stream in streams):
        raise RuntimeError("ffprobe did not find a WMV2 video stream in generated XMV")


def ffmpeg_decode_validate(ffmpeg: Path, xmv: Path) -> None:
    proc = subprocess.run(
        [
            str(ffmpeg),
            "-hide_banner",
            "-v",
            "warning",
            "-i",
            str(xmv),
            "-f",
            "null",
            "-",
        ],
        capture_output=True,
        text=True,
    )
    output = (proc.stderr or "") + (proc.stdout or "")
    fatal_markers = ("error", "overflow", "invalid", "failed")
    fatal_lines = [
        line
        for line in output.splitlines()
        if any(marker in line.lower() for marker in fatal_markers)
    ]
    if proc.returncode != 0 or fatal_lines:
        sample = "\n".join(fatal_lines[:12]) or output.strip()
        raise RuntimeError(f"ffmpeg decode validation failed for generated XMV:\n{sample}")


def find_video_sources(game_root: Path) -> list[Path]:
    out: list[Path] = []
    for path in game_root.rglob("*"):
        if not path.is_file():
            continue
        if path.suffix.lower() in VIDEO_EXTENSIONS:
            if any(part.lower() in {"__converted_originals", "backup", "backups"} for part in path.parts):
                continue
            out.append(path)
    return sorted(out, key=lambda p: str(p).lower())


def convert_one(args, ffmpeg: Path, ffprobe: Path, source: Path) -> bool:
    out = source.with_suffix(".XMV")
    if out.exists() and not args.force:
        log(f"skip: {source.relative_to(args.game_root)} already has {out.name}")
        return False

    log(f"convert: {source.relative_to(args.game_root)}")
    if args.dry_run:
        return False

    with tempfile.TemporaryDirectory(prefix="openjkdf2x_cutscene_") as tmp_str:
        tmp = Path(tmp_str)
        avi = tmp / "video.avi"
        wav = tmp / "audio.wav"

        has_audio = encode_intermediates(ffmpeg, ffprobe, source, avi, wav, args.width, args.height, args.fps, args.video_bitrate)
        video = parse_avi(avi)
        audio = parse_wave(wav) if has_audio else None

        temp_out = out.with_suffix(".xmv.tmp")
        if temp_out.exists():
            temp_out.unlink()
        write_xmv(temp_out, video, audio, args.fps)
        ffprobe_validate(ffprobe, temp_out)
        ffmpeg_decode_validate(ffmpeg, temp_out)
        temp_out.replace(out)

    log(f"  wrote: {out.relative_to(args.game_root)} ({out.stat().st_size / 1024 / 1024:.1f} MB)")
    if args.delete_originals:
        source.unlink()
        log(f"  deleted original: {source.relative_to(args.game_root)}")
    return True


def parse_args(argv: list[str]):
    parser = argparse.ArgumentParser(description="Convert JK/MotS SMK/SAN cutscenes to OpenJKDF2x Xbox XMV files.")
    parser.add_argument("--game-root", type=Path, default=Path.cwd(), help="Folder containing default.xbe; scanned recursively.")
    parser.add_argument("--force", action="store_true", help="Overwrite existing .xmv files.")
    parser.add_argument("--delete-originals", action="store_true", help="Delete each original SMK/SAN after its XMV validates.")
    parser.add_argument("--dry-run", action="store_true", help="List work without converting.")
    parser.add_argument("--width", type=int, default=DEFAULT_WIDTH)
    parser.add_argument("--height", type=int, default=DEFAULT_HEIGHT)
    parser.add_argument("--fps", type=int, default=DEFAULT_FPS)
    parser.add_argument("--video-bitrate", default=DEFAULT_VIDEO_BITRATE)
    parser.add_argument("--max-files", type=int, default=0, help=argparse.SUPPRESS)
    return parser.parse_args(argv)


def main(argv: list[str]) -> int:
    args = parse_args(argv)
    args.game_root = args.game_root.resolve()
    if not args.game_root.is_dir():
        log(f"error: game root not found: {args.game_root}")
        return 2

    roots = [Path(__file__).resolve().parent, args.game_root, Path.cwd()]
    ffmpeg = find_executable("ffmpeg.exe", roots) or find_executable("ffmpeg", roots)
    ffprobe = find_executable("ffprobe.exe", roots) or find_executable("ffprobe", roots)
    if not ffmpeg or not ffprobe:
        log("error: ffmpeg/ffprobe not found. Put them beside this tool, under tools/ffmpeg/bin, or on PATH.")
        return 2

    sources = find_video_sources(args.game_root)
    if args.max_files:
        sources = sources[: args.max_files]

    log(f"OpenJKDF2x cutscene packager")
    log(f"root: {args.game_root}")
    log(f"ffmpeg: {ffmpeg}")
    log(f"ffprobe: {ffprobe}")
    log(f"videos found: {len(sources)}")
    if args.delete_originals:
        log("delete originals: yes, after each generated XMV validates")
    if not sources:
        return 0

    converted = 0
    failed = 0
    for source in sources:
        try:
            if convert_one(args, ffmpeg, ffprobe, source):
                converted += 1
        except Exception as exc:
            failed += 1
            log(f"  FAILED: {source.relative_to(args.game_root)}: {exc}")

    log(f"done: converted={converted} failed={failed}")
    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main(sys.argv[1:]))
