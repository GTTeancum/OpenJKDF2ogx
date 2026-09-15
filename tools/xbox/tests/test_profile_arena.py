"""Do not infer an arena from a missing or stale phase marker."""
from analyze_split_profile import analyze
window = 'PerfSplit: players=4 frames=600 tickSpanMs=10000 counterSpanUs=10000000 submitUs=3000000 presentUs=1000'
first = "Smoke: AlwaysSoak gameplay-ready cycle=0 phase=1/2 seconds=90 players=4 episode='JK1MP' level='m2.jkl'"
second = first.replace('phase=1/2', 'phase=2/2').replace('m2.jkl', 'm4.jkl')
result = analyze('\n'.join([window, first, window,
    "Smoke: AlwaysSoak phase-complete cycle=0 phase=1/2 elapsedMs=90000 episode='JK1MP' level='m2.jkl'",
    window, second, window]))
assert [r['arena']['level'] if r['arena'] else None for r in result] == [None, 'm2.jkl', None, 'm4.jkl']
assert all(r['counter_fps'] == 60 for r in result)
assert not analyze(window.rsplit(' presentUs=', 1)[0])
print('PASS: explicit arena attribution, transition reset, missing-marker and truncated-window handling')
