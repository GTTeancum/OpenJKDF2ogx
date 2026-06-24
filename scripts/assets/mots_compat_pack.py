#!/usr/bin/env python3
"""Build Xbox-ready MotS compatibility files from an installed MotS copy.

The project reads MotS .GOO files as normal GOB containers when MotS mode is
selected explicitly. This script creates a repeatable compatibility layout with
stable episode pack names plus a JK-loaded compatibility patch for Xbox/CXBX
testing. The patch intentionally uses a .gob extension so it augments JK's
resource path instead of becoming a hidden MotS mode switch.
"""

from __future__ import annotations

import argparse
import re
import shutil
import struct
from collections import deque
from pathlib import Path


GOB_MAGIC = 0x20424F47
GOB_VERSION = 20

DEFAULT_SOURCE = Path(r"C:\Games\Star Wars Jedi Knight - Mysteries of the Sith")
DEFAULT_OUTPUT = Path("build/generated/mots_xbox_compat")

EPISODE_PACKS = (
    ("JKM.GOO", "MOTS_SP.GOO"),
    ("JKM_MP.GOO", "MOTS_DM.GOO"),
    ("JKM_KFY.GOO", "MOTS_CTF.GOO"),
    ("JKM_SABER.GOO", "MOTS_SABER.GOO"),
)

RESOURCE_FILES = (
    "JKMRES.GOO",
    "JKMsndLO.goo",
)

PATCH_ARCHIVE_NAME = "mots_xbox_patch.gob"
MOTS_RESOURCE_ARCHIVE = "JKMRES.GOO"
MOTS_SOUND_ARCHIVE = "JKMsndLO.goo"

PATCH_SEED_PREFIXES = (
    "cog\\force_",
    "cog\\weap_",
    "cog\\pow_",
    "cog\\class_",
    "misc\\per\\",
)

PATCH_SEED_EXACT = {
    "jkl\\static.jkl",
    "misc\\items.dat",
    "misc\\models.dat",
    "misc\\sabers.dat",
    "misc\\sithstrings.uni",
    "cog\\00_bloodtrail.cog",
    "cog\\00_carbtrail.cog",
    "cog\\00_desttrail.cog",
    "cog\\00_hraildet.cog",
    "cog\\00_multicam.cog",
    "cog\\00_returnsaber.cog",
    "cog\\00_smoketrail.cog",
    "cog\\00_thrownsaber.cog",
    "cog\\exp_cpel.cog",
    "cog\\exp_flash.cog",
    "cog\\exp_hrail.cog",
}

PATCH_REFERENCE_RE = re.compile(
    rb"[-A-Za-z0-9_~.\\/]+\.(?:3do|ai|bm|cmp|cog|dat|jkl|key|mat|par|per|pup|snd|spr|uni|wav)",
    re.IGNORECASE,
)


def read_gob_entries(path: Path) -> list[tuple[str, bytes]]:
    with path.open("rb") as f:
        header = f.read(12)
        if len(header) != 12:
            raise ValueError(f"{path} is too small to be a GOB/GOO")

        magic, version, table_offset = struct.unpack("<III", header)
        if magic != GOB_MAGIC or version != GOB_VERSION:
            raise ValueError(f"{path} has unexpected GOB header magic=0x{magic:08X} version={version}")

        f.seek(table_offset)
        count_data = f.read(4)
        if len(count_data) != 4:
            raise ValueError(f"{path} has a truncated entry table")

        entry_count = struct.unpack("<I", count_data)[0]
        table_entries: list[tuple[int, int, str]] = []
        for _ in range(entry_count):
            raw_entry = f.read(136)
            if len(raw_entry) != 136:
                raise ValueError(f"{path} has a truncated entry record")

            file_offset, file_size, raw_name = struct.unpack("<II128s", raw_entry)
            name = raw_name.split(b"\0", 1)[0].decode("latin-1")
            if not name:
                raise ValueError(f"{path} contains an empty entry name")
            table_entries.append((file_offset, file_size, name))

        records: list[tuple[str, bytes]] = []
        for file_offset, file_size, name in table_entries:
            f.seek(file_offset)
            data = f.read(file_size)
            if len(data) != file_size:
                raise ValueError(f"{path}:{name} is truncated")
            records.append((name, data))

    return records


def write_gob(path: Path, records: list[tuple[str, bytes]]) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)

    table_offset = 12
    data_offset = table_offset + 4 + (136 * len(records))
    entries: list[tuple[int, int, str]] = []
    next_offset = data_offset

    for name, data in records:
        encoded = name.encode("latin-1")
        if len(encoded) >= 128:
            raise ValueError(f"GOB entry name is too long: {name}")
        entries.append((next_offset, len(data), name))
        next_offset += len(data)

    with path.open("wb") as f:
        f.write(struct.pack("<III", GOB_MAGIC, GOB_VERSION, table_offset))
        f.write(struct.pack("<I", len(entries)))
        for file_offset, file_size, name in entries:
            encoded = name.encode("latin-1")
            f.write(struct.pack("<II128s", file_offset, file_size, encoded + b"\0" * (128 - len(encoded))))
        for _, data in records:
            f.write(data)


def copy_resource_files(source: Path, output: Path) -> list[Path]:
    copied: list[Path] = []
    resource_dir = source / "Resource"
    output_dir = output / "Resource"
    output_dir.mkdir(parents=True, exist_ok=True)
    expected = {name.lower() for name in RESOURCE_FILES}

    for stale in output_dir.iterdir():
        if stale.is_file() and stale.name.lower() not in expected:
            stale.unlink()

    for name in RESOURCE_FILES:
        src = resource_dir / name
        if not src.exists():
            raise FileNotFoundError(src)
        dst = output_dir / name
        shutil.copy2(src, dst)
        copied.append(dst)

    return copied


def _canonical_name(name: str) -> str:
    return name.replace("/", "\\").lower()


def _reference_candidates(token: str) -> list[str]:
    token = _canonical_name(token.strip().strip("\"'()[],;:"))
    if not token:
        return []

    suffix = token.rsplit(".", 1)[-1]
    has_dir = "\\" in token
    candidates = [token] if has_dir else []

    if suffix == "3do":
        candidates.append(f"3do\\{token}")
    elif suffix == "mat":
        candidates.extend((f"3do\\mat\\{token}", f"mat\\{token}"))
    elif suffix == "bm":
        candidates.append(f"ui\\bm\\{token}")
    elif suffix == "key":
        candidates.append(f"3do\\key\\{token}")
    elif suffix == "cog":
        candidates.append(f"cog\\{token}")
    elif suffix == "snd":
        candidates.append(f"misc\\snd\\{token}")
    elif suffix == "spr":
        candidates.append(f"misc\\spr\\{token}")
    elif suffix == "pup":
        candidates.append(f"misc\\pup\\{token}")
    elif suffix == "ai":
        candidates.append(f"misc\\ai\\{token}")
    elif suffix == "per":
        candidates.append(f"misc\\per\\{token}")
    elif suffix == "cmp":
        candidates.append(f"misc\\cmp\\{token}")
    elif suffix in ("dat", "par", "uni"):
        candidates.append(f"misc\\{token}")
    elif suffix == "jkl":
        candidates.append(f"jkl\\{token}")
    elif suffix == "wav":
        candidates.extend((f"sound\\{token}", f"voice\\{token}", f"voiceuu\\{token}"))
        if has_dir and not token.startswith("sound\\"):
            candidates.append(f"sound\\{token}")

    deduped: list[str] = []
    seen: set[str] = set()
    for candidate in candidates:
        if candidate and candidate not in seen:
            deduped.append(candidate)
            seen.add(candidate)
    return deduped


def _scan_references(data: bytes) -> set[str]:
    refs: set[str] = set()
    for match in PATCH_REFERENCE_RE.finditer(data):
        token = match.group(0).decode("latin-1", "ignore")
        refs.update(_reference_candidates(token))
    return refs


def build_patch_records(source: Path) -> list[tuple[str, bytes]]:
    resource_path = source / "Resource" / MOTS_RESOURCE_ARCHIVE
    sound_path = source / "Resource" / MOTS_SOUND_ARCHIVE
    if not resource_path.exists():
        raise FileNotFoundError(resource_path)
    if not sound_path.exists():
        raise FileNotFoundError(sound_path)

    source_records: list[tuple[str, bytes]] = []
    source_records.extend(read_gob_entries(resource_path))
    source_records.extend(read_gob_entries(sound_path))

    by_name: dict[str, tuple[str, bytes]] = {}
    for name, data in source_records:
        by_name.setdefault(_canonical_name(name), (name, data))

    selected: set[str] = set()
    pending: deque[str] = deque()

    def add(name: str) -> None:
        canonical = _canonical_name(name)
        if canonical in by_name and canonical not in selected:
            selected.add(canonical)
            pending.append(canonical)

    for name in PATCH_SEED_EXACT:
        add(name)
    for canonical in by_name:
        if canonical.startswith(PATCH_SEED_PREFIXES):
            add(canonical)

    while pending:
        current = pending.popleft()
        _, data = by_name[current]
        for ref in _scan_references(data):
            add(ref)

    records = [by_name[name] for name in sorted(selected)]
    return records


def build_patch_archive(source: Path, output: Path, generated_so_far: list[Path]) -> Path:
    output_dir = output / "mods"
    output_dir.mkdir(parents=True, exist_ok=True)

    for stale in output_dir.iterdir():
        if stale.is_file() and stale.name.lower() != PATCH_ARCHIVE_NAME.lower():
            stale.unlink()

    manifest_lines = [
        "MotS Xbox compatibility patch",
        "",
        "This is a curated JK-loaded GOB overlay built from MotS resource archives.",
        "It intentionally uses a .gob extension so JK mode can see selected MotS weapon, force, and player resources.",
        "MotS mode still loads its normal .GOO resource and episode packs explicitly.",
        "",
        "Generated alongside:",
    ]
    manifest_lines.extend(f"- {path.relative_to(output)}" for path in generated_so_far)
    manifest = "\n".join(manifest_lines) + "\n"

    dst = output_dir / PATCH_ARCHIVE_NAME
    records = build_patch_records(source)
    records.append(("misc\\mots_xbox_compat.txt", manifest.encode("latin-1")))
    write_gob(dst, records)
    return dst


def build_episode_packs(source: Path, output: Path) -> list[Path]:
    built: list[Path] = []
    episode_dir = source / "Episode"
    output_dir = output / "Episode"
    output_dir.mkdir(parents=True, exist_ok=True)

    for source_name, output_name in EPISODE_PACKS:
        src = episode_dir / source_name
        if not src.exists():
            raise FileNotFoundError(src)

        dst = output_dir / output_name
        write_gob(dst, read_gob_entries(src))
        built.append(dst)

    return built


def write_manifest(output: Path, source: Path, generated: list[Path]) -> None:
    manifest = output / "MOTS_XBOX_COMPAT_MANIFEST.txt"
    lines = [
        "MotS Xbox compatibility files",
        f"Source: {source}",
        "",
        "Generated files:",
    ]
    lines.extend(f"- {path.relative_to(output)}" for path in generated)
    lines.extend(
        [
            "",
            "Copy this folder's Episode and Resource directories into a MotS-specific Xbox/CXBX game folder.",
            "Copy mods/mots_xbox_patch.gob into the JK Xbox/CXBX game folder when JK should expose MotS compatibility assets.",
            "Select MotS mode explicitly; no CD key sidecar is generated or consulted.",
            "The patch archive is GOB format with a .gob extension because JK mode loads mods/*.gob.",
        ]
    )
    manifest.write_text("\n".join(lines) + "\n", encoding="utf-8")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, default=DEFAULT_SOURCE, help="Installed MotS folder")
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT, help="Destination compatibility folder")
    parser.add_argument("--episodes-only", action="store_true", help="Only generate Episode/*.GOO packs")
    args = parser.parse_args()

    source = args.source
    output = args.output
    if not source.exists():
        raise FileNotFoundError(source)

    generated: list[Path] = []
    generated.extend(build_episode_packs(source, output))
    if not args.episodes_only:
        generated.extend(copy_resource_files(source, output))
        generated.append(build_patch_archive(source, output, generated))
    write_manifest(output, source, generated)

    print(f"Generated MotS Xbox compatibility files in {output}")
    for path in generated:
        print(f"  {path}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
