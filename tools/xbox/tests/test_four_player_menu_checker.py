"""Reject per-slot failures that the old player-one-only checker could miss."""
from check_four_player_menu import check

lines = []
for cycle in range(1, 4):
    ammo = 60-cycle*6
    lines.append(f'Smoke: escape menu auto-open fired cycle={cycle}')
    for slot in range(4):
        lines.append(f'Smoke: menu slot={slot} entryAmmo={ammo} minAmmo={ammo} maxAmmo={ammo} exitAmmo={ammo}')
    lines.append('Smoke: escape menu return fired')
    for slot in range(4):
        lines.append(f'Smoke: fire probe down=0 curMs=3000 weapon=2 ammo={ammo-6} rawDown=0 secondary={slot%2} port={slot}')
good = '\n'.join(lines)
assert len(check(good)) == 3
bad = [good.replace('minAmmo=54', 'minAmmo=53', 1),
       good.replace('slot=2', 'slot=1', 1),
       good.replace('ammo=36 rawDown=0 secondary=1 port=3', 'ammo=42 rawDown=0 secondary=1 port=3'),
       good.replace('cycle=3', 'cycle=4')]
for case in bad:
    try:
        check(case)
    except AssertionError:
        pass
    else:
        raise AssertionError('accepted incomplete or failing per-slot evidence')
print('PASS: valid cycles accepted; transient firing, missing slots, no resumption and incomplete cycles rejected')
