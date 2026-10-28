"""Create an isolated authored-RGB JK map fixture; never change installed assets."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[3]
sys.path.insert(0, str(root / 'scripts/assets'))
from mots_compat_pack import read_gob_entries

stage = Path(sys.argv[1]).resolve()
records = read_gob_entries(stage / 'Episode/JK1.GOB')
data = next(data for name, data in records if name.lower() == 'jkl\\01narshadda.jkl')
lines = data.decode('latin1').splitlines()
remaining = 0
changed = 0
for i, line in enumerate(lines):
    if line.lower().startswith('world surfaces '):
        remaining = int(line.split()[-1])
        continue
    if not remaining or not line.strip() or line.lstrip().startswith('#'):
        continue
    parts = line.split()
    if not parts[0].endswith(':'):
        continue
    remaining -= 1
    surface = int(parts[0][:-1])
    count = int(parts[9])
    if int(parts[4]) != 4 or surface % 4 == 3:
        continue  # Keep untextured/adjoin and interleaved scalar surfaces intact.
    assert len(parts) == 10 + count * 2
    colors = [(0.9, 0.04, 0.04), (0.04, 0.9, 0.04), (0.04, 0.04, 0.9)]
    rgb = colors[surface % 4]
    lights = []
    for j in range(count):
        lights.extend([parts[10 + count + j], *(str(x) for x in rgb)])
    lines[i] = ' '.join(parts[:10 + count] + lights)
    changed += 1
assert changed > 0 and remaining == 0
target = stage / 'Episode/JK1/jkl/01narshadda.jkl'
target.parent.mkdir(parents=True, exist_ok=True)
target.write_bytes(('\r\n'.join(lines) + '\r\n').encode('latin1'))
print(f'Static RGB fixture: {changed} authored surfaces, {target}')
