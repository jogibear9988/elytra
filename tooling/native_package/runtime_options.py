"""Verify executable-owned options before embedding them in a package launcher."""

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
