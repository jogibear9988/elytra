#!/usr/bin/env python3
"""Prove header precedence checks detect the actual mixed-layout crash pattern."""

import _support
from pathlib import Path
import tempfile

from check_hub_frontend_headers import HEADER, verify


with tempfile.TemporaryDirectory(prefix="elytra-hub-header-policy-") as directory:
    root = Path(directory)
    vendor, overlay = root / "vendor", root / "overlay"
    for path in (vendor, overlay):
        (path / HEADER).parent.mkdir(parents=True)
        (path / HEADER).write_text("// actual header selection test\n")

    def command(target, includes):
        return {
            "directory": str(root),
            "file": "ContentView.cpp",
            "arguments": [
                "clang++",
                *includes,
                "-c",
                "ContentView.cpp",
                "-o",
                f"CMakeFiles/{target}.dir/ContentView.cpp.o",
            ],
        }

    good = command("firefly-application", ["-I", str(overlay), "-I" + str(vendor)])
    standalone = command("ladybird_impl", ["-I" + str(vendor)])
    assert verify([good, standalone], overlay, ["firefly-application"]) == 1
    # -I is searched before -isystem regardless of argument appearance.
    assert (
        verify(
            [command("firefly-application", ["-isystem", str(vendor), "-I" + str(overlay)])],
            overlay,
        )
        == 1
    )
    for bad, required in [
        (command("firefly-application", ["-I" + str(vendor), "-I" + str(overlay)]), []),
        (standalone, ["firefly-application"]),
        (command("firefly-application", ["-I" + str(vendor)]), ["firefly-application"]),
        (command("firefly-application", ["-isystem" + str(overlay), "-I" + str(vendor)]), []),
    ]:
        try:
            verify([bad], overlay, required)
        except ValueError:
            pass
        else:
            raise AssertionError("Missing/misordered embedded header policy accepted")
    standalone["file"] = str(vendor / "UI/Qt/RemoteView.cpp")
    assert verify([good, standalone], overlay, ["firefly-application"], vendor) == 1
    standalone["arguments"][1:1] = ["-I" + str(overlay)]
    try:
        verify([standalone], overlay, upstream=vendor)
    except ValueError:
        pass
    else:
        raise AssertionError("Standalone original frontend acquired the embedded class layout")
print("PASS embedded Hub header precedence, required consumers and standalone layout isolation")
