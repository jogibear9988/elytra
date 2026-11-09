#!/usr/bin/env python3
"""Verify composed view source fidelity, standalone canvas and refusal before writes."""

import _support

from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]
sys.path.insert(0, str(ROOT / "tooling"))
import prepare_canvas_ui as canvas
import prepare_view_preferences as preferences


def main():
    source = ROOT / "vendor/ladybird" / preferences.SOURCE
    original = source.read_text()
    adapted = preferences.adapt(original)
    assert preferences.revert(adapted) == original
    # The upstream host-settings implementation remains intact for unowned views.
    original_zoom = original.split(preferences.NEEDLE, 1)[1].split("\n}\n", 1)[0]
    assert preferences.REPLACEMENT + original_zoom + "\n}\n" in adapted
    assert adapted.removeprefix(preferences.PREFIX).replace(
        preferences.REPLACEMENT, preferences.NEEDLE
    ) == canvas.adapt(original)
    for invalid in (
        original.replace(preferences.NEEDLE, ""),
        original + preferences.NEEDLE,
        preferences.PREFIX + original,
        original.replace(canvas.NEEDLE, ""),
        original + canvas.NEEDLE,
        canvas.PREFIX + original,
        adapted,
    ):
        try:
            preferences.adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing, duplicate or repeated source seam accepted")
    with tempfile.TemporaryDirectory(prefix="elytra-view-preferences-") as temporary:
        root = Path(temporary)
        standalone = root / "canvas"
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tooling/prepare_canvas_ui.py"),
                "--output-dir",
                str(standalone),
            ],
            check=True,
            timeout=10,
        )
        assert (standalone / "ViewImplementation.cpp").read_text() == canvas.adapt(original)
        output = root / "composed"
        target = preferences.prepare(source, output)
        assert target.read_text() == adapted
        modified = target.stat().st_mtime_ns
        assert preferences.prepare(source, output) == target
        assert target.stat().st_mtime_ns == modified
        drifted = root / "drifted.cpp"
        drifted.write_text(original + "\n")
        for destination in (root / "must-not-exist", output):
            try:
                preferences.prepare(drifted, destination)
            except ValueError as error:
                assert "source pin" in str(error)
            else:
                raise AssertionError("Unpinned source accepted")
            assert not (root / "must-not-exist").exists()
            assert target.read_text() == adapted and target.stat().st_mtime_ns == modified
    print(
        "PASS actual view source composition, standalone canvas, exact reversal and fail-before-write"
    )


if __name__ == "__main__":
    main()
