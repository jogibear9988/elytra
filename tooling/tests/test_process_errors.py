#!/usr/bin/env python3
"""Compare genuine callback exceptions and Promise rejection events in Node and Mite."""

import _support

import argparse
import os
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/process-errors"
FATAL = {
    "timer": (1, "", "controlled-fatal-timer"),
    "microtask": (1, "", "controlled-fatal-microtask"),
    "nextTick": (1, "", "controlled-fatal-nextTick"),
    "io": (1, "", "controlled-fatal-io"),
    "monitor-only": (1, "MONITOR\n", "controlled-fatal-monitor-only"),
    "handler-throws": (7, "MONITOR\nHANDLER\n", "controlled-fatal-listener"),
    "monitor-throws": (7, "MONITOR\n", "controlled-fatal-listener"),
    "rejection-default": (1, "", "controlled-fatal-rejection-default"),
    "rejection-default-monitor": (
        1,
        "MONITOR:unhandledRejection\n",
        "controlled-fatal-rejection-default-monitor",
    ),
    "rejection-handler-throws": (
        1,
        "REJECTION\nMONITOR:uncaughtException\n",
        "controlled-fatal-listener",
    ),
    "handled-handler-throws": (
        1,
        "REJECTION\nHANDLED\nMONITOR:uncaughtException\n",
        "controlled-fatal-listener",
    ),
}


def invoke(name, binary, arguments):
    environment = dict(os.environ)
    environment.pop("MITE_PROCESS_ERRORS_GC", None)
    if name == "Mite":
        environment["MITE_PROCESS_ERRORS_GC"] = "cli-without-gc"
    result = subprocess.run(
        [str(binary), *(["--expose-gc"] if name == "Node" else []), *arguments],
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=10,
        env=environment,
    )
    if name == "Host":
        assert result.stderr.endswith("HOST_SHUTDOWN\n"), result
        assert result.stderr.count("HOST_SHUTDOWN\n") == 1, result
        result.stderr = result.stderr.removesuffix("HOST_SHUTDOWN\n")
    else:
        assert "HOST_SHUTDOWN" not in result.stderr, result
    return result


def run(name, binary, fixture, *args):
    return invoke(name, binary, [str(FIXTURES / fixture), *args])


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--host", type=Path, help="Optional genuine non-GUI Mite embedding host")
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Require --mite and Node, or --reference-only")
    runtimes = [("Node", args.node)]
    if not args.reference_only:
        runtimes.append(("Mite", args.mite.resolve()))
        if args.host:
            runtimes.append(("Host", args.host.resolve()))
    print(
        "GC roles: Node=real --expose-gc; "
        + ("Mite CLI=behavior only, no collector; " if not args.reference_only else "")
        + (
            "embedded Host=required real collector and idle reclamation"
            if args.host and not args.reference_only
            else "embedded Host not requested"
        ),
        flush=True,
    )
    for fixture, count in [("recovery.mjs", 5), ("rejections.mjs", 2), ("rejection-origin.mjs", 1)]:
        expected = None
        for name, binary in runtimes:
            result = run(name, binary, fixture)
            assert result.returncode == 0 and not result.stderr, (name, fixture, result)
            lines = result.stdout.splitlines()
            assert len(lines) == count and all(line.startswith("PASS ") for line in lines), (
                name,
                fixture,
                result,
            )
            if expected is None:
                expected = result.stdout
            assert result.stdout == expected, (name, fixture, result)
        print(expected, end="", flush=True)
    for mode in ["unhandledRejection", "rejectionHandled"]:
        expected = None
        for name, binary in runtimes:
            result = run(name, binary, "rejection-callbacks.mjs", mode)
            assert result.returncode == 0 and not result.stderr, (name, mode, result)
            assert (
                result.stdout
                == f"PASS process {mode} callback throw reaches monitor and uncaught recovery once\n"
            ), (name, mode, result)
            if expected is None:
                expected = result.stdout
            assert result.stdout == expected, (name, mode, result)
        print(expected, end="", flush=True)
    for fixture, mode, marker in [
        (
            "entry.mjs",
            "throw",
            "PASS process ESM entry throw uses uncaught exception before rejection listeners\n",
        ),
        (
            "entry.mjs",
            "await",
            "PASS process ESM entry await uses uncaught exception before rejection listeners\n",
        ),
        ("entry.cjs", "", "PASS process CommonJS entry recovery retains synchronous origin\n"),
    ]:
        for name, binary in runtimes:
            result = run(name, binary, fixture, *([mode] if mode else []))
            assert result.returncode == 0 and not result.stderr and result.stdout == marker, (
                name,
                fixture,
                mode,
                result,
            )
        print(marker, end="", flush=True)
    source = (FIXTURES / "entry.cjs").read_text().replace("CommonJS", "eval")
    marker = "PASS process eval entry recovery retains synchronous origin\n"
    for name, binary in runtimes:
        result = invoke(name, binary, ["-e", source])
        assert result.returncode == 0 and not result.stderr and result.stdout == marker, (
            name,
            result,
        )
    print(marker, end="", flush=True)
    for mode in ["string", "undefined", "null", "number"]:
        marker = f"PASS process default primitive {mode} rejection uses ERR_UNHANDLED_REJECTION\n"
        for name, binary in runtimes:
            result = run(name, binary, "primitive.mjs", mode)
            assert result.returncode == 0 and not result.stderr and result.stdout == marker, (
                name,
                mode,
                result,
            )
        print(marker, end="", flush=True)
    for mode, (node_status, output, marker) in FATAL.items():
        for name, binary in runtimes:
            result = run(name, binary, "fatal.mjs", mode)
            assert result.returncode == node_status, (name, mode, result)
            assert result.stdout == output, (name, mode, result)
            assert marker in result.stderr, (name, mode, result)
            assert "FORBIDDEN" not in result.stdout, (name, mode, result)
        print(
            f"PASS process fatal {mode} nonzero without continued or recursive callbacks",
            flush=True,
        )
    if args.host and not args.reference_only:
        result = run("Host", args.host.resolve(), "gc-reported.mjs")
        marker = "PASS embedded process reported promise and ephemeral reason reclaim after notification\n"
        assert result.returncode == 0 and not result.stderr and result.stdout == marker, result
        print(marker, end="", flush=True)


if __name__ == "__main__":
    main()
