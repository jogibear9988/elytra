#!/usr/bin/env python3
"""Compose one JIT-only item; retain upstream Cargo inputs and every other Rust module."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "vendor/ladybird/Libraries/LibJS/Rust/src"
PINS = {
    "lib.rs": "f8b2ba0485760f1a922fb2015ad635360e621144548d6667274c7f4c46b2aff3",
    "unicode.rs": "96a7b1f5dd6115137f802cd00979372c93f4dc1d3980c4551a3c2b03a4681a78",
    "runtime/mod.rs": "01efbe78fba3b854d663fec7d337d668e116f5b6c06e426a9a3975fb2f066ea6",
    "runtime/ecmascript_function_object.rs": "4dc91ca34ba86b91a9d2b3b6e4fc0eb6a560babef8bdfac9a959d21a17576121",
}
SEAM = "    pub(crate) fn has_lazy_property(&self, vm: &Vm, property_key: &PropertyKey) -> bool {"
REPLACEMENT = '    #[cfg(feature = "jit")]\n' + SEAM


def digest(data):
    return hashlib.sha256(data).hexdigest()


def literal(path):
    value = str(path).replace("\\", "/")
    hashes = "#"
    while '"' + hashes in value:
        hashes += "#"
    return f'r{hashes}"{value}"{hashes}'


def item_source(source):
    if source.count(SEAM) != 1 or REPLACEMENT in source:
        raise ValueError("Review missing/duplicate/already composed non-JIT item")
    result = source.replace(SEAM, REPLACEMENT)
    assert result.replace(REPLACEMENT, SEAM) == source
    return result


def module_source(source, base, redirects):
    replacements = []
    pattern = r'(?:#\[path = "([^"\n]+)"\]\n)?((?:pub(?:\([^)]*\))? )?mod (\w+);)'

    def replace(match):
        old, declaration, name = match.groups()
        target = (base / old).resolve() if old else base / name
        if not old:
            target = (
                target.with_suffix(".rs")
                if target.with_suffix(".rs").is_file()
                else target / "mod.rs"
            )
        if not target.is_file():
            raise ValueError(f"Review missing upstream Rust module: {name}")
        # An explicit path to foo.rs changes its implicit child-module base.
        # Such file modules need their own checked routing unit; mod.rs roots
        # retain their directory semantics without another composition.
        if (
            target.name not in {"mod.rs", "lib.rs"}
            and re.search(r"^(?:pub(?:\([^)]*\))? )?mod \w+;", target.read_text(), re.M)
            and target.resolve() not in redirects
        ):
            raise ValueError(f"Review nested Rust file-module routing: {target}")
        target = redirects.get(target.resolve(), target)
        replacement = f"#[path = {literal(target)}]\n{declaration}"
        replacements.append((match.group(), replacement))
        return replacement

    result = re.sub(pattern, replace, source)
    restored = result
    for old, new in reversed(replacements):
        restored = restored.replace(new, old)
    if not replacements or restored != source:
        raise ValueError("Rust module composition changed unrelated bytes")
    return result


def prepare(output, driver, chain="", check=False):
    inputs = {}
    for name, expected in PINS.items():
        file = SOURCE / name
        if digest(file.read_bytes()) != expected:
            raise ValueError(f"Review pinned non-JIT source: {file}")
        inputs[str(file)] = expected
    for name in ("prepare.py", "driver.py", "Driver.cpp"):
        file = Path(__file__).parent / name
        inputs[str(file)] = digest(file.read_bytes())
    provenance = digest(json.dumps({"inputs": inputs, "chain": chain}, sort_keys=True).encode())
    redirects = {
        (SOURCE / "unicode.rs").resolve(): output / "unicode.rs",
        (SOURCE / "runtime/mod.rs").resolve(): output / "runtime.rs",
        (SOURCE / "runtime/ecmascript_function_object.rs").resolve(): output
        / "ecmascript_function_object.rs",
    }
    root = module_source((SOURCE / "lib.rs").read_text(), SOURCE, redirects)
    root += f"\n// Owned exact-item build-input fingerprint: {provenance}\n"
    planned = {
        "lib.rs": root.encode(),
        "unicode.rs": module_source(
            (SOURCE / "unicode.rs").read_text(), SOURCE / "unicode", redirects
        ).encode(),
        "runtime.rs": module_source(
            (SOURCE / "runtime/mod.rs").read_text(), SOURCE / "runtime", redirects
        ).encode(),
        "ecmascript_function_object.rs": item_source(
            (SOURCE / "runtime/ecmascript_function_object.rs").read_text()
        ).encode(),
    }
    config = {
        "schema": 1,
        "kind": "mite-rust-nojit-driver",
        "original": str(SOURCE / "lib.rs"),
        "adapted": str(output / "lib.rs"),
        "driver": str(driver),
        "chain": chain,
        "files": inputs | {str(output / name): digest(data) for name, data in planned.items()},
    }
    planned["config.json"] = (json.dumps(config, indent=2) + "\n").encode()
    planned["DriverConfig.h"] = "".join(
        f"#define {key} {json.dumps(value, ensure_ascii=False)}\n"
        for key, value in {
            "FIREFLY_DRIVER_PYTHON": sys.executable,
            "FIREFLY_DRIVER_SCRIPT": str(Path(__file__).parent / "driver.py"),
            "FIREFLY_DRIVER_CONFIG": str(output / "config.json"),
        }.items()
    ).encode()
    output.mkdir(parents=True, exist_ok=True)
    for name, data in planned.items():
        file = output / name
        if not file.exists() or file.read_bytes() != data:
            if check:
                raise ValueError(f"Generated non-JIT source drift: {file}")
            file.write_bytes(data)
    print(
        "PASS exact non-JIT item composition; original Cargo graph and all other modules retained"
    )


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--chain", default="")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    prepare(args.output.resolve(), args.driver.resolve(), args.chain, args.check)
