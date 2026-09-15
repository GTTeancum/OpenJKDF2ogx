"""Report detailed scope windows; long-gap windows are dropped by the producer.

Use analyze_frame_pacing.py alongside this report to retain stall evidence.
"""
import argparse
import json
import re
from pathlib import Path


def analyze(text):
    windows = []
    arena = None
    for line in text.splitlines():
        ready = re.match(r"Smoke: AlwaysSoak gameplay-ready cycle=(\d+) phase=(\d+)/(\d+) seconds=\d+ players=\d+ episode='([^']*)' level='([^']*)'", line)
        if ready:
            arena = {"cycle": int(ready[1]), "phase": int(ready[2]),
                     "episode": ready[4], "level": ready[5]}
        elif line.startswith(('Smoke: AlwaysSoak phase-complete ', 'Smoke: AlwaysSoak load ')):
            arena = None
        if not line.startswith("PerfSplit:"):
            continue
        values = {key: int(value) for key, value in re.findall(r"(\w+)=(\d+)", line)}
        if not all(values.get(key, 0) > 0 for key in ("frames", "tickSpanMs", "counterSpanUs")):
            continue
        # A RAM-ring snapshot may end mid-line. Require the last field too.
        if "presentUs" not in values:
            continue
        count = values["frames"]
        windows.append({
            "arena": arena,
            "frames": count,
            "players": values.get("players"),
            "tick_span_ms": values["tickSpanMs"],
            "counter_span_us": values["counterSpanUs"],
            "tick_fps": round(count * 1000 / values["tickSpanMs"], 3),
            "counter_fps": round(count * 1000000 / values["counterSpanUs"], 3),
            "counter_to_tick_ratio": round(values["counterSpanUs"] / (values["tickSpanMs"] * 1000), 4),
            "counter_ms_per_frame": {
                key[:-2]: round(value / count / 1000, 4)
                for key, value in values.items()
                if key.endswith("Us") and key != "counterSpanUs"
            },
        })
    return windows


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("log", type=Path, help="One completed RAM snapshot, not concatenated polls")
    args = parser.parse_args()
    windows = analyze(args.log.read_text(errors="replace"))
    print(json.dumps({"source": str(args.log.resolve()),
                      "note": "Long-gap windows are dropped by the producer: these FPS values are not an inclusive pacing baseline. Also run analyze_frame_pacing.py. Counter and tick clocks are distinct; nested scope durations must not be summed. No hardware equivalence is implied.",
                      "windows": windows}, indent=2))
    if not windows:
        raise SystemExit("No complete dual-clock profiler windows found")
