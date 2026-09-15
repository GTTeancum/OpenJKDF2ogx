"""Report retained frame windows, including stalls absent from PerfSplit."""
import argparse
import json
import re
from pathlib import Path


def analyze(text):
    windows = []
    for line in text.splitlines():
        if not line.startswith('PerfHW: '):
            continue
        values = {k: int(v) for k, v in re.findall(r'(\w+)=(\d+)(?=\s|$)', line)}
        # memPage is the final field: reject truncated ring-buffer records.
        required = ('spanMs', 'frames', 'maxFrameMs', 'h50', 'h100',
                    'h250', 'h500', 'texUp', 'uiUp', 'memPage')
        if not all(k in values for k in required) or values['spanMs'] <= 0:
            continue
        values['fps'] = values['frames'] * 1000 / values['spanMs']
        windows.append(values)
    span = sum(w['spanMs'] for w in windows)
    return {
        'windows': windows,
        'covered_ms': span,
        'weighted_fps': sum(w['frames'] for w in windows) * 1000 / span if span else None,
        'worst_frame_ms': max((w['maxFrameMs'] for w in windows), default=None),
        'frames_over_500ms': sum(w['h500'] for w in windows),
    }


if __name__ == '__main__':
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument('log', type=Path, help='One complete snapshot; never concatenate overlapping polls')
    args = parser.parse_args()
    report = analyze(args.log.read_text(errors='replace'))
    report['note'] = ('Engine-clock windows include stalls and may cross startup, menus or map loads. '
                      'Inspect surrounding markers before assigning a cause or arena. '
                      'These are emulator results, not original-hardware FPS.')
    print(json.dumps(report, indent=2))
