#!/usr/bin/env python3
"""Compose exact canvas and owned-view zoom seams without changing Ladybird."""

import argparse
import hashlib
from pathlib import Path

if __package__:
    from . import prepare_canvas_ui as canvas
else:
    import prepare_canvas_ui as canvas

ROOT = Path(__file__).resolve().parents[1]
SOURCE = canvas.SOURCE
DIGEST = canvas.DIGEST
PREFIX = '#include "native/zoom/View.h"\n'
NEEDLE = "void ViewImplementation::apply_zoom_for_current_host()\n{\n"
REPLACEMENT = NEEDLE + "    if (Firefly::Zoom::owns_view(*this))\n        return;\n"


def revert(output):
    """Undo only the two owned seams, refusing missing or repeated compositions."""
    if not output.startswith(PREFIX + canvas.PREFIX):
        raise ValueError("Review view preferences adapter prefix")
    if output.count(PREFIX) != 1 or output.count(canvas.PREFIX) != 1:
        raise ValueError("Review repeated view preferences adapter prefix")
    if output.count(REPLACEMENT) != 1 or output.count(canvas.REPLACEMENT) != 1:
        raise ValueError("Review view preferences adapter reversal")
    return (
        output.removeprefix(PREFIX + canvas.PREFIX)
        .replace(REPLACEMENT, NEEDLE)
        .replace(canvas.REPLACEMENT, canvas.NEEDLE)
    )


def adapt(original):
    """Preserve the existing canvas composition and unowned upstream zoom policy."""
    if original.count(NEEDLE) != 1 or PREFIX in original or REPLACEMENT in original:
        raise ValueError("Review owned-view zoom seam after upstream change")
    output = PREFIX + canvas.adapt(original).replace(NEEDLE, REPLACEMENT)
    if revert(output) != original:
        raise ValueError("View preferences adapter changed unrelated source bytes")
    return output


def prepare(source, output_dir):
    """Validate source and reversal before creating or replacing any output."""
    data = Path(source).read_bytes()
    if hashlib.sha256(data).hexdigest() != DIGEST:
        raise ValueError(f"Review view preferences source pin after Ladybird update: {SOURCE}")
    original = data.decode("utf-8")
    output = adapt(original)
    for invalid in (
        original.replace(NEEDLE, ""),
        original + NEEDLE,
        PREFIX + original,
        original.replace(canvas.NEEDLE, ""),
        original + canvas.NEEDLE,
        canvas.PREFIX + original,
        output,
    ):
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("View preferences accepted missing/duplicate/repeated seam")
    target = Path(output_dir) / "ViewImplementation.cpp"
    if not target.exists() or target.read_bytes() != output.encode():
        target.parent.mkdir(parents=True, exist_ok=True)
        target.write_bytes(output.encode())
    return target


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output-dir", type=Path, required=True)
    args = parser.parse_args()
    try:
        prepare(ROOT / "vendor/ladybird" / SOURCE, args.output_dir)
    except ValueError as error:
        raise SystemExit(str(error)) from error
    print("PASS view preferences pin, canvas/zoom exact reversal and seam drift/repetition refusal")


if __name__ == "__main__":
    main()
