#!/usr/bin/env python3
"""Generate Xbox Duke button glyph BM files from xbox_duke_buttons.png."""

from __future__ import annotations

import argparse
import struct
from pathlib import Path

from PIL import Image


DEFAULT_RES2 = Path(r"C:\Games\Emulators\CXBX\openJKDF2x\Resource\Res2.gob")

BUTTONS = {
    "xbtn_a": (56, 40, 184, 200),
    "xbtn_b": (246, 40, 374, 200),
    "xbtn_x": (436, 40, 564, 200),
    "xbtn_y": (626, 40, 754, 200),
    "xbtn_white": (816, 56, 944, 184),
    "xbtn_black": (986, 56, 1098, 156),
    "xbtn_start": (1176, 92, 1264, 148),
    "xbtn_back": (1196, 332, 1284, 388),
    "xbtn_lt": (1026, 608, 1154, 712),
    "xbtn_rt": (1176, 608, 1304, 712),
}


def read_gob_entry_bytes(gob_path: Path, entry_name: str) -> bytes:
    needle = entry_name.lower()
    with gob_path.open("rb") as f:
        magic, _version, directory_offset = struct.unpack("<LLL", f.read(12))
        if magic != 0x20424F47:
            raise ValueError(f"{gob_path} is not a GOB file")
        f.seek(directory_offset)
        count = struct.unpack("<L", f.read(4))[0]
        for _ in range(count):
            raw = f.read(136)
            offset, size = struct.unpack("<LL", raw[:8])
            name = raw[8:].split(b"\0", 1)[0].decode("latin1").lower()
            if name == needle:
                f.seek(offset)
                return f.read(size)
    raise FileNotFoundError(f"{entry_name} not found in {gob_path}")


def read_uicolormap(res2: Path) -> list[tuple[int, int, int]]:
    raw = read_gob_entry_bytes(res2, "misc\\cmp\\uicolormap.cmp")
    pal = raw[0x40 : 0x40 + 0x300]
    if len(pal) != 0x300:
        raise ValueError("uicolormap.cmp did not contain a full 256-color palette")
    return [(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]) for i in range(256)]


def nearest_palette_index(color: tuple[int, int, int], palette: list[tuple[int, int, int]]) -> int:
    r, g, b = color
    best_idx = 1
    best_dist = 1 << 30
    for idx, (pr, pg, pb) in enumerate(palette[1:], start=1):
        dr = r - pr
        dg = g - pg
        db = b - pb
        dist = dr * dr + dg * dg + db * db
        if dist < best_dist:
            best_idx = idx
            best_dist = dist
    return best_idx


def quantize_icon(img: Image.Image, target_palette: list[tuple[int, int, int]] | None) -> tuple[bytes, bytes]:
    rgba = img.convert("RGBA")
    if target_palette is not None:
        cache: dict[tuple[int, int, int], int] = {}
        indices = bytearray()
        for r, g, b, a in rgba.getdata():
            if a < 24:
                indices.append(0)
                continue
            color = (r, g, b)
            idx = cache.get(color)
            if idx is None:
                idx = nearest_palette_index(color, target_palette)
                cache[color] = idx
            indices.append(idx)

        out_palette = bytearray(256 * 3)
        for idx, (r, g, b) in enumerate(target_palette):
            out_palette[idx * 3] = r
            out_palette[idx * 3 + 1] = g
            out_palette[idx * 3 + 2] = b
        return bytes(indices), bytes(out_palette)

    rgb = Image.new("RGB", rgba.size, (0, 0, 0))
    rgb.paste(rgba.convert("RGB"), mask=rgba.getchannel("A"))
    quant = rgb.quantize(colors=255, method=Image.Quantize.MEDIANCUT)
    pal_raw = quant.getpalette()[: 255 * 3]
    out_palette = bytearray(256 * 3)
    out_palette[3:] = bytes(pal_raw)
    indices = bytearray()
    for idx, alpha in zip(quant.tobytes(), rgba.getchannel("A").tobytes()):
        indices.append(0 if alpha < 24 else min(255, idx + 1))
    return bytes(indices), bytes(out_palette)


def write_bm(path: Path, img: Image.Image, target_palette: list[tuple[int, int, int]] | None) -> None:
    width, height = img.size
    rd_tex_format = [0, 8, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0]
    header = [
        0x20204D42,
        70,
        0,
        2,
        1,
        0,
        0,
        0,
        *rd_tex_format,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    ]
    indices, palette = quantize_icon(img, target_palette)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(struct.pack("<32I", *header))
        f.write(struct.pack("<II", width, height))
        f.write(indices)
        f.write(palette)


def write_bm32(path: Path, img: Image.Image) -> None:
    width, height = img.size
    rd_tex_format = [0, 32, 8, 8, 8, 0, 8, 16, 0, 0, 0, 0, 0, 0]
    header = [
        0x20204D42,
        70,
        0,
        0,
        1,
        0,
        0,
        0,
        *rd_tex_format,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
        0,
    ]
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(struct.pack("<32I", *header))
        f.write(struct.pack("<II", width, height))
        f.write(img.convert("RGBA").tobytes())


def prepare_button(source: Image.Image, box: tuple[int, int, int, int]) -> Image.Image:
    icon = source.crop(box).convert("RGBA")
    pix = bytearray(icon.tobytes())
    for i in range(0, len(pix), 4):
        r, g, b, a = pix[i], pix[i + 1], pix[i + 2], pix[i + 3]
        if a == 0 or max(r, g, b) <= 8:
            pix[i + 3] = 0
    icon = Image.frombytes("RGBA", icon.size, bytes(pix))
    bbox = icon.getchannel("A").getbbox()
    if bbox:
        icon = icon.crop(bbox)
    icon.thumbnail((72, 44), Image.Resampling.LANCZOS)
    out = Image.new("RGBA", (80, 56), (0, 0, 0, 0))
    out.alpha_composite(icon, ((out.width - icon.width) // 2, (out.height - icon.height) // 2))
    return out


def output_roots(repo: Path, game_resource: Path | None) -> list[Path]:
    roots = [
        repo / "resource",
        repo / "packaging" / "dsi" / "sdcard" / "jk1" / "resource",
        repo / "build" / "xbox" / "release" / "Resource",
    ]
    if game_resource is not None:
        roots.append(game_resource)
    return roots


def generate(repo: Path, game_resource: Path | None, res2: Path | None) -> Path:
    src_path = repo / "xbox_duke_buttons.png"
    source = Image.open(src_path).convert("RGBA")
    target_palette = read_uicolormap(res2) if res2 and res2.exists() else None
    preview_dir = repo / "build" / "generated" / "xbox_duke_buttons"
    preview_dir.mkdir(parents=True, exist_ok=True)
    sheet = Image.new("RGBA", (len(BUTTONS) * 96, 96), (32, 32, 32, 255))

    for idx, (stem, box) in enumerate(BUTTONS.items()):
        icon = prepare_button(source, box)
        icon.save(preview_dir / f"{stem}.png")
        sheet.alpha_composite(icon, (idx * 96 + 8, 20))
        for root in output_roots(repo, game_resource):
            write_bm(root / "ui" / "bm" / f"{stem}.bm", icon, target_palette)
            write_bm32(root / "ui" / "bm" / f"{stem.replace('xbtn_', 'xbtn_tc_')}.bm", icon)

    sheet.save(preview_dir / "xbox_duke_buttons_sheet.png")
    return preview_dir / "xbox_duke_buttons_sheet.png"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--res2", type=Path, default=DEFAULT_RES2)
    parser.add_argument("--game-resource", type=Path, default=Path(r"C:\Games\Emulators\CXBX\openJKDF2x\Resource"))
    parser.add_argument("--no-game-copy", action="store_true")
    parser.add_argument("--own-palette", action="store_true")
    args = parser.parse_args()
    sheet = generate(args.repo.resolve(), None if args.no_game_copy else args.game_resource, None if args.own_palette else args.res2)
    print(sheet)


if __name__ == "__main__":
    main()
