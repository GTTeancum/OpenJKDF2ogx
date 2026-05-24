#!/usr/bin/env python3
"""Generate Xbox weapon wheel BM icons from JK weapon pickup/profile 3DOs.

The retail data has force/inventory icons but no equivalent weapon icon BM
set.  These assets are deliberately generated from the original 3DOs so the
wheel displays recognizable weapon profiles instead of hand-authored
placeholder shapes.
"""

from __future__ import annotations

import argparse
import math
import re
import struct
from pathlib import Path
from typing import Iterable

from PIL import Image, ImageDraw, ImageFilter


DEFAULT_RES2 = Path(r"C:\Games\Emulators\CXBX\openJKDF2x\Resource\Res2.gob")
DEFAULT_GAME_RESOURCE = Path(r"C:\Games\Emulators\CXBX\openJKDF2x\Resource")


WEAPONS = [
    ("xw_fists", "3do\\fistg.3do", False),
    ("xw_bryar", "3do\\bryp.3do", False),
    ("xw_strifle", "3do\\strp.3do", False),
    ("xw_thermal", "3do\\detp.3do", False),
    ("xw_tusken", "3do\\bowp.3do", False),
    ("xw_repeater", "3do\\rptp.3do", False),
    ("xw_rail", "3do\\rldp.3do", False),
    ("xw_seqchg", "3do\\seqp.3do", False),
    ("xw_conc", "3do\\conp.3do", False),
    ("xw_saber", "3do\\sabp.3do", False),
]


VERT_RE = re.compile(
    r"^\s*(\d+):\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)"
)
FACE_HEAD_RE = re.compile(
    r"^\s*\d+:\s+(-?\d+)\s+0x[0-9a-fA-F]+\s+\d+\s+\d+\s+\d+\s+[-+0-9.eE]+\s+(\d+)\s+(.*)$"
)
TEX_VERT_RE = re.compile(r"^\s*(\d+):\s+([-+0-9.eE]+)\s+([-+0-9.eE]+)")
MATERIAL_RE = re.compile(r"^\s*(\d+):\s+([^\s#]+)")


def read_gob_entry(gob_path: Path, entry_name: str) -> str:
    return read_gob_entry_bytes(gob_path, entry_name).decode("latin1", errors="ignore")


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


def parse_3do(text: str) -> tuple[list[str], list[tuple[float, float, float]], list[tuple[float, float]], list[dict]]:
    lines = text.splitlines()
    materials: list[str] = []
    vertices: list[tuple[float, float, float]] = []
    tex_vertices: list[tuple[float, float]] = []
    faces: list[dict] = []

    i = 0
    while i < len(lines):
        line = lines[i].strip()
        if line.startswith("MATERIALS "):
            expected = int(line.split()[1])
            i += 1
            while i < len(lines) and len(materials) < expected:
                match = MATERIAL_RE.match(lines[i])
                if match:
                    materials.append(match.group(2).lower())
                i += 1
            continue
        if line.startswith("VERTICES "):
            expected = int(line.split()[1])
            i += 1
            while i < len(lines) and len(vertices) < expected:
                match = VERT_RE.match(lines[i])
                if match:
                    vertices.append(
                        (
                            float(match.group(2)),
                            float(match.group(3)),
                            float(match.group(4)),
                        )
                    )
                i += 1
            continue
        if line.startswith("TEXTURE VERTICES "):
            expected = int(line.split()[2])
            i += 1
            while i < len(lines) and len(tex_vertices) < expected:
                match = TEX_VERT_RE.match(lines[i])
                if match:
                    tex_vertices.append((float(match.group(2)), float(match.group(3))))
                i += 1
            continue
        if line.startswith("FACES "):
            expected = int(line.split()[1])
            i += 1
            while i < len(lines) and len(faces) < expected:
                match = FACE_HEAD_RE.match(lines[i])
                if match:
                    material = int(match.group(1))
                    verts = int(match.group(2))
                    tail = match.group(3)
                    pairs = re.findall(r"(-?\d+)\s*,\s*(-?\d+)", tail)
                    indices = [int(v) for v, _t in pairs[:verts]]
                    tex_indices = [int(t) for _v, t in pairs[:verts]]
                    if len(indices) >= 3:
                        faces.append({"material": material, "indices": indices, "tex_indices": tex_indices})
                i += 1
            continue
        i += 1

    if not vertices or not faces:
        raise ValueError("3DO parse produced no geometry")
    return materials, vertices, tex_vertices, faces


def read_uicolormap(res2: Path) -> list[tuple[int, int, int]]:
    raw = read_gob_entry_bytes(res2, "misc\\cmp\\uicolormap.cmp")
    pal = raw[0x40 : 0x40 + 0x300]
    if len(pal) != 0x300:
        raise ValueError("uicolormap.cmp did not contain a full 256-color palette")
    return [(pal[i * 3], pal[i * 3 + 1], pal[i * 3 + 2]) for i in range(256)]


def parse_mat(res2: Path, material_name: str, palette: list[tuple[int, int, int]]) -> dict:
    raw = read_gob_entry_bytes(res2, "3do\\mat\\" + material_name.lower())
    if len(raw) < 80 or raw[:4] != b"MAT ":
        raise ValueError(f"bad MAT {material_name}")
    pos = 0
    _magic, revision, mat_type, num_texinfo, num_textures = struct.unpack_from("<4sLLLL", raw, pos)
    pos += 20
    tex_format = struct.unpack_from("<14L", raw, pos)
    pos += 56
    bpp = tex_format[1]
    texinfos = []
    texture_refs = []
    for _ in range(num_texinfo):
        info = struct.unpack_from("<6L", raw, pos)
        pos += 24
        texture_ref = 0
        if info[0] & 8:
            ext = struct.unpack_from("<4L", raw, pos)
            pos += 16
            texture_ref = ext[3]
        texinfos.append(info)
        texture_refs.append(texture_ref)

    textures = []
    for _ in range(num_textures):
        width, height, alpha_en, unk_0c, transparent, mipmaps = struct.unpack_from("<6L", raw, pos)
        pos += 24
        levels = []
        w, h = width, height
        for _m in range(mipmaps):
            byte_count = w * h * (2 if bpp == 16 or tex_format[0] else 1)
            levels.append((w, h, raw[pos : pos + byte_count]))
            pos += byte_count
            w = max(1, w // 2)
            h = max(1, h // 2)
        textures.append({"alpha": alpha_en, "transparent": transparent, "levels": levels})

    rgb = (150, 150, 150)
    image = None
    if textures and textures[0]["levels"]:
        width, height, data = textures[0]["levels"][0]
        pixels = []
        if bpp == 16 or tex_format[0]:
            for i in range(0, len(data), 2):
                px = data[i] | (data[i + 1] << 8)
                if tex_format[3] == 6:
                    r = ((px >> 11) & 0x1F) * 255 // 31
                    g = ((px >> 5) & 0x3F) * 255 // 63
                    b = (px & 0x1F) * 255 // 31
                else:
                    r = ((px >> 10) & 0x1F) * 255 // 31
                    g = ((px >> 5) & 0x1F) * 255 // 31
                    b = (px & 0x1F) * 255 // 31
                pixels.append((r, g, b))
        else:
            pixels = [palette[p] for p in data]
        image_obj = Image.new("RGB", (width, height))
        image_obj.putdata(pixels)
        image = image_obj
        visible = [p for p in pixels if p != (0, 0, 0)]
        if visible:
            rgb = tuple(sum(p[c] for p in visible) // len(visible) for c in range(3))
    elif texinfos:
        c = texinfos[0][1] & 0xFF
        rgb = palette[c]
    return {"rgb": rgb, "image": image, "texinfos": texinfos, "texture_refs": texture_refs}


def face_normal(points: list[tuple[float, float, float]]) -> tuple[float, float, float]:
    ax, ay, az = points[0]
    bx, by, bz = points[1]
    cx, cy, cz = points[2]
    ux, uy, uz = bx - ax, by - ay, bz - az
    vx, vy, vz = cx - ax, cy - ay, cz - az
    nx = uy * vz - uz * vy
    ny = uz * vx - ux * vz
    nz = ux * vy - uy * vx
    length = math.sqrt(nx * nx + ny * ny + nz * nz) or 1.0
    return nx / length, ny / length, nz / length


def project_vertex(v: tuple[float, float, float]) -> tuple[float, float, float]:
    x, y, z = v
    return y, -z, x


def material_luma(material: int, normal: tuple[float, float, float]) -> int:
    nx, ny, nz = normal
    light = max(0.0, nx * 0.25 + ny * -0.55 + nz * 0.65)
    base = 148 + ((material * 19) % 42)
    return max(70, min(238, int(base + light * 52)))


def render_icon(
    material_colors: list[dict],
    vertices: list[tuple[float, float, float]],
    tex_vertices: list[tuple[float, float]],
    faces: list[dict],
    skip_hand: bool,
    size: int = 64,
) -> Image.Image:
    model_faces = [f for f in faces if not skip_hand or f["material"] > 1]
    if len(model_faces) < 3:
        model_faces = faces

    projected = [project_vertex(v) for v in vertices]
    used_indices = sorted({idx for f in model_faces for idx in f["indices"]})
    min_x = min(projected[i][0] for i in used_indices)
    max_x = max(projected[i][0] for i in used_indices)
    min_y = min(projected[i][1] for i in used_indices)
    max_y = max(projected[i][1] for i in used_indices)
    span_x = max(max_x - min_x, 0.0001)
    span_y = max(max_y - min_y, 0.0001)

    render_size = 256
    scale = (render_size * 0.82) / max(span_x, span_y)
    ox = (render_size - span_x * scale) * 0.5 - min_x * scale
    oy = (render_size - span_y * scale) * 0.5 - min_y * scale

    fill = Image.new("RGBA", (render_size, render_size), (0, 0, 0, 0))

    def to_screen(idx: int) -> tuple[float, float]:
        px, py, _depth = projected[idx]
        return px * scale + ox, py * scale + oy

    def depth(face: dict) -> float:
        return sum(projected[i][2] for i in face["indices"]) / len(face["indices"])

    def material_image(face: dict) -> Image.Image | None:
        material = face["material"]
        mat = material_colors[material] if 0 <= material < len(material_colors) else None
        if mat and mat.get("image"):
            return mat["image"]
        return None

    def sample_face_color(face: dict, normal: tuple[float, float, float]) -> tuple[int, int, int, int]:
        material = face["material"]
        mat = material_colors[material] if 0 <= material < len(material_colors) else None
        rgb = mat["rgb"] if mat else (160, 160, 160)
        nx, ny, nz = normal
        light = max(0.55, min(1.08, 0.82 + nx * 0.10 + ny * -0.16 + nz * 0.14))
        return (
            max(0, min(255, int(rgb[0] * light))),
            max(0, min(255, int(rgb[1] * light))),
            max(0, min(255, int(rgb[2] * light))),
            255,
        )

    def paste_textured_triangle(
        dst: Image.Image,
        texture: Image.Image,
        screen_tri: list[tuple[float, float]],
        uv_tri: list[tuple[float, float]],
    ) -> None:
        min_x = max(0, int(math.floor(min(p[0] for p in screen_tri))) - 1)
        min_y = max(0, int(math.floor(min(p[1] for p in screen_tri))) - 1)
        max_x = min(dst.width, int(math.ceil(max(p[0] for p in screen_tri))) + 1)
        max_y = min(dst.height, int(math.ceil(max(p[1] for p in screen_tri))) + 1)
        if max_x <= min_x or max_y <= min_y:
            return

        (x0, y0), (x1, y1), (x2, y2) = screen_tri
        (u0, v0), (u1, v1), (u2, v2) = uv_tri
        det = x0 * (y1 - y2) + x1 * (y2 - y0) + x2 * (y0 - y1)
        if abs(det) < 0.00001:
            return
        a_u = (u0 * (y1 - y2) + u1 * (y2 - y0) + u2 * (y0 - y1)) / det
        b_u = (u0 * (x2 - x1) + u1 * (x0 - x2) + u2 * (x1 - x0)) / det
        c_u = (u0 * (x1 * y2 - x2 * y1) + u1 * (x2 * y0 - x0 * y2) + u2 * (x0 * y1 - x1 * y0)) / det
        a_v = (v0 * (y1 - y2) + v1 * (y2 - y0) + v2 * (y0 - y1)) / det
        b_v = (v0 * (x2 - x1) + v1 * (x0 - x2) + v2 * (x1 - x0)) / det
        c_v = (v0 * (x1 * y2 - x2 * y1) + v1 * (x2 * y0 - x0 * y2) + v2 * (x0 * y1 - x1 * y0)) / det

        patch_w = max_x - min_x
        patch_h = max_y - min_y
        coeffs = (
            a_u,
            b_u,
            c_u + a_u * min_x + b_u * min_y,
            a_v,
            b_v,
            c_v + a_v * min_x + b_v * min_y,
        )
        patch = texture.transform(
            (patch_w, patch_h),
            Image.Transform.AFFINE,
            coeffs,
            resample=Image.Resampling.BILINEAR,
        ).convert("RGBA")
        mask = Image.new("L", (patch_w, patch_h), 0)
        shifted = [(x - min_x, y - min_y) for x, y in screen_tri]
        ImageDraw.Draw(mask).polygon(shifted, fill=255)
        patch.putalpha(mask)
        dst.alpha_composite(patch, (min_x, min_y))

    for face in sorted(model_faces, key=depth):
        pts3 = [vertices[i] for i in face["indices"]]
        normal = face_normal(pts3)
        pts2 = [to_screen(i) for i in face["indices"]]
        tex = material_image(face)
        tex_pts = []
        if tex and tex_vertices:
            for t in face.get("tex_indices", []):
                if 0 <= t < len(tex_vertices):
                    u, v = tex_vertices[t]
                    tex_pts.append((u % tex.width, abs(v) % tex.height))
        if tex and len(tex_pts) == len(pts2):
            for i in range(1, len(pts2) - 1):
                paste_textured_triangle(fill, tex, [pts2[0], pts2[i], pts2[i + 1]], [tex_pts[0], tex_pts[i], tex_pts[i + 1]])
        else:
            color = sample_face_color(face, normal)
            ImageDraw.Draw(fill).polygon(pts2, fill=color)

        shade = max(0.70, min(1.15, 0.94 + normal[0] * 0.05 + normal[1] * -0.10 + normal[2] * 0.08))
        if shade < 0.98:
            mask = Image.new("L", fill.size, 0)
            ImageDraw.Draw(mask).polygon(pts2, fill=int((1.0 - shade) * 130))
            fill.alpha_composite(Image.composite(Image.new("RGBA", fill.size, (0, 0, 0, 255)), Image.new("RGBA", fill.size, (0, 0, 0, 0)), mask))

    alpha = fill.getchannel("A")
    bbox = alpha.getbbox()
    if bbox:
        pad = 12
        bbox = (
            max(0, bbox[0] - pad),
            max(0, bbox[1] - pad),
            min(fill.width, bbox[2] + pad),
            min(fill.height, bbox[3] + pad),
        )
        fill = fill.crop(bbox)

    alpha = fill.getchannel("A")
    shadow = alpha.filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(1.2))
    composited = Image.new("RGBA", fill.size, (0, 0, 0, 0))
    shadow_rgba = Image.new("RGBA", fill.size, (0, 0, 0, 90))
    composited.alpha_composite(Image.composite(shadow_rgba, composited, shadow))
    composited.alpha_composite(fill)

    icon = Image.new("RGBA", (size, size), (0, 0, 0, 0))
    scale_down = min((size - 6) / composited.width, (size - 6) / composited.height)
    out_w = max(1, int(composited.width * scale_down))
    out_h = max(1, int(composited.height * scale_down))
    resized = composited.resize((out_w, out_h), Image.Resampling.LANCZOS)
    icon.alpha_composite(resized, ((size - out_w) // 2, (size - out_h) // 2))
    return icon


def render_fists_icon(size: int = 32) -> Image.Image:
    aa = 4
    img = Image.new("RGBA", (size * aa, size * aa), (0, 0, 0, 0))
    draw = ImageDraw.Draw(img)

    def ellipse(box, fill, outline=(234, 207, 176, 230)):
        box = tuple(int(v * aa) for v in box)
        draw.ellipse(box, fill=fill, outline=outline, width=max(1, aa))

    def rect(box, fill, outline=(234, 207, 176, 230)):
        box = tuple(int(v * aa) for v in box)
        draw.rounded_rectangle(box, radius=2 * aa, fill=fill, outline=outline, width=max(1, aa))

    rect((6, 15, 14, 24), (151, 102, 72, 255))
    rect((18, 15, 26, 24), (151, 102, 72, 255))
    for x in (5, 9, 18, 22):
        ellipse((x, 8, x + 6, 17), (190, 137, 96, 255))
    ellipse((10, 11, 17, 20), (174, 119, 83, 255))
    ellipse((23, 11, 30, 20), (174, 119, 83, 255))
    shadow = img.getchannel("A").filter(ImageFilter.MaxFilter(3)).filter(ImageFilter.GaussianBlur(0.6 * aa))
    composited = Image.new("RGBA", img.size, (0, 0, 0, 0))
    composited.alpha_composite(Image.composite(Image.new("RGBA", img.size, (0, 0, 0, 72)), composited, shadow))
    composited.alpha_composite(img)
    return composited.resize((size, size), Image.Resampling.LANCZOS)


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
        0x20204D42,  # "BM  "
        70,
        0,
        2,  # embedded palette follows image data
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
    if len(header) != 32:
        raise AssertionError("BM header must be 128 bytes")
    indices, palette = quantize_icon(img)
    path.parent.mkdir(parents=True, exist_ok=True)
    with path.open("wb") as f:
        f.write(struct.pack("<32I", *header))
        f.write(struct.pack("<II", width, height))
        f.write(indices)
        f.write(palette)


def emit_png(path: Path, img: Image.Image) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    img.convert("RGBA").save(path)


def output_roots(repo: Path, game_resource: Path | None) -> list[Path]:
    roots = [
        repo / "resource",
        repo / "packaging" / "dsi" / "sdcard" / "jk1" / "resource",
        repo / "build" / "xbox" / "release" / "Resource",
    ]
    if game_resource is not None:
        roots.append(game_resource)
    return roots


def generate(res2: Path, roots: Iterable[Path], preview_dir: Path) -> None:
    palette = read_uicolormap(res2)
    material_cache: dict[str, dict] = {}
    previews = []
    for stem, entry, skip_hand in WEAPONS:
        if stem == "xw_fists":
            icon = render_fists_icon()
        else:
            material_names, vertices, tex_vertices, faces = parse_3do(read_gob_entry(res2, entry))
            material_colors = []
            for name in material_names:
                if name not in material_cache:
                    material_cache[name] = parse_mat(res2, name, palette)
                material_colors.append(material_cache[name])
            icon = render_icon(material_colors, vertices, tex_vertices, faces, skip_hand=skip_hand)
        for root in roots:
            write_bm(root / "ui" / "bm" / f"{stem}.bm", icon)
        png_path = preview_dir / f"{stem}.png"
        emit_png(png_path, icon)
        previews.append((stem, icon))

    sheet = Image.new("RGBA", (len(previews) * 48 + 8, 56), (28, 32, 38, 255))
    for i, (_stem, icon) in enumerate(previews):
        rgba = Image.open(preview_dir / f"{_stem}.png").convert("RGBA")
        sheet.alpha_composite(rgba.resize((32, 32), Image.Resampling.NEAREST), (i * 48 + 16, 8))
    sheet.save(preview_dir / "xbox_weapon_wheel_icons_sheet.png")


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--repo", type=Path, default=Path.cwd())
    parser.add_argument("--res2", type=Path, default=DEFAULT_RES2)
    parser.add_argument("--game-resource", type=Path, default=DEFAULT_GAME_RESOURCE)
    parser.add_argument("--no-game-copy", action="store_true")
    args = parser.parse_args()

    repo = args.repo.resolve()
    game_resource = None if args.no_game_copy else args.game_resource
    roots = output_roots(repo, game_resource)
    preview_dir = repo / "build" / "generated" / "xbox_weapon_wheel_icons"
    generate(args.res2, roots, preview_dir)
    print(f"Generated {len(WEAPONS)} Xbox weapon wheel icons")
    for root in roots:
        print(root / "ui" / "bm")
    print(preview_dir / "xbox_weapon_wheel_icons_sheet.png")


if __name__ == "__main__":
    main()
