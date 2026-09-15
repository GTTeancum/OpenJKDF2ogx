from analyze_frame_pacing import analyze

def record(span, frames, worst, stalls):
    return (f'PerfHW: spanMs={span} frames={frames} fps=999.99 '
            f'maxFrameMs={worst} h50=2 h100=2 h250=1 h500={stalls} '
            'texUp=4 uiUp=0 memPage=0')

text = '\n'.join([record(10000, 600, 20, 0), record(20000, 600, 2200, 1)])
result = analyze(text)
assert result['weighted_fps'] == 40  # Time-weighted, not the mean of 60 and 30.
assert result['worst_frame_ms'] == 2200
assert result['frames_over_500ms'] == 1
assert len(result['windows']) == 2
assert not analyze(record(10000, 600, 20, 0).removesuffix('memPage=0'))['windows']
assert not analyze(record(10000, 600, 20, 0)[1:])['windows']
assert analyze(record(10000, 0, 2200, 1))['weighted_fps'] == 0
assert analyze('')['weighted_fps'] is None
print('PASS: retained stalls, time weighting, zero frames, and truncated-record rejection')
