#!/usr/bin/env python3
"""Add native input provenance notification while preserving every other upstream byte."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "vendor/ladybird/Libraries/LibWeb/HTML/HTMLInputElement.cpp"
EXPECTED = "91e7a91600aa431b05262c1bfcbc461e612bff634d58bf8441e3e36bc4998ad2"
INCLUDE = '#include "files/Origins.h"\n#include <LibGC/Root.h>\n'
LIST = "    auto files = FileAPI::FileList::create();"
ROOTED = "\n    GC::Root file_origin_list { files };"
SEAM = "        files->add_file(file);"
HOOK = "        Firefly::FileOrigins::did_select(document(), selected_file, *file);\n"


def adapt(source):
    if (
        source.count(SEAM) != 1
        or source.count(LIST) != 1
        or "Firefly::FileOrigins" in source
        or INCLUDE in source
    ):
        raise ValueError("Review native selected-file creation seam")
    return INCLUDE + source.replace(LIST, LIST + ROOTED).replace(SEAM, HOOK + SEAM)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    original = SOURCE.read_bytes()
    if hashlib.sha256(original).hexdigest() != EXPECTED:
        raise SystemExit("Review owned file-origin adapter against updated upstream")
    source = original.decode()
    generated = adapt(source)
    assert (
        generated.removeprefix(INCLUDE).replace(HOOK + SEAM, SEAM).replace(LIST + ROOTED, LIST)
        == source
    )
    for changed in [source.replace(SEAM, ""), source.replace(LIST, ""), source + SEAM, generated]:
        try:
            adapt(changed)
        except ValueError:
            pass
        else:
            raise AssertionError("Drift/duplicate selected-file seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != generated:
        args.output.write_text(generated)
    print("PASS native input File provenance seam, pin, exact byte preservation and drift refusal")


if __name__ == "__main__":
    main()
