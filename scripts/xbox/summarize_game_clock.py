"""Compare saved gameplay clock probes against the polling host's monotonic clock.

Usage: python scripts/xbox/summarize_game_clock.py <smoke-run-directory> [...]
No emulator or desktop input is sent. Use uninterrupted gameplay runs: menus,
loads and game-time resets make aggregate rates unsuitable for qualification.
"""
import argparse
import json
from pathlib import Path


def report(directory):
    samples = []
    for path in sorted(directory.glob("ram_poll_*.txt")):
        for line in path.read_text(errors="replace").splitlines():
            if not line.startswith("ClockHost: "):
                continue
            probe = json.loads(line.removeprefix("ClockHost: "))
            if probe.get("valid"):
                samples.extend((probe["first"], probe["last"]))
    if len(samples) < 2:
        return {"run": directory.name, "valid": False, "reason": "insufficient samples"}
    first, last = samples[0], samples[-1]
    low = last["host_before"] - first["host_after"]
    high = last["host_after"] - first["host_before"]
    deltas = [(b - a) & 0xffffffff for a, b in zip(first["words"], last["words"])]
    # Reject resets anywhere in the series, including one hidden by a later
    # positive aggregate delta. Counter microseconds legitimately wrap.
    reset = any(((b["words"][3] - a["words"][3]) & 0xffffffff) > 0x7fffffff
                for a, b in zip(samples, samples[1:]))
    if low <= 0 or high >= 4294 or reset:
        return {"run": directory.name, "valid": False,
                "reason": "overlap, game reset, or counter wrap ambiguity"}
    result = {"run": directory.name, "valid": True, "samples": len(samples),
            "host_seconds": [low, high], "game_seconds": deltas[3] / 1000,
            "counter_seconds": deltas[2] / 1000000, "tick_seconds": deltas[1] / 1000,
            "game_to_host_ratio": [deltas[3] / 1000 / high, deltas[3] / 1000 / low],
            "host_fps": [deltas[4] / high, deltas[4] / low]}
    if len(deltas) == 10:
        result.update(physics_steps=deltas[5], physics_seconds=deltas[6] / 1000000,
                      physics_to_host_ratio=[deltas[6] / 1000000 / high, deltas[6] / 1000000 / low],
                      bot_scheduler_calls=deltas[7], bot_think_calls=deltas[8])
    return result


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("runs", nargs="+", type=Path)
    for directory in parser.parse_args().runs:
        print(json.dumps(report(directory), sort_keys=True))
