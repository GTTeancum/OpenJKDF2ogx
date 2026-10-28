"""Launch each untested retail MotS MP map in native XEMU without host input."""

from __future__ import annotations

import argparse
import json
import re
import subprocess
from pathlib import Path


ROOT = Path(__file__).resolve().parents[3]
ARCHIVES = {"JKM_MP.GOO": "JKM_MP", "JKM_KFY.GOO": "JKM_KFY",
            "JKM_SABER.GOO": "JKM_SABER"}
ALREADY_TESTED = {("JKM_MP.GOO", "mdm02_freezer.jkl"),
                  ("JKM_MP.GOO", "mdm03_canopy.jkl"),
                  ("JKM_KFY.GOO", "k1.jkl"),
                  ("JKM_SABER.GOO", "MSB1_Home.jkl")}


def has_gameplay_capture(row: dict) -> bool:
    run_dir = Path(row["runDir"]) if row.get("runDir") else None
    if not run_dir or not run_dir.is_dir():
        return False
    gameplay_poll = None
    for ram_file in sorted(run_dir.glob("ram_poll_*.txt")):
        if "MPLoadTrace: GameplayShow done" in ram_file.read_text(encoding="utf-8", errors="replace"):
            match = re.search(r"_(\d+)s\.txt$", ram_file.name)
            gameplay_poll = int(match.group(1)) if match else None
            break
    if gameplay_poll is None:
        return False
    return any((match := re.search(r"_(\d+)s\.png$", path))
               and int(match.group(1)) > gameplay_poll for path in row.get("screenshots", []))


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--limit", type=int, default=0)
    parser.add_argument("--include-tested", action="store_true")
    parser.add_argument("--retry-failed", action="store_true")
    parser.add_argument("--duration", type=int, default=35)
    args = parser.parse_args()

    work = ROOT / "build" / "xbox" / "mots_validation"
    audit = json.loads((work / "mp_map_audit.json").read_text(encoding="utf-8"))
    output = work / "mp_map_matrix.json"
    rows = json.loads(output.read_text(encoding="utf-8")) if args.retry_failed and output.exists() else []
    targets = [m for m in audit["maps"]
               if args.include_tested or (m["archive"], m["level"]) not in ALREADY_TESTED]
    if args.retry_failed:
        previous = {(row["archive"], row["level"]): row for row in rows}
        targets = [m for m in targets if not (
            (old := previous.get((m["archive"], m["level"])))
            and old.get("ok") and has_gameplay_capture(old))]
    if args.limit:
        targets = targets[:args.limit]
    for index, entry in enumerate(targets, 1):
        archive, level = entry["archive"], entry["level"]
        if archive not in ARCHIVES or not re.fullmatch(r"[A-Za-z0-9_]+\.jkl", level):
            raise ValueError(f"Unexpected MP map record: {archive}/{level}")
        episode = ARCHIVES[archive]
        map_name = level[:-4]
        label = f"mots-matrix-{episode}-{map_name}".lower()
        log = work / f"{label}.log"
        command = [
            "powershell.exe", "-NoProfile", "-ExecutionPolicy", "Bypass", "-File",
            str(ROOT / "tools" / "xbox_xemu_smoke.ps1"),
            "-DurationSeconds", str(args.duration), "-PollIntervalSeconds", "4",
            "-RunLabel", label,
            "-AutoStartArgs", f"-autostart -mp -episode {episode} -map {map_name} -motsCompat",
            "-SnapshotDisk", "-NoHostInput", "-MuteHostAudio",
            "-SystemScanMode", "480p", "-ScreenshotEverySeconds", "10",
            "-WorkRoot", str(work),
        ]
        print(f"[{index}/{len(targets)}] {episode}/{level}", flush=True)
        with log.open("wb") as stream:
            result = subprocess.run(command, cwd=ROOT, stdout=stream, stderr=subprocess.STDOUT,
                                    check=False)
        report = log.read_text(encoding="utf-8", errors="replace")
        run_match = re.search(r"^runDir=(.+)$", report, re.MULTILINE)
        run_dir = Path(run_match.group(1).strip()) if run_match else None
        ram_text = ""
        screenshots = []
        gameplay_poll = None
        if run_dir and run_dir.is_dir():
            ram_files = sorted(run_dir.glob("ram_poll_*.txt"))
            if ram_files:
                ram_text = ram_files[-1].read_text(encoding="utf-8", errors="replace")
            for ram_file in ram_files:
                if "MPLoadTrace: GameplayShow done" in ram_file.read_text(encoding="utf-8", errors="replace"):
                    match = re.search(r"_(\d+)s\.txt$", ram_file.name)
                    gameplay_poll = int(match.group(1)) if match else None
                    break
            screenshots = [str(path) for path in sorted((run_dir / "screenshots").glob("shot_*.png"))]
        visual_ready = bool(gameplay_poll is not None and any(
            (match := re.search(r"_(\d+)s\.png$", path)) and int(match.group(1)) > gameplay_poll
            for path in screenshots))
        ok = (result.returncode == 0 and "fatalCount=0" in report
              and re.search(r"^reached=.*gameplay-frames", report, re.MULTILINE)
              and f"TitleShowLoading: enter level='{level}'" in ram_text
              and visual_ready)
        row = {"archive": archive, "level": level, "ok": bool(ok),
                     "visualReady": visual_ready, "gameplayPollSeconds": gameplay_poll,
                     "exitCode": result.returncode, "runDir": str(run_dir) if run_dir else None,
                     "screenshots": screenshots, "log": str(log)}
        rows = [old for old in rows if (old["archive"], old["level"]) != (archive, level)]
        rows.append(row)
        output.write_text(json.dumps(rows, indent=2) + "\n", encoding="utf-8")
        print(f"  {'PASS' if ok else 'FAIL'} screenshots={len(screenshots)} "
              f"gameplayPoll={gameplay_poll} visualReady={visual_ready}", flush=True)
    if not all(row["ok"] for row in rows):
        raise SystemExit(1)


if __name__ == "__main__":
    main()
