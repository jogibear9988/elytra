#!/usr/bin/env python3
"""Compare actual captured call frames and descriptor behavior, allowing LibJS formatting differences."""

import _support

import argparse
from pathlib import Path
import shutil
import subprocess

ROOT = Path(__file__).resolve().parents[2]
FIXTURES = ROOT / "mite/tests/error-stack"


def run(name, binary, fixture):
    result = subprocess.run(
        [str(binary), *(["--expose-gc"] if name == "Node" else []), str(FIXTURES / fixture)],
        capture_output=True,
        text=True,
        encoding="utf-8",
        timeout=15,
    )
    if name == "Host":
        assert (
            result.stderr.endswith("HOST_SHUTDOWN\n")
            and result.stderr.count("HOST_SHUTDOWN\n") == 1
        ), (name, fixture, result)
        result.stderr = result.stderr.removesuffix("HOST_SHUTDOWN\n")
    assert result.returncode == 0 and not result.stderr, (name, fixture, result)
    return result.stdout


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path)
    parser.add_argument("--host", type=Path)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument("--reference-only", action="store_true")
    args = parser.parse_args()
    if not args.node or (not args.reference_only and not args.mite):
        parser.error("Require Node and --mite, or --reference-only")
    runtimes = [("Node", args.node)]
    if not args.reference_only:
        runtimes.append(("Mite", args.mite.resolve()))
        if args.host:
            runtimes.append(("Host", args.host.resolve()))
    for fixture, count in [
        ("main.mjs", 3),
        ("frames.mjs", 2),
        ("validation.mjs", 2),
        ("limits.mjs", 1),
        ("receivers.mjs", 2),
        ("call-sites.mjs", 2),
        ("reentrant.mjs", 1),
        ("bindings.mjs", 1),
    ]:
        expected = None
        for name, binary in runtimes:
            output = run(name, binary, fixture)
            assert len(output.splitlines()) == count and all(
                line.startswith("PASS ") for line in output.splitlines()
            ), (name, fixture, output)
            if expected is None:
                expected = output
            assert output == expected, (name, fixture, expected, output)
        print(expected, end="", flush=True)
    prepared = None
    for name, binary in runtimes:
        output = run(name, binary, "prepare.mjs")
        assert len(output.splitlines()) == 3 and all(
            line.startswith("PASS ") for line in output.splitlines()
        ), (name, output)
        if prepared is None:
            prepared = output
        assert output == prepared, (name, prepared, output)
    print(prepared, end="", flush=True)
    reference = run("Node", args.node, "gc-active.mjs")
    assert (
        reference
        == "PASS captureStackTrace real GC preserves lazy formatter frames and arbitrary cached object identity\n"
    ), reference
    if args.host and not args.reference_only:
        assert run("Host", args.host.resolve(), "gc-active.mjs") == reference
        reclaimed = run("Host", args.host.resolve(), "gc-reclaimed.mjs")
        assert (
            reclaimed
            == "PASS embedded captureStackTrace unreachable native record target and cache reclaim after idle GC\n"
        ), reclaimed
        print(reference + reclaimed, end="", flush=True)
    else:
        print(reference, end="", flush=True)
    sites = run("Node", args.node, "gc-call-sites.mjs")
    assert sites == "PASS captureStackTrace retained CallSite source records survive real GC\n", (
        sites
    )
    if args.host and not args.reference_only:
        actual = run("Host", args.host.resolve(), "gc-call-sites.mjs")
        assert (
            actual
            == sites
            + "PASS embedded retained CallSites release unreachable capture owner after idle GC\n"
        ), actual
        print(actual, end="", flush=True)
    else:
        print(sites, end="", flush=True)
    print(
        "GC roles: Node uses --expose-gc; CLI behavior does not claim GC; embedded idle reclamation requires --host",
        flush=True,
    )


if __name__ == "__main__":
    main()
