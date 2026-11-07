#!/usr/bin/env python3
"""Prepare an audited Code-OSS source overlay without changing upstream files."""

import argparse
import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "vendor/code-oss-tests/manifest.json"
NODE_CHECK = "typeof process?.versions?.node === 'string'"
NATIVE_CHECK = (
    "(typeof process?.versions?.node === 'string' || typeof process?.versions?.mite === 'string')"
)
FILES = ("platform.ts", "process.ts")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def adapt(name, data):
    """Change only declared identity checks and their matching TypeScript field."""
    source = data.decode("utf-8")
    if name not in FILES or source.count(NODE_CHECK) != 1:
        raise ValueError(f"Unexpected Code-OSS native identity seam: {name}")
    source = source.replace(NODE_CHECK, NATIVE_CHECK)
    if name == "platform.ts":
        field = "\t\tnode?: string;"
        if source.count(field) != 1:
            raise ValueError("Unexpected Code-OSS native process interface")
        source = source.replace(field, field + "\n\t\tmite?: string;")
    return source.encode("utf-8")


def prepare(source, output):
    """Validate every input before creating a fresh, explicitly partial overlay."""
    source = Path(source).resolve(strict=True)
    output = Path(output).absolute()
    target = output.resolve()
    if target == source or source in target.parents or target in source.parents:
        raise ValueError("Overlay output must be separate from its source tree")
    if output.exists() or output.is_symlink():
        raise ValueError("Overlay output must not already exist")
    manifest = json.loads(MANIFEST.read_text())
    planned = {}
    records = []
    for name in (*FILES, "LICENSE.txt"):
        entry = manifest["files"][name]
        original = (source / entry["upstream"]).read_bytes()
        # Upstream explicitly checks LICENSE.txt out with CRLF. Preserve either
        # exact pinned form; do not normalize sources or accept arbitrary drift.
        accepted = {entry["sha256"]}
        if name == "LICENSE.txt":
            accepted.add(entry["crlfCheckoutSha256"])
        if sha256(original) not in accepted:
            raise ValueError(f"Code-OSS source differs from pinned input: {entry['upstream']}")
        patched = adapt(name, original) if name in FILES else original
        planned[entry["upstream"]] = patched
        records.append(
            {
                "path": entry["upstream"],
                "sourceSha256": sha256(original),
                "overlaySha256": sha256(patched),
            }
        )
    report = {
        "schema": 1,
        "kind": "partial-source-overlay",
        "repository": manifest["repository"],
        "revision": manifest["revision"],
        "adaptations": [
            "Recognize process.versions.mite as native without claiming Node or Electron identity"
        ],
        "files": records,
    }
    output.mkdir(parents=True)
    for name, data in planned.items():
        destination = output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    (output / "ELYTRA-ADAPTATIONS.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Pinned Code-OSS checkout")
    parser.add_argument("--output", type=Path, required=True, help="Fresh partial source overlay")
    args = parser.parse_args()
    prepare(args.source, args.output)
    print(f"Prepared audited native-identity overlay in {args.output}")


if __name__ == "__main__":
    main()
