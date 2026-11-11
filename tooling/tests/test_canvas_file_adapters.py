#!/usr/bin/env python3
"""Exercise renderer canvas/file compositions and atomic refusal before writes."""

import _support

import hashlib
from pathlib import Path
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tooling"))
import prepare_canvas_hooks as canvas
import prepare_file_origin_hook as origin
import prepare_file_request_hook as request


def rejects(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Unreviewed or malformed renderer composition accepted")


def snapshot(directory):
    return {
        path.name: (path.read_bytes(), path.stat().st_mtime_ns)
        for path in directory.glob("*")
        if path.is_file()
    }


def canvas_checks(directory):
    source_root = directory / "source"
    for _, (relative, _, _, _) in canvas.ENTRIES.items():
        path = source_root / "vendor/ladybird" / relative
        path.parent.mkdir(parents=True, exist_ok=True)
        path.write_bytes((ROOT / "vendor/ladybird" / relative).read_bytes())
    output = directory / "generated"
    canvas.prepare(output, source_root)
    accepted = snapshot(output)
    canvas.prepare(output, source_root)
    assert snapshot(output) == accepted
    for name, (relative, _, seam, hook) in canvas.ENTRIES.items():
        source = (source_root / "vendor/ladybird" / relative).read_text()
        composed = (output / name).read_text()
        assert composed.removeprefix(canvas.PREFIX).replace(hook, seam) == source
        for invalid in (source.replace(seam, ""), source + seam, composed):
            rejects(lambda: canvas.adapt(invalid, seam, hook))
    assert ".blend(background_color(read))" in (output / "Document.cpp").read_text()
    assert (
        "inputs.opaque_canvas = opaque_canvas;" in (output / "PaintingRustBridge.cpp").read_text()
    )
    # Reject drift in the second unit before writing the already validated first.
    relative = canvas.ENTRIES["PaintingRustBridge.cpp"][0]
    source = source_root / "vendor/ladybird" / relative
    source.write_bytes(source.read_bytes() + b"\n// unreviewed\n")
    absent = directory / "absent"
    for destination in (absent, output):
        rejects(lambda: canvas.prepare(destination, source_root))
        assert not absent.exists()
        assert snapshot(output) == accepted


def file_checks(module, directory, seams):
    output = directory / "generated/unit.cpp"
    module.prepare(output)
    accepted = snapshot(output.parent)
    module.prepare(output)
    assert snapshot(output.parent) == accepted
    original = module.SOURCE.read_text()
    generated = module.adapt(original)
    assert output.read_text() == generated
    for seam in seams:
        rejects(lambda: module.adapt(original.replace(seam, "")))
        rejects(lambda: module.adapt(original + seam))
    rejects(lambda: module.adapt(generated))
    drift = directory / "unreviewed.cpp"
    drift.write_text(original + "\n// unreviewed\n")
    absent = directory / "absent/unit.cpp"
    for destination in (absent, output):
        rejects(lambda: module.prepare(destination, drift))
        assert not absent.parent.exists()
        assert snapshot(output.parent) == accepted
    expected = module.EXPECTED
    try:
        # A newly approved hash alone cannot authorize changed ownership seams.
        for seam in seams:
            for invalid in (original.replace(seam, ""), original + seam):
                drift.write_text(invalid)
                module.EXPECTED = hashlib.sha256(drift.read_bytes()).hexdigest()
                for destination in (absent, output):
                    rejects(lambda: module.prepare(destination, drift))
                    assert not absent.parent.exists()
                    assert snapshot(output.parent) == accepted
    finally:
        module.EXPECTED = expected


def main():
    with tempfile.TemporaryDirectory(prefix="elytra-canvas-file-source-") as temporary:
        directory = Path(temporary)
        canvas_checks(directory / "canvas")
        (directory / "origin").mkdir()
        (directory / "request").mkdir()
        file_checks(origin, directory / "origin", (origin.LIST, origin.SEAM))
        file_checks(
            request, directory / "request", (request.REQUEST, request.CLOSED, request.DELIVERY)
        )
        assert origin.ROOTED in origin.adapt(origin.SOURCE.read_text())
        adapted = request.adapt(request.SOURCE.read_text())
        assert request.REQUEST_HOOK in adapted and request.GUARDED in adapted
    print("PASS canvas/file source fidelity, GC/provenance guards, drift and fail-before-write")


if __name__ == "__main__":
    main()
