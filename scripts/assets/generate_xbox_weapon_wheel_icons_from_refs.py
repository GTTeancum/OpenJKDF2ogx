#!/usr/bin/env python3
"""Generate Xbox weapon wheel BM icons from supplied reference screenshots."""

from __future__ import annotations

import argparse
import collections
import struct
from io import BytesIO
from pathlib import Path
from urllib.request import Request, urlopen

from PIL import Image, ImageChops, ImageFilter


ASSETS = [
    {
        "stem": "xw_fists",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/2/27/Fists.jpg/revision/latest?cb=20110504011633",
        "crop": (0.00, 0.44, 0.45, 0.98),
        "box": (48, 38),
    },
    {
        "stem": "xw_bryar",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/7/7e/Bryar_Pistol.jpg/revision/latest?cb=20110504004929",
        "box": (56, 32),
        "black_floor": 8,
        "black_soft": 22,
    },
    {
        "stem": "xw_strifle",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/9/9b/Stormtrooper_Rifle.jpg/revision/latest?cb=20110504010853",
        "flip": True,
        "box": (60, 30),
        "black_floor": 8,
        "black_soft": 24,
    },
    {
        "stem": "xw_thermal",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/1/13/Thermal_Detonator_Belt.jpg/revision/latest?cb=20110504014858",
        "box": (46, 36),
    },
    {
        "stem": "xw_tusken",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/7/78/Crossbow.jpg/revision/latest?cb=20110505042451",
        "box": (60, 34),
    },
    {
        "stem": "xw_repeater",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/1/19/Imperial_Repeater_Rifle.jpg/revision/latest?cb=20110529175738",
        "box": (60, 32),
    },
    {
        "stem": "xw_rail",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/d/d9/Rail_Detonator.jpg/revision/latest?cb=20110520223450",
        "flip": True,
        "box": (60, 34),
    },
    {
        "stem": "xw_seqchg",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/8/87/Sequncer_Charge.jpg/revision/latest?cb=20110613235408",
        "box": (44, 38),
    },
    {
        "stem": "xw_conc",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/1/16/Concussion_Rifle.jpg/revision/latest?cb=20110522014952",
        "box": (60, 32),
    },
    {
        "stem": "xw_saber",
        "url": "https://static.wikia.nocookie.net/jkdf2/images/d/d1/Lightsaber.jpg/revision/latest?cb=20110701201813",
        "flip": True,
        "crop": (0.02, 0.68, 0.22, 0.94),
        "box": (40, 28),
    },
]


def download_image(url: str) -> Image.Image:
    req = Request(url, headers={"User-Agent": "Mozilla/5.0"})
    with urlopen(req, timeout=30) as resp:
        raw = resp.read()
    return Image.open(BytesIO(raw)).convert("RGBA")


def relative_crop(img: Image.Image, crop: tuple[float, float, float, float] | None) -> Image.Image:
    if not crop:
        return img
    w, h = img.size
    left, top, right, bottom = crop
    return img.crop((int(left * w), int(top * h), int(right * w), int(bottom * h)))


def edge_background_mask(img: Image.Image, threshold: int = 80) -> Image.Image:
    rgb = img.convert("RGB")
    pix = rgb.load()
    w, h = rgb.size
    visited = bytearray(w * h)
    q: collections.deque[tuple[int, int]] = collections.deque()

    def is_bg(x: int, y: int) -> bool:
        r, g, b = pix[x, y]
        return max(r, g, b) <= threshold and (r + g + b) <= threshold * 3

    def push(x: int, y: int) -> None:
        idx = y * w + x
        if visited[idx] or not is_bg(x, y):
            return
        visited[idx] = 1
        q.append((x, y))

    for x in range(w):
        push(x, 0)
        push(x, h - 1)
    for y in range(h):
        push(0, y)
        push(w - 1, y)

    while q:
        x, y = q.popleft()
        if x > 0:
            push(x - 1, y)
        if x + 1 < w:
            push(x + 1, y)
        if y > 0:
            push(x, y - 1)
        if y + 1 < h:
            push(x, y + 1)

    mask = Image.frombytes("L", (w, h), bytes(visited))
    mask = mask.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(0.7))
    return ImageChops.invert(mask)


def remove_black_key(img: Image.Image, floor: int = 18, soft: int = 45) -> Image.Image:
    rgba = img.convert("RGBA")
    pix = bytearray(rgba.tobytes())
    for i in range(0, len(pix), 4):
        r, g, b, a = pix[i], pix[i + 1], pix[i + 2], pix[i + 3]
        mx = max(r, g, b)
        if mx <= floor:
            pix[i + 3] = 0
        elif mx <= soft:
            pix[i + 3] = min(a, int(a * (mx - floor) / max(1, soft - floor)))
    return Image.frombytes("RGBA", rgba.size, bytes(pix))


def prepare_icon(asset: dict, size: int = 64) -> Image.Image:
    img = download_image(asset["url"])
    img = relative_crop(img, asset.get("crop"))
    if asset.get("flip"):
        img = img.transpose(Image.Transpose.FLIP_LEFT_RIGHT)

    alpha = edge_background_mask(img)
    img.putalpha(alpha)
    img = remove_black_key(img, asset.get("black_floor", 18), asset.get("black_soft", 45))
    alpha = img.getchannel("A")
    bbox = alpha.getbbox()
    if bbox:
        pad = 2
        bbox = (
            max(0, bbox[0] - pad),
            max(0, bbox[1] - pad),
            min(img.width, bbox[2] + pad),
            min(img.height, bbox[3] + pad),
        )
        img = img.crop(bbox)

    box_w, box_h = asset["box"]
    icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    scale = min(box_w / max(1, img.width), box_h / max(1, img.height))
    out_w = max(1, int(img.width * scale))
    out_h = max(1, int(img.height * scale))
    resized = img.resize((out_w, out_h), Image.Resampling.LANCZOS)
    icon.alpha_composite(resized, ((size - out_w) // 2, (size - out_h) // 2))
    return icon


def quantize_icon(img: Image.Image) -> tuple[bytes, bytes]:
    rgba = img.convert("RGBA")
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


def write_bm(path: Path, img: Image.Image) -> None:
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
    indices, palette = quantize_icon(img)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(struct.pack("<32I", *header))
        f.write(struct.pack("<II", width, height))
        f.write(indices)
        f.write(palette)


def output_roots(repo: Path, game_resource: Path | None) -> list[Path]:
    roots = [
        repo / "resource",
        repo / "packaging" / "dsi" / "sdcard" / "jk1" / "resource",
        repo / "build" / "xbox" / "release" / "Resource",
    ]
    if game_resource is not None:
        roots.append(game_resource)
    return roots


def generate(repo: Path, game_resource: Path | None) -> Path:
    preview_dir = repo / "build" / "generated" / "xbox_weapon_wheel_icons"
    preview_dir.mkdir(parents=True, exist_ok=True)
    roots = output_roots(repo, game_resource)
    icons: list[tuple[str, Image.Image]] = []
    for asset in ASSETS:
        icon = prepare_icon(asset)
        icons.append((asset["stem"], icon))
        icon.save(preview_dir / f'{asset["stem"]}.png')
        for root in roots:
            write_bm(root / "ui" / "bm" / f'{asset["stem"]}.bm', icon)

    sheet = Image.new("RGBA", (len(icons) * 72 + 8, 80), (24, 24, 24, 255))
    for i, (_stem, icon) in enumerate(icons):
        sheet.alpha_composite(icon, (i * 72 + 4, 8))
    sheet.save(preview_dir / "xbox_weapon_wheel_icons_sheet.png")
    return preview_dir / "xbox_weapon_wheel_icons_sheet.png"


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--game-resource", type=Path, default=Path(r"C:\Games\Emulators\CXBX\openJKDF2x\Resource"))
    parser.add_argument("--no-game-copy", action="store_true")
    args = parser.parse_args()
    sheet = generate(args.repo.resolve(), None if args.no_game_copy else args.game_resource)
    print(sheet)


if __name__ == "__main__":
    main()
