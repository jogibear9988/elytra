#!/usr/bin/env python3
"""Check exact Hub callback retirement composition, without starting Qt or a Hub."""

import _support
import hashlib
from pathlib import Path
import tempfile

import prepare_hub_connection as connection

ROOT = Path(__file__).resolve().parents[2]


def rejects(operation):
    try:
        operation()
    except ValueError:
        return
    raise AssertionError("Invalid Hub connection composition accepted")


original = (ROOT / "vendor/ladybird" / connection.SOURCE).read_text()
generated = connection.adapt(original)
assert connection.revert(generated) == original
assert generated.count(connection.CLEANUP) == 1
# The actual callback stores are cleared only for the exact current endpoint.
# Already-retired endpoints neither dereference the Application nor clear a new
# endpoint's callbacks, even when their delayed IPC closures are finally freed.
assert "if (s_the != &connection)\n        return;" in generated
assert "if (s_the != this)\n        return;" in generated
assert generated.count("Hub::release_embedded_ui_callbacks(connection);") == 1
for before, after in connection.SEAMS:
    for changed in (original.replace(before, ""), original + before):
        rejects(lambda: connection.adapt(changed))
    for changed in (generated.replace(after, ""), generated + after):
        rejects(lambda: connection.revert(changed))
rejects(lambda: connection.adapt(generated))
rejects(lambda: connection.revert(original))

with tempfile.TemporaryDirectory(prefix="elytra-hub-connection-") as temporary:
    root = Path(temporary)
    source, output = root / "upstream.cpp", root / "generated/ConnectionFromUI.cpp"
    source.write_text(original)
    connection.prepare(source, output)
    timestamp = output.stat().st_mtime_ns
    connection.prepare(source, output)
    assert output.read_text() == generated and output.stat().st_mtime_ns == timestamp
    source.write_text(original + "\n// unreviewed drift\n")
    rejects(lambda: connection.prepare(source, output))
    assert output.read_text() == generated and output.stat().st_mtime_ns == timestamp
    absent = root / "absent/ConnectionFromUI.cpp"
    rejects(lambda: connection.prepare(source, absent))
    assert not absent.parent.exists()
    fingerprint = connection.DIGEST
    try:
        # A refreshed hash alone must not hide a missing/duplicate native seam.
        for before, _ in connection.SEAMS:
            for changed in (original.replace(before, ""), original + before):
                source.write_text(changed)
                connection.DIGEST = hashlib.sha256(source.read_bytes()).hexdigest()
                rejects(lambda: connection.prepare(source, absent))
                assert not absent.parent.exists()
    finally:
        connection.DIGEST = fingerprint
print("PASS Hub endpoint pin/reversal/idempotence/drift and missing/duplicate retirement refusal")
