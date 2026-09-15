"""Require every local player's ammo to hold in each menu and resume afterward."""
from pathlib import Path
import argparse
import re


def check(text, cycles=3):
    opens = list(re.finditer(r'Smoke: escape menu auto-open fired cycle=(\d+)', text))
    assert [int(m[1]) for m in opens] == list(range(1, cycles + 1)), 'incomplete cycles'
    evidence = []
    for i, opened in enumerate(opens):
        end = opens[i+1].start() if i+1 < len(opens) else len(text)
        segment = text[opened.end():end]
        returned = re.search(r'Smoke: escape menu return fired[^\n]*', segment)
        assert returned, f'cycle {i+1}: missing return'
        rows = re.findall(r'Smoke: menu slot=(\d+) entryAmmo=([\d.-]+) minAmmo=([\d.-]+) maxAmmo=([\d.-]+) exitAmmo=([\d.-]+)', segment[:returned.start()])
        assert [int(row[0]) for row in rows] == list(range(4)), f'cycle {i+1}: missing slots'
        for row in rows:
            slot = int(row[0]); amounts = list(map(float, row[1:]))
            assert amounts[0] > 0 and len(set(amounts)) == 1, f'cycle {i+1} slot {slot}: ammo changed {amounts}'
            shots = [float(m[1]) for m in re.finditer(
                rf'Smoke: fire probe down=[01] curMs=\d+ weapon=\d+ ammo=([\d.-]+)[^\n]*\bport={slot}\b',
                segment[returned.end():])]
            # A held burst can extend to the next menu without a release log.
            if i+1 < len(opens):
                next_row = re.search(rf'Smoke: menu slot={slot} entryAmmo=([\d.-]+)', text[end:])
                if next_row: shots.append(float(next_row[1]))
            assert any(0 <= shot < amounts[0] for shot in shots), f'cycle {i+1} slot {slot}: no resumed ammo consumption'
        evidence.append(f'cycle {i+1}: all four held and resumed')
    return evidence


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('run_dir', type=Path)
    parser.add_argument('--cycles', type=int, default=3)
    args = parser.parse_args()
    errors = []
    for path in sorted(args.run_dir.glob('ram_poll_*.txt'), reverse=True):
        try:
            evidence = check(path.read_text(errors='replace'), args.cycles)
        except AssertionError as error:
            errors.append(f'{path.name}: {error}')
        else:
            print(f'PASS: {path.name}; ' + '; '.join(evidence))
            raise SystemExit(0)
    raise SystemExit('FAIL: ' + '; '.join(errors))
