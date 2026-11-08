#!/usr/bin/env python3
"""Verify real public safeStorage in an isolated native keychain; never use login credentials."""

import _support

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile
from test_renderer import ROOT, run


def keychain_list():
    """Read preferences only, so tests can prove private keychains do not modify them."""
    return subprocess.check_output(
        ["/usr/bin/security", "list-keychains", "-d", "user"], text=True, timeout=5
    )


def component(probe):
    if sys.platform != "darwin":
        # Genuine provider-negative test; this does not claim Windows/Linux encryption support.
        with tempfile.TemporaryDirectory(prefix="elytra-safe-unavailable-") as directory:
            result = run(
                probe.resolve(),
                "--test-keychain",
                Path(directory) / "unused.keychain",
                "--test-profile",
                "native-negative",
                ROOT / "firefly/tests/safe-storage/public-unavailable.mjs",
            )
            assert (
                result.returncode == 0 and "PASS public safeStorage unavailable:" in result.stdout
            ), result
            assert "VERIFICATION FAILED" not in result.stderr, result
            print(result.stdout.strip(), flush=True)
        return
    before = keychain_list()
    with tempfile.TemporaryDirectory(prefix="elytra-safe-storage-") as directory:
        keychain = Path(directory) / "isolated.keychain"
        state = Path(directory) / "ciphertext.json"
        script = ROOT / "firefly/tests/safe-storage/public.mjs"
        try:
            for phase, profile in [
                ("write", "public-acceptance-profile"),
                ("read", "public-acceptance-profile"),
                ("wrong-key", "other-public-acceptance-profile"),
            ]:
                result = run(
                    probe.resolve(),
                    "--test-keychain",
                    keychain,
                    "--test-profile",
                    profile,
                    script,
                    phase,
                    state,
                )
                assert (
                    result.returncode == 0 and "PASS public safeStorage " + phase in result.stdout
                ), result
                assert "VERIFICATION FAILED" not in result.stderr, result
                assert keychain_list() == before, "Private test keychain changed user search list"
                print(result.stdout.strip(), flush=True)
        finally:
            # Only caller-created private paths are deleted; no implicit/default keychain calls.
            for path in [keychain, Path(str(keychain) + "-db")]:
                if path.exists():
                    subprocess.run(
                        ["/usr/bin/security", "delete-keychain", str(path)],
                        capture_output=True,
                        text=True,
                        timeout=5,
                        check=True,
                    )
            assert keychain_list() == before, "Private keychain cleanup changed user search list"


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, help="Native public safeStorage component host")
    parser.add_argument("--firefly", type=Path, help="Actual Firefly main for pre-ready guard only")
    parser.add_argument(
        "--electron", type=Path, help="Actual Electron for pre-ready reference only"
    )
    args = parser.parse_args()
    if not any([args.probe, args.firefly, args.electron]):
        parser.error("Provide --probe, --firefly and/or --electron")
    if args.probe:
        component(args.probe)
    with tempfile.TemporaryDirectory(prefix="elytra-safe-pre-ready-") as directory:
        source = (ROOT / "firefly/tests/safe-storage/pre-ready.mjs").read_text()
        for label, binary in [("Firefly", args.firefly), ("Electron reference", args.electron)]:
            if binary is None:
                continue
            script = Path(directory) / (label + ".mjs")
            script.write_text(
                source.replace("from 'firefly'", "from 'electron'")
                if label == "Electron reference"
                else source
            )
            result = run(binary.resolve(), script)
            assert (
                result.returncode == 0 and "PASS actual main safeStorage pre-ready" in result.stdout
            ), result
            print(label + ": " + result.stdout.strip(), flush=True)


if __name__ == "__main__":
    main()
