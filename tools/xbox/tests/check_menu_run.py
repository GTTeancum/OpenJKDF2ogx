"""Check complete ordered menu/firing evidence from a smoke run's RAM snapshots."""
from pathlib import Path
import argparse
import re

parser = argparse.ArgumentParser(description=__doc__)
parser.add_argument('run_dir', type=Path)
parser.add_argument('--cycles', type=int, default=3)
args = parser.parse_args()
errors = []
for path in sorted(args.run_dir.glob('ram_poll_*.txt'), reverse=True):
    text = path.read_text(errors='replace')
    opens = list(re.finditer(r'Smoke: escape menu auto-open fired cycle=(\d+)', text))
    if [int(m[1]) for m in opens] != list(range(1, args.cycles + 1)):
        errors.append(f'{path.name}: incomplete or unordered cycle markers')
        continue
    evidence = []
    for index, opened in enumerate(opens):
        end = opens[index + 1].start() if index + 1 < len(opens) else len(text)
        segment = text[opened.end():end]
        returned = re.search(r'Smoke: escape menu return fired entryAmmo=([\d.-]+) exitAmmo=([\d.-]+)', segment)
        if not returned:
            errors.append(f'{path.name}: cycle {index + 1} has no return')
            break
        before, after = map(float, returned.groups())
        if before <= 0 or before != after:
            errors.append(f'{path.name}: cycle {index + 1} ammo in menu {before} -> {after}')
            break
        shots = []
        for line in segment[returned.end():].splitlines():
            if 'port=' in line and not re.search(r'\bport=0\b', line):
                continue
            shot = re.search(r'Smoke: fire probe down=[01] curMs=\d+ weapon=\d+ ammo=([\d.-]+)', line)
            if shot: shots.append(shot[1])
        # A burst may remain held until the next menu, so its next ammo sample
        # is that menu's entry reading rather than a fire-release log.
        if index + 1 < len(opens):
            next_return = re.search(r'Smoke: escape menu return fired entryAmmo=([\d.-]+)', text[end:])
            if next_return: shots.append(next_return[1])
        consumed = [float(value) for value in shots if 0 <= float(value) < after]
        if not consumed:
            errors.append(f'{path.name}: cycle {index + 1} has no post-return ammo consumption')
            break
        evidence.append(f'cycle {index + 1}: menu {before:g}->{after:g}, resumed {consumed[0]:g}')
    else:
        print(f'PASS: {path.name}; ' + '; '.join(evidence))
        raise SystemExit(0)
print('FAIL: no snapshot proves every requested cycle.\n' + '\n'.join(errors))
raise SystemExit(1)
