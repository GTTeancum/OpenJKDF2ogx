#!/usr/bin/env python
from __future__ import print_function

import argparse
import os
import sys


REPO_ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
DEFAULT_LOG_DIR = os.path.join(REPO_ROOT, "build", "xbox", "xemu_ram_logs")


COMMON_MARKERS = [
    "Smoke: XSL autostart",
    "Smoke: XSL ready auto localPlayers=4",
    "XSL lobby started",
    "XSL peer discovered",
    "XSL gameplay begin",
    "MPLoadTrace: GameplayShow done",
    "SplitScreenPostLoad: armed enabled=1 locals=4",
]

HOST_MARKERS = [
    "XSL SMOKE harness role=host",
    "XSL host launch scheduled",
    "XSL host launch commit announced",
    "XSL roster applied host=1",
    "SplitScreenPostLoad: slot=0 player=0",
    "SplitScreenPostLoad: slot=3 player=3",
]

CLIENT_MARKERS = [
    "XSL SMOKE harness role=client",
    "XSL client launch acked",
    "XSL client launch commit received",
    "XSL roster applied host=0",
    "SplitScreenPostLoad: slot=0 player=4",
    "SplitScreenPostLoad: slot=3 player=7",
    "XSL SMOKE keepalive server timeout suppressed",
]

REAL_HOST_MARKERS = [
    "XSL lobby role",
    "XSL host session registered",
    "XSL host launch scheduled",
    "XSL host launch commit announced",
    "XSL gameplay begin host=1",
    "XSL roster applied host=1",
    "SplitScreenPostLoad: slot=0 player=0",
    "SplitScreenPostLoad: slot=3 player=3",
]

REAL_CLIENT_MARKERS = [
    "XSL lobby role",
    "XSL client session registered",
    "XSL secure addr translated",
    "XSL client launch acked",
    "XSL client launch commit received",
    "XSL gameplay begin host=0",
    "XSL roster applied host=0",
    "SplitScreenPostLoad: slot=0 player=4",
    "SplitScreenPostLoad: slot=3 player=7",
]

FORBIDDEN_MARKERS = [
    "gameplay-leave-enter",
    "jkGuiMultiTally_Show",
    "SERVER_LEFT_GAME",
    "HAS_LEFT_THE_GAME",
]

REAL_FORBIDDEN_MARKERS = FORBIDDEN_MARKERS + [
    "XSL SMOKE harness",
    "smoke-harness synthetic",
    "XSL SMOKE keepalive server timeout suppressed",
]


def read_file(path):
    with open(path, "rb") as handle:
        return handle.read().decode("utf-8", errors="replace")


def missing_markers(text, markers):
    return [marker for marker in markers if marker not in text]


def present_markers(text, markers):
    return [marker for marker in markers if marker in text]


def detect_real_lobby_roles(first_path, first_text, second_path, second_text):
    first_is_host = "XSL gameplay begin host=1" in first_text
    first_is_client = "XSL gameplay begin host=0" in first_text
    second_is_host = "XSL gameplay begin host=1" in second_text
    second_is_client = "XSL gameplay begin host=0" in second_text

    if first_is_host and second_is_client:
        return first_path, first_text, second_path, second_text
    if second_is_host and first_is_client:
        return second_path, second_text, first_path, first_text

    print("could not determine real-lobby host/client roles:", file=sys.stderr)
    print("  first:  %s host=%s client=%s" % (first_path, first_is_host, first_is_client), file=sys.stderr)
    print("  second: %s host=%s client=%s" % (second_path, second_is_host, second_is_client), file=sys.stderr)
    return None


def main():
    parser = argparse.ArgumentParser(description="Verify OpenJKDF2 two-XEMU System Link smoke RAM logs.")
    parser.add_argument("--mode", choices=("harness", "real-lobby"), default="harness",
                        help="harness verifies the synthetic role proof; real-lobby verifies peer discovery and secure launch.")
    parser.add_argument("--log-dir", default=DEFAULT_LOG_DIR, help="Directory containing poll_xemu_ram_log.py outputs.")
    parser.add_argument("--host-log", default=None, help="Host RAM log path. Defaults to port4488 output.")
    parser.add_argument("--client-log", default=None, help="Client RAM log path. Defaults to port4489 output.")
    args = parser.parse_args()

    host_log = args.host_log or os.path.join(args.log_dir, "port4488_openjkdf2_ram_log.txt")
    client_log = args.client_log or os.path.join(args.log_dir, "port4489_openjkdf2_ram_log.txt")

    for path in (host_log, client_log):
        if not os.path.exists(path):
            print("missing log: %s" % path, file=sys.stderr)
            return 2

    host_text = read_file(host_log)
    client_text = read_file(client_log)

    if args.mode == "real-lobby":
        roles = detect_real_lobby_roles(host_log, host_text, client_log, client_text)
        if roles is None:
            return 1
        host_log, host_text, client_log, client_text = roles
        host_missing = missing_markers(host_text, COMMON_MARKERS + REAL_HOST_MARKERS)
        client_missing = missing_markers(client_text, COMMON_MARKERS + REAL_CLIENT_MARKERS)
        host_forbidden = present_markers(host_text, REAL_FORBIDDEN_MARKERS)
        client_forbidden = present_markers(client_text, REAL_FORBIDDEN_MARKERS)
    else:
        host_missing = missing_markers(host_text, COMMON_MARKERS + HOST_MARKERS)
        client_missing = missing_markers(client_text, COMMON_MARKERS + CLIENT_MARKERS)
        host_forbidden = present_markers(host_text, FORBIDDEN_MARKERS)
        client_forbidden = present_markers(client_text, FORBIDDEN_MARKERS)

    if host_missing or client_missing or host_forbidden or client_forbidden:
        if host_missing:
            print("host missing:")
            for marker in host_missing:
                print("  %s" % marker)
        if client_missing:
            print("client missing:")
            for marker in client_missing:
                print("  %s" % marker)
        if host_forbidden:
            print("host forbidden markers present:")
            for marker in host_forbidden:
                print("  %s" % marker)
        if client_forbidden:
            print("client forbidden markers present:")
            for marker in client_forbidden:
                print("  %s" % marker)
        return 1

    if args.mode == "real-lobby":
        print("System Link real-lobby smoke verified:")
    else:
        print("System Link smoke harness verified:")
    print("  host:   %s" % host_log)
    print("  client: %s" % client_log)
    if args.mode == "real-lobby":
        print("  proof:  peer discovery, secure launch, and 4+4 local players reached gameplay")
    else:
        print("  proof:  host local players 0-3 and client local players 4-7 reached gameplay")
    return 0


if __name__ == "__main__":
    sys.exit(main())
