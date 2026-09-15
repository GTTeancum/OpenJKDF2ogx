"""Capture a timed sequence through XEMU's native screenshot facility only."""
import argparse
import json
import time
from pathlib import Path
from xemu_native_screenshot import trigger_native_screenshot

p = argparse.ArgumentParser()
p.add_argument('--pid', type=int, required=True)
p.add_argument('--xemu-exe', required=True)
p.add_argument('--screenshot-dir', required=True)
p.add_argument('--count', type=int, default=12)
p.add_argument('--interval', type=float, default=1.1)
a = p.parse_args()
if not 1 <= a.count <= 120 or a.interval < 0.05:
    p.error('count must be 1..120; interval must be >= 0.05')
records = []
try:
    for i in range(a.count):
        start = time.time()
        ok, message, path = trigger_native_screenshot(a.pid, a.xemu_exe, a.screenshot_dir, 5)
        if ok and a.interval < 1.1:
            # Archive native PNG bytes unchanged before the next request can
            # reuse XEMU's filename (which has only whole-second precision).
            native = Path(path)
            deadline = time.time() + 5
            while not native.read_bytes().endswith(b'IEND\xaeB`\x82'):
                if time.time() > deadline:
                    raise RuntimeError('native PNG did not finish writing')
                time.sleep(0.02)
            archived = native.with_name(f'frame-{i:03d}-{native.name}')
            native.rename(archived)
            path = str(archived)
        record = dict(index=i, requestedAt=start, finishedAt=time.time(), ok=ok, message=message, path=path)
        records.append(record)
        print(json.dumps(record), flush=True)
        if not ok:
            raise RuntimeError(message)
        if i + 1 < a.count:
            time.sleep(a.interval)
finally:
    Path(a.screenshot_dir, 'sequence.json').write_text(json.dumps(records, indent=2))
