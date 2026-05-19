#!/usr/bin/env python3
"""
Utilities for Jedi Knight .sft bitmap fonts.

Supported workflows:
  export         Render an existing .sft to PNG plus metadata JSON.
  render-ttf     Build a replacement .sft from an existing .sft template and a TTF/OTF.
  extract-gob    Extract matching files from a GOB archive.

The SFT format is a small SFNT header, one or more charset tables, then an
embedded BM file. This script preserves the original header/table structure
when writing replacement fonts and only replaces the embedded BM payload.
"""

from __future__ import annotations

import argparse
import fnmatch
import json
import struct
from dataclasses import dataclass
from pathlib import Path
from typing import Iterable

try:
    from PIL import Image, ImageDraw, ImageFont
except ImportError as exc:  # pragma: no cover - user-facing dependency check
    raise SystemExit("Pillow is required: python -m pip install Pillow") from exc


HUD_SFT_NAMES = [
    "HelthNum.sft",
    "HelthNum16.sft",
    "ArmorNum.sft",
    "ArmorNum16.sft",
    "ArmorNumsSuper.sft",
    "ArmorNumsSuper16.sft",
    "AmoNums.sft",
    "AmoNums16.sft",
    "AmoNumsSuper.sft",
    "AmoNumsSuper16.sft",
    "msgFont.sft",
    "msgFont16.sft",
]


@dataclass
class RdTexFormat:
    values: tuple[int, ...]

    @property
    def is16bit(self) -> int:
        return self.values[0]

    @property
    def bpp(self) -> int:
        return self.values[1]


@dataclass
class Bitmap:
    header: bytes
    pal_fmt: int
    num_mips: int
    format: RdTexFormat
    mips: list[tuple[int, int, bytes]]
    palette: bytes | None

    @property
    def width(self) -> int:
        return self.mips[0][0]

    @property
    def height(self) -> int:
        return self.mips[0][1]

    @property
    def pixels(self) -> bytes:
        return self.mips[0][2]


@dataclass
class FontEntry:
    glyph_x: int
    width: int


@dataclass
class Charset:
    first: int
    last: int
    entries: list[FontEntry]


@dataclass
class SftFont:
    header: bytes
    margin_y: int
    margin_x: int
    field_10: int
    charsets: list[Charset]
    bitmap_offset: int
    bitmap: Bitmap
    raw: bytes

    def all_chars(self) -> Iterable[tuple[int, FontEntry]]:
        for charset in self.charsets:
            for i, entry in enumerate(charset.entries):
                yield charset.first + i, entry


def read_struct(fmt: str, data: bytes, offset: int) -> tuple[tuple[int, ...], int]:
    size = struct.calcsize(fmt)
    return struct.unpack_from(fmt, data, offset), offset + size


def parse_bitmap(data: bytes, offset: int = 0) -> tuple[Bitmap, int]:
    start = offset
    if data[offset:offset + 4] != b"BM  ":
        raise ValueError(f"Expected BM magic at 0x{offset:X}")

    header = data[offset:offset + 0x80]
    if len(header) != 0x80:
        raise ValueError("Truncated BM header")

    fields = struct.unpack_from("<32I", header, 0)
    version = fields[1]
    if version != 70:
        raise ValueError(f"Unsupported BM version {version}")

    pal_fmt = fields[3]
    num_mips = fields[4]
    tex_format = RdTexFormat(tuple(fields[8:22]))
    offset += 0x80

    if tex_format.bpp not in (8, 16):
        raise ValueError(f"Unsupported BM bpp {tex_format.bpp}")

    bytes_per_pixel = tex_format.bpp // 8
    mips: list[tuple[int, int, bytes]] = []
    for _ in range(num_mips):
        (width, height), offset = read_struct("<II", data, offset)
        byte_count = width * height * bytes_per_pixel
        pixels = data[offset:offset + byte_count]
        if len(pixels) != byte_count:
            raise ValueError("Truncated BM pixel data")
        offset += byte_count
        mips.append((width, height, pixels))

    palette = None
    if pal_fmt & 2:
        palette = data[offset:offset + 0x300]
        if len(palette) != 0x300:
            raise ValueError("Truncated BM palette")
        offset += 0x300

    return Bitmap(data[start:start + 0x80], pal_fmt, num_mips, tex_format, mips, palette), offset


def parse_sft(path: Path) -> SftFont:
    raw = path.read_bytes()
    if raw[:4] != b"SFNT":
        raise ValueError(f"{path} is not an SFT/SFNT font")
    if len(raw) < 44:
        raise ValueError(f"{path} is truncated")

    header = raw[:40]
    fields = struct.unpack_from("<4s9I", header, 0)
    version = fields[1]
    if version != 10:
        raise ValueError(f"Unsupported SFT version {version}")
    margin_y = fields[2]
    margin_x = fields[3]
    field_10 = fields[4]
    num_charsets = fields[5]

    offset = 40
    charsets: list[Charset] = []
    for _ in range(num_charsets):
        (first, last), offset = read_struct("<HH", raw, offset)
        count = last - first + 1
        entries: list[FontEntry] = []
        for _entry_idx in range(count):
            (glyph_x, width), offset = read_struct("<ii", raw, offset)
            entries.append(FontEntry(glyph_x, width))
        charsets.append(Charset(first, last, entries))

    bitmap_offset = offset
    bitmap, end_offset = parse_bitmap(raw, bitmap_offset)
    if end_offset != len(raw):
        # Keep this permissive. Some tools append padding; preserving raw header
        # still keeps replacement output deterministic.
        pass
    return SftFont(header, margin_y, margin_x, field_10, charsets, bitmap_offset, bitmap, raw)


def palette_color(palette: bytes | None, idx: int) -> tuple[int, int, int]:
    if not palette:
        return (idx, idx, idx)
    base = idx * 3
    return (palette[base], palette[base + 1], palette[base + 2])


def bitmap_to_rgba(bitmap: Bitmap) -> Image.Image:
    width, height, pixels = bitmap.mips[0]
    if bitmap.format.bpp == 8:
        out = Image.new("RGBA", (width, height))
        dst = out.load()
        for y in range(height):
            row = y * width
            for x in range(width):
                idx = pixels[row + x]
                r, g, b = palette_color(bitmap.palette, idx)
                dst[x, y] = (r, g, b, 0 if idx == 0 else 255)
        return out

    out = Image.new("RGBA", (width, height))
    dst = out.load()
    vals = struct.unpack_from(f"<{width * height}H", pixels, 0)
    g_bits = bitmap.format.values[3]
    for y in range(height):
        row = y * width
        for x in range(width):
            px = vals[row + x]
            if g_bits == 6:
                r5 = (px >> 11) & 0x1F
                g6 = (px >> 5) & 0x3F
                b5 = px & 0x1F
                dst[x, y] = ((r5 * 527 + 23) >> 6, (g6 * 259 + 33) >> 6, (b5 * 527 + 23) >> 6, 255)
            else:
                a1 = (px >> 15) & 1
                r5 = (px >> 10) & 0x1F
                g5 = (px >> 5) & 0x1F
                b5 = px & 0x1F
                dst[x, y] = ((r5 * 527 + 23) >> 6, (g5 * 527 + 23) >> 6, (b5 * 527 + 23) >> 6, 255 if a1 else 0)
    return out


def choose_foreground_index(bitmap: Bitmap) -> int:
    if bitmap.format.bpp != 8:
        return 1
    counts: dict[int, int] = {}
    for idx in bitmap.pixels:
        if idx:
            counts[idx] = counts.get(idx, 0) + 1
    if not counts:
        return 1
    return max(counts.items(), key=lambda item: item[1])[0]


def choose_foreground_pixel_16(bitmap: Bitmap) -> int:
    if bitmap.format.bpp != 16:
        return 0xFFFF
    count = bitmap.width * bitmap.height
    vals = struct.unpack_from(f"<{count}H", bitmap.pixels, 0)
    colors: set[int] = set()
    for px in vals:
        if px and px != 0xF81F:
            colors.add(px)
    if not colors:
        return 0xFFFF

    def luminance(px: int) -> int:
        r5 = (px >> 11) & 0x1F
        g6 = (px >> 5) & 0x3F
        b5 = px & 0x1F
        r = (r5 * 527 + 23) >> 6
        g = (g6 * 259 + 33) >> 6
        b = (b5 * 527 + 23) >> 6
        return r * 30 + g * 59 + b * 11

    return max(colors, key=luminance)


def metadata(font: SftFont, source: Path) -> dict:
    return {
        "source": str(source),
        "margin_x": font.margin_x,
        "margin_y": font.margin_y,
        "field_10": font.field_10,
        "bitmap": {
            "width": font.bitmap.width,
            "height": font.bitmap.height,
            "bpp": font.bitmap.format.bpp,
            "is16bit": font.bitmap.format.is16bit,
            "pal_fmt": font.bitmap.pal_fmt,
            "num_mips": font.bitmap.num_mips,
            "foreground_index_guess": choose_foreground_index(font.bitmap),
        },
        "charsets": [
            {
                "first": charset.first,
                "last": charset.last,
                "chars": [
                    {
                        "codepoint": charset.first + i,
                        "char": chr(charset.first + i),
                        "glyph_x": entry.glyph_x,
                        "width": entry.width,
                    }
                    for i, entry in enumerate(charset.entries)
                ],
            }
            for charset in font.charsets
        ],
    }


def command_export(args: argparse.Namespace) -> None:
    sft_path = Path(args.sft)
    out_dir = Path(args.out_dir)
    out_dir.mkdir(parents=True, exist_ok=True)
    font = parse_sft(sft_path)

    stem = sft_path.stem
    png_path = out_dir / f"{stem}.png"
    json_path = out_dir / f"{stem}.json"
    glyphs_path = out_dir / f"{stem}_glyphs.png"

    atlas = bitmap_to_rgba(font.bitmap)
    atlas.save(png_path)
    json_path.write_text(json.dumps(metadata(font, sft_path), indent=2), encoding="utf-8")

    # Create a readable glyph contact sheet with vertical separators.
    sheet_h = font.bitmap.height + 18
    sheet = Image.new("RGBA", (font.bitmap.width, sheet_h), (0, 0, 0, 0))
    sheet.alpha_composite(atlas, (0, 0))
    draw = ImageDraw.Draw(sheet)
    for codepoint, entry in font.all_chars():
        x = entry.glyph_x
        draw.line((x, 0, x, font.bitmap.height), fill=(255, 0, 0, 128))
        if entry.width > 0 and 32 <= codepoint <= 126:
            draw.text((x, font.bitmap.height + 1), chr(codepoint), fill=(255, 255, 255, 255))
    sheet.save(glyphs_path)

    print(f"wrote {png_path}")
    print(f"wrote {glyphs_path}")
    print(f"wrote {json_path}")


def render_text_mask(text: str, font_path: Path, px_size: int, pad: int) -> Image.Image:
    font = ImageFont.truetype(str(font_path), px_size)
    bbox = font.getbbox(text)
    width = max(1, bbox[2] - bbox[0] + pad * 2)
    height = max(1, bbox[3] - bbox[1] + pad * 2)
    img = Image.new("L", (width, height), 0)
    draw = ImageDraw.Draw(img)
    draw.text((pad - bbox[0], pad - bbox[1]), text, font=font, fill=255)
    return img


def fit_font_size(font_path: Path, chars: list[str], height: int, max_width: int) -> int:
    sample = "".join(chars) or "0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ"
    lo, hi = 4, max(5, height * 3)
    best = lo
    while lo <= hi:
        mid = (lo + hi) // 2
        mask = render_text_mask(sample, font_path, mid, 1)
        if mask.height <= height and mask.width <= max_width * 2:
            best = mid
            lo = mid + 1
        else:
            hi = mid - 1
    return best


def render_replacement_pixels(template: SftFont, font_path: Path, px_size: int | None, threshold: int, foreground_idx: int | None) -> tuple[bytes, list[Charset]]:
    width = template.bitmap.width
    height = template.bitmap.height
    bpp = template.bitmap.format.bpp
    if bpp not in (8, 16):
        raise ValueError("render-ttf currently writes 8bpp or 16bpp SFT/BM fonts only")

    out = bytearray(width * height * (bpp // 8))
    fg8 = foreground_idx if foreground_idx is not None else choose_foreground_index(template.bitmap)
    fg16 = foreground_idx if foreground_idx is not None else choose_foreground_pixel_16(template.bitmap)

    chars = [chr(cp) for cp, _entry in template.all_chars() if cp >= 32]
    size = px_size or fit_font_size(font_path, chars, height, width)
    pil_font = ImageFont.truetype(str(font_path), size)

    new_charsets: list[Charset] = []
    cursor_x = 0
    for charset in template.charsets:
        new_entries: list[FontEntry] = []
        for i, old_entry in enumerate(charset.entries):
            cp = charset.first + i
            ch = chr(cp)

            if ch.isspace() or old_entry.width <= 0:
                width_hint = max(1, old_entry.width)
                new_entries.append(FontEntry(cursor_x, width_hint))
                cursor_x += width_hint + template.margin_x
                continue

            bbox = pil_font.getbbox(ch)
            glyph_w = max(1, bbox[2] - bbox[0])
            glyph_h = max(1, bbox[3] - bbox[1])
            glyph = Image.new("L", (glyph_w + 2, height), 0)
            draw = ImageDraw.Draw(glyph)
            y = (height - glyph_h) // 2 - bbox[1]
            draw.text((1 - bbox[0], y), ch, font=pil_font, fill=255)

            # If the generated glyph would overrun the original strip, keep the
            # original width and clip. That is preferable to corrupting the BM.
            draw_w = min(glyph.width, max(1, width - cursor_x))
            for gy in range(height):
                for gx in range(draw_w):
                    if glyph.getpixel((gx, gy)) >= threshold:
                        dst_offset = gy * width + cursor_x + gx
                        if bpp == 8:
                            out[dst_offset] = fg8 & 0xFF
                        else:
                            struct.pack_into("<H", out, dst_offset * 2, fg16 & 0xFFFF)
            new_entries.append(FontEntry(cursor_x, draw_w))
            cursor_x += draw_w + template.margin_x
            if cursor_x >= width:
                # Stop advancing outside the strip, but continue writing sane
                # metadata for later chars.
                cursor_x = width - 1
        new_charsets.append(Charset(charset.first, charset.last, new_entries))

    return bytes(out), new_charsets


def rebuild_sft(template: SftFont, pixels: bytes, charsets: list[Charset]) -> bytes:
    if template.bitmap.format.bpp not in (8, 16):
        raise ValueError("Only 8bpp and 16bpp SFT rebuilds are supported")
    expected_len = template.bitmap.width * template.bitmap.height * (template.bitmap.format.bpp // 8)
    if len(pixels) != expected_len:
        raise ValueError("Replacement pixel buffer size does not match template")

    out = bytearray(template.header)
    for charset in charsets:
        out += struct.pack("<HH", charset.first, charset.last)
        for entry in charset.entries:
            out += struct.pack("<ii", entry.glyph_x, entry.width)

    bm = bytearray(template.bitmap.header)
    bm += struct.pack("<II", template.bitmap.width, template.bitmap.height)
    bm += pixels
    # Preserve extra mips as-is, if any. Font strips normally only use mip 0.
    for width, height, mip_pixels in template.bitmap.mips[1:]:
        bm += struct.pack("<II", width, height)
        bm += mip_pixels
    if template.bitmap.palette:
        bm += template.bitmap.palette
    out += bm
    return bytes(out)


def command_render_ttf(args: argparse.Namespace) -> None:
    template_path = Path(args.template_sft)
    font_path = Path(args.font)
    output_path = Path(args.output_sft)
    template = parse_sft(template_path)

    pixels, charsets = render_replacement_pixels(
        template,
        font_path,
        args.size,
        args.threshold,
        args.foreground_index,
    )
    output_path.parent.mkdir(parents=True, exist_ok=True)
    output_path.write_bytes(rebuild_sft(template, pixels, charsets))
    print(f"wrote {output_path}")

    if args.preview:
        preview_path = Path(args.preview)
        preview_path.parent.mkdir(parents=True, exist_ok=True)
        bitmap = Bitmap(
            template.bitmap.header,
            template.bitmap.pal_fmt,
            template.bitmap.num_mips,
            template.bitmap.format,
            [(template.bitmap.width, template.bitmap.height, pixels)] + template.bitmap.mips[1:],
            template.bitmap.palette,
        )
        bitmap_to_rgba(bitmap).save(preview_path)
        print(f"wrote {preview_path}")


def iter_gob_entries(gob_path: Path):
    with gob_path.open("rb") as f:
        header = f.read(12)
        if len(header) != 12:
            raise ValueError("Truncated GOB header")
        magic, version, entry_table_offset = struct.unpack("<4sII", header)
        if magic != b"GOB ":
            raise ValueError("Bad GOB magic")
        if version != 20:
            raise ValueError(f"Unsupported GOB version {version}")
        f.seek(entry_table_offset)
        (num_files,) = struct.unpack("<I", f.read(4))
        for _ in range(num_files):
            entry_raw = f.read(136)
            if len(entry_raw) != 136:
                raise ValueError("Truncated GOB entry table")
            file_offset, file_size, name_raw = struct.unpack("<Ii128s", entry_raw)
            name = name_raw.split(b"\0", 1)[0].decode("latin-1")
            yield name, file_offset, file_size


def command_extract_gob(args: argparse.Namespace) -> None:
    gob_path = Path(args.gob)
    out_dir = Path(args.out_dir)
    patterns = args.pattern or [f"ui/sft/{name}" for name in HUD_SFT_NAMES] + [f"ui\\sft\\{name}" for name in HUD_SFT_NAMES]
    normalized_patterns = [p.lower().replace("/", "\\") for p in patterns]

    out_dir.mkdir(parents=True, exist_ok=True)
    count = 0
    with gob_path.open("rb") as f:
        for name, file_offset, file_size in iter_gob_entries(gob_path):
            normalized = name.lower().replace("/", "\\")
            if not any(fnmatch.fnmatch(normalized, pat) for pat in normalized_patterns):
                continue
            f.seek(file_offset)
            payload = f.read(file_size)
            out_path = out_dir / name.replace("\\", "/")
            out_path.parent.mkdir(parents=True, exist_ok=True)
            out_path.write_bytes(payload)
            print(f"wrote {out_path}")
            count += 1
    print(f"extracted {count} file(s)")


def build_parser() -> argparse.ArgumentParser:
    parser = argparse.ArgumentParser(description=__doc__)
    sub = parser.add_subparsers(dest="command", required=True)

    export = sub.add_parser("export", help="export an SFT atlas PNG and metadata JSON")
    export.add_argument("sft")
    export.add_argument("out_dir")
    export.set_defaults(func=command_export)

    render = sub.add_parser("render-ttf", help="create a replacement SFT from a TTF/OTF and an existing SFT template")
    render.add_argument("template_sft")
    render.add_argument("font")
    render.add_argument("output_sft")
    render.add_argument("--size", type=int, default=None, help="font pixel size; default auto-fits to template height")
    render.add_argument("--threshold", type=int, default=96, help="alpha threshold for indexed output")
    render.add_argument("--foreground-index", type=int, default=None, help="palette index to use for glyph pixels; default infers from template")
    render.add_argument("--preview", default=None, help="optional PNG preview path")
    render.set_defaults(func=command_render_ttf)

    extract = sub.add_parser("extract-gob", help="extract matching files from a GOB")
    extract.add_argument("gob")
    extract.add_argument("out_dir")
    extract.add_argument("--pattern", action="append", help="case-insensitive glob, e.g. 'ui\\sft\\*.sft'; default HUD SFTs")
    extract.set_defaults(func=command_extract_gob)

    return parser


def main() -> None:
    parser = build_parser()
    args = parser.parse_args()
    args.func(args)


if __name__ == "__main__":
    main()
