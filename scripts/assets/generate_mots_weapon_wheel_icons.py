#!/usr/bin/env python3
"""Render the six MotS-only Xbox weapon-wheel icons from retail 3DO models.

The generated BM files are source-controlled UI resources; the MotS GOO is
read-only input and is never copied into the repository by this script.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from generate_xbox_weapon_wheel_icons import (
    emit_png,
    parse_3do,
    parse_mat,
    read_gob_entry,
    read_uicolormap,
    render_icon,
    write_bm,
)


DEFAULT_MOTS_RESOURCE = Path(r"C:\Games\Emulators\CXBX\openJKDF2x\Resource\JKMRES.GOO")

# Use the actual MotS meshes. The world/pickup model is preferred over a POV
# model so the icon does not include Kyle's or Mara's arm.
MOTS_WEAPONS = (
    ("xw_mots_blastech", "3do\\blsp.3do"),
    ("xw_mots_scope", "3do\\sscg.3do"),
    ("xw_mots_flash", "3do\\flsp_1.3do"),
    ("xw_mots_seeker", "3do\\skrp.3do"),
    ("xw_mots_manseq", "3do\\seqp_1.3do"),
    ("xw_mots_carbo", "3do\\cbnp.3do"),
)


def generate(resource: Path, output: Path, preview: Path) -> None:
    palette = read_uicolormap(resource)
    materials: dict[str, dict] = {}
    for stem, model_name in MOTS_WEAPONS:
        names, vertices, tex_vertices, faces = parse_3do(read_gob_entry(resource, model_name))
        model_materials = []
        for name in names:
            if name not in materials:
                materials[name] = parse_mat(resource, name, palette)
            model_materials.append(materials[name])
        icon = render_icon(model_materials, vertices, tex_vertices, faces, skip_hand=False)
        if stem == "xw_mots_scope":
            icon = icon.rotate(90)
        write_bm(output / "ui" / "bm" / f"{stem}.bm", icon)
        emit_png(preview / f"{stem}.png", icon)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument("--mots-resource", type=Path, default=DEFAULT_MOTS_RESOURCE)
    parser.add_argument("--output", type=Path, default=Path("resource"))
    parser.add_argument("--preview", type=Path, default=Path("build/generated/mots_weapon_wheel_icons"))
    args = parser.parse_args()
    generate(args.mots_resource, args.output, args.preview)
    print(f"Generated {len(MOTS_WEAPONS)} MotS weapon-wheel icons in {args.output / 'ui' / 'bm'}")


if __name__ == "__main__":
    main()
