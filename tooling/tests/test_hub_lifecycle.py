#!/usr/bin/env python3
"""Check exact Hub startup composition and refusal before generated writes."""

import _support

import hashlib
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tooling"))
import prepare_hub_lifecycle as lifecycle


def rejects(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Invalid Hub lifecycle source accepted")


def main():
    source = ROOT / "vendor/ladybird" / lifecycle.SOURCE
    original = source.read_text()
    output = lifecycle.adapt(original)
    assert lifecycle.revert(output) == original
    assert output.count(lifecycle.PREFIX) == 1
    for before, after in lifecycle.SEAMS:
        assert output.count(after) == 1
        assert before not in output
        rejects(lambda: lifecycle.adapt(original.replace(before, "")))
        rejects(lambda: lifecycle.adapt(original + before))
        rejects(lambda: lifecycle.revert(output.replace(after, "")))
        rejects(lambda: lifecycle.revert(output + after))
    rejects(lambda: lifecycle.adapt(output))
    rejects(lambda: lifecycle.revert(original))
    rejects(lambda: lifecycle.revert(lifecycle.PREFIX + output))

    with tempfile.TemporaryDirectory(prefix="elytra-hub-lifecycle-") as temporary:
        directory = Path(temporary)
        target = directory / "generated/Application.cpp"
        lifecycle.prepare(source, target)
        assert target.read_text() == output
        timestamp = target.stat().st_mtime_ns
        lifecycle.prepare(source, target)
        assert target.stat().st_mtime_ns == timestamp
        assert target.read_text() == output

        drift = directory / "drifted.cpp"
        drift.write_text(original + "\n// unreviewed upstream change\n")
        absent = directory / "absent/Application.cpp"
        for destination in (absent, target):
            rejects(lambda: lifecycle.prepare(drift, destination))
            assert not absent.parent.exists()
            assert target.read_text() == output
            assert target.stat().st_mtime_ns == timestamp

        # Even an incorrectly refreshed fingerprint must not authorize missing
        # seams. Preparation checks structure before creating any output path.
        expected = lifecycle.DIGEST
        try:
            for before, _ in lifecycle.SEAMS:
                for invalid in (original.replace(before, ""), original + before):
                    drift.write_text(invalid)
                    lifecycle.DIGEST = hashlib.sha256(drift.read_bytes()).hexdigest()
                    for destination in (absent, target):
                        rejects(lambda: lifecycle.prepare(drift, destination))
                        assert not absent.parent.exists()
                        assert target.read_text() == output
                        assert target.stat().st_mtime_ns == timestamp
        finally:
            lifecycle.DIGEST = expected
    print(
        "PASS Hub lifecycle fingerprint, exact reversal, drift refusal, idempotence and no writes on failure"
    )


if __name__ == "__main__":
    main()
