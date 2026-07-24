#!/usr/bin/env python3
from __future__ import annotations

import argparse
import hashlib
import json
import pathlib
import re
import sys


EVENT_RE = re.compile(
    r"(?:MAIN|CLIENT|OVERLAY)->(?:MAIN|CLIENT|OVERLAY)::[a-z0-9-]+"
)


def tree_digest(root: pathlib.Path, relative: str) -> str:
    base = root / relative
    digest = hashlib.sha256()
    for path in sorted(p for p in base.rglob("*") if p.is_file()):
        digest.update(path.relative_to(root).as_posix().encode())
        digest.update(b"\0")
        digest.update(path.read_bytes())
        digest.update(b"\0")
    return digest.hexdigest()


def events_in(paths: list[pathlib.Path]) -> set[str]:
    events: set[str] = set()
    for path in paths:
        events.update(EVENT_RE.findall(path.read_text(errors="replace")))
    return events


def main() -> int:
    parser = argparse.ArgumentParser(
        description="Check the native host against the reviewed upstream backend contract."
    )
    parser.add_argument(
        "root", nargs="?", default=".", help="Patched awakened-poe-trade checkout"
    )
    parser.add_argument(
        "--require-complete",
        action="store_true",
        help="Also fail while any capability is not marked implemented",
    )
    args = parser.parse_args()

    root = pathlib.Path(args.root).resolve()
    contract_path = root / "native/compat/upstream-contract.json"
    contract = json.loads(contract_path.read_text())

    failures: list[str] = []
    for relative, expected in contract["watchedTrees"].items():
        actual = tree_digest(root, relative)
        if actual != expected:
            failures.append(
                f"upstream backend tree changed: {relative}\n"
                f"  reviewed: {expected}\n"
                f"  current:  {actual}"
            )

    ipc_events = events_in(sorted((root / "ipc").glob("*.ts")))
    native_events = events_in(
        sorted((root / "native/src").glob("*.cpp"))
        + sorted((root / "native/src").glob("*.h"))
    )
    missing_events = sorted(ipc_events - native_events)
    if missing_events:
        failures.append(
            "IPC events missing from native source:\n  " + "\n  ".join(missing_events)
        )

    incomplete = {
        name: state
        for name, state in contract["capabilities"].items()
        if state != "implemented"
    }
    if args.require_complete and incomplete:
        failures.append(
            "native capabilities not complete:\n  "
            + "\n  ".join(f"{name}: {state}" for name, state in incomplete.items())
        )

    if failures:
        print("\n\n".join(failures), file=sys.stderr)
        print(
            "\nReview the upstream diff, port backend behavior, test it, then update "
            "native/compat/upstream-contract.json.",
            file=sys.stderr,
        )
        return 1

    print(
        f"Upstream contract matches; {len(ipc_events)} IPC events are represented "
        f"by the native host."
    )
    if incomplete:
        print(
            "Release-parity work remaining: "
            + ", ".join(f"{name} ({state})" for name, state in incomplete.items())
        )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
