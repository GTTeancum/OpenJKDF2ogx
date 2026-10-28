"""Stage a MotS SP mod-map fixture with conspicuous authored 3DO RGB light.

The retail archive is copied only into the disposable XEMU staging directory.
The source game's archives and maps are never changed.
"""

from pathlib import Path
import re
import shutil
import sys


ROOT = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(ROOT / "scripts" / "assets"))
from mots_compat_pack import read_gob_entries


def main(stage: Path) -> None:
    episode = stage / "Episode"
    retail = episode / "JKM.GOO"
    mod = episode / "MOTS_ARCH_PROBE.GOO"
    shutil.copyfile(retail, mod)

    records = read_gob_entries(retail)
    data = next(data for name, data in records if name.lower() == "jkl\\s1l1_rebelbase.jkl")
    lines = data.decode("latin1").splitlines()
    section = False
    changed = 0
    vertex = re.compile(r"^\s*(\d+):\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s+([\d.]+)\s*$")
    for index, line in enumerate(lines):
        if line.lower().strip() == "section: archlighting":
            section = True
            continue
        if not section:
            continue
        match = vertex.match(line)
        if match:
            lines[index] = f"{match.group(1)}: {match.group(2)} .90 .02 .02"
            changed += 1
    if changed < 100:
        raise RuntimeError(f"Expected MotS archlighting vertices; found {changed}")

    # Bring an existing architecture-lit crate into the opening sightline so
    # the native capture proves visible 3DO tint, not merely parsed RGB data.
    crate = re.compile(r"^245: crate4_3 crate4_3 .*? 73 55$")
    for index, line in enumerate(lines):
        if crate.match(line):
            lines[index] = "245: crate4_3 crate4_3 -5.19905 4.55426 2.933014 0 0 0 80 55"
            break
    else:
        raise RuntimeError("Expected architecture-lit crate 245 was not found")

    target = episode / "MOTS_ARCH_PROBE" / "jkl" / "s1l1_rebelbase.jkl"
    target.parent.mkdir(parents=True, exist_ok=True)
    target.write_bytes(("\r\n".join(lines) + "\r\n").encode("latin1"))
    print(f"MotS arch RGB fixture: {changed} vertices, {target}")


if __name__ == "__main__":
    main(Path(sys.argv[1]).resolve())
