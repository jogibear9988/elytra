"""Verify executable-owned options before embedding them in a package launcher."""

import os
import subprocess


def verify_electron_compat(runtime):
    # The runtime handles help before any GUI initialization. Older SDKs must
    # fail packaging rather than produce an app that cannot parse its launcher.
    result = subprocess.run(
        [str(runtime), "--electron-compat", "--help"],
        capture_output=True,
        text=True,
        timeout=10,
    )
    if result.returncode != 0 or not result.stdout.startswith("Usage: firefly [--electron-compat]"):
        raise ValueError("The selected Firefly SDK does not support --electron-compat")


def verify_libjs_revision(runtime, expected, *, firefly=False):
    """Read the engine revision from the executable before staging it."""
    environment = {
        key: value
        for key, value in os.environ.items()
        if not key.startswith(("FIREFLY_", "MITE_")) and key != "ELECTRON_RUN_AS_NODE"
    }
    if firefly:
        # Firefly enters its Mite CLI before initializing Qt or starting helpers.
        environment["ELECTRON_RUN_AS_NODE"] = "1"
    result = subprocess.run(
        [str(runtime), "-p", "process.versions.libjs"],
        env=environment,
        capture_output=True,
        text=True,
        timeout=15,
    )
    if result.returncode != 0 or result.stdout.splitlines() != [expected]:
        raise ValueError(f"{runtime} does not report the pinned LibJS revision {expected}")
