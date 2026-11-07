#!/usr/bin/env python3
"""Compose one pinned Rust origin entry; reuse all upstream parser/modules and Cargo inputs."""

import argparse
import hashlib
import json
from pathlib import Path
import re
import sys

ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "vendor/ladybird/Libraries/LibURL/Rust/src"
PIN = {
    "ffi.rs": "17a5426baa3c27705c9fdd353008b1e6d6429361232b0125ee7e9098dbf6ab2b",
    "lib.rs": "ef89028f84e1795c498b80101cfacc3a09a5689893f654ecc159106d832a5c41",
    "origin.rs": "144b199d117db6d6f50fb2a253e201554bf7983aefeb7b48a71295e5670aaeb9",
}
SEAM = "    pub fn origin(&self) -> Origin {\n"
HOOK = "        if let Some(origin) = crate::firefly_origin::custom_origin(self.as_str()) {\n            return origin;\n        }\n"


def digest(data):
    return hashlib.sha256(data).hexdigest()


def literal(path):
    value = str(path).replace("\\", "/")
    hashes = "#"
    while '"' + hashes in value:
        hashes += "#"
    return f'r{hashes}"{value}"{hashes}'


def origin_source(data):
    if data.count(SEAM) != 1:
        raise ValueError("Missing/ambiguous Rust URL origin seam")
    result = data.replace(SEAM, SEAM + HOOK)
    assert result.replace(SEAM + HOOK, SEAM) == data
    return result


def module_source(data, directory, provenance, ffi=False):
    modules = set()
    replacements = []
    pattern = r'(?:#\[path = "([^"]+)"\]\n)?((?:pub )?mod (\w+);)'

    def replace(match):
        old_path, declaration, name = match.groups()
        modules.add(name)
        base = SOURCE / "ffi" if ffi else SOURCE
        target = (base / old_path).resolve() if old_path else base / name
        if not old_path:
            target = (
                target.with_suffix(".rs")
                if target.with_suffix(".rs").is_file()
                else target / "mod.rs"
            )
        if not ffi and name in {"origin", "ffi"}:
            target = directory / f"{name}.rs"
        elif not target.is_file():
            raise ValueError(f"Missing upstream Rust module: {name}")
        replacement = f"#[path = {literal(target)}]\n{declaration}"
        replacements.append((match.group(), replacement))
        return replacement

    result = re.sub(pattern, replace, data)
    expected = (
        {"form_urlencoded", "origin", "pattern", "url"}
        if ffi
        else {"rust_demangle", "rust_panic", "ffi", "origin", "pattern", "textcodec", "url"}
    )
    if modules != expected or len(replacements) != len(expected):
        raise ValueError("Review Rust liburl module layout")
    restored = result
    for original, replacement in replacements:
        restored = restored.replace(replacement, original)
    assert restored == data
    if ffi:
        return result
    return (
        result
        + f"\n// Owned build-input fingerprint: {provenance}\n#[path = {literal(ROOT / 'firefly/native/protocol/rust/origin_policy.rs')}]\nmod firefly_origin;\n"
    )


def prepare(output, driver, chain="", check=False):
    inputs = {str(SOURCE / name): expected for name, expected in PIN.items()}
    for filename, expected in inputs.items():
        if digest(Path(filename).read_bytes()) != expected:
            raise ValueError(f"Review Rust origin adapter after upstream update: {filename}")
    owned = [
        Path(__file__),
        ROOT / "tooling/rust-origin/driver.py",
        ROOT / "tooling/rust-origin/Driver.cpp",
        ROOT / "firefly/native/protocol/rust/origin_policy.rs",
    ]
    inputs.update({str(path.resolve()): digest(path.read_bytes()) for path in owned})
    provenance = digest(json.dumps({"inputs": inputs, "chain": chain}, sort_keys=True).encode())
    origin = origin_source((SOURCE / "origin.rs").read_text(encoding="utf-8"))
    for invalid in ["", (SOURCE / "origin.rs").read_text(encoding="utf-8") + SEAM]:
        try:
            origin_source(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Ambiguous Rust origin seam accepted")
    root = module_source((SOURCE / "lib.rs").read_text(encoding="utf-8"), output, provenance)
    planned = {
        "lib.rs": root.encode(),
        "origin.rs": origin.encode(),
        "ffi.rs": module_source(
            (SOURCE / "ffi.rs").read_text(encoding="utf-8"), output, provenance, ffi=True
        ).encode(),
    }
    config = {
        "schema": 1,
        "kind": "firefly-rust-origin-driver",
        "original": str(SOURCE / "lib.rs"),
        "adapted": str(output / "lib.rs"),
        "driver": str(driver),
        "chain": chain,
        "files": inputs | {str(output / name): digest(data) for name, data in planned.items()},
    }
    planned["config.json"] = (json.dumps(config, indent=2) + "\n").encode()
    header = "".join(
        f"#define {key} {json.dumps(value, ensure_ascii=False)}\n"
        for key, value in {
            "FIREFLY_DRIVER_PYTHON": sys.executable,
            "FIREFLY_DRIVER_SCRIPT": str(ROOT / "tooling/rust-origin/driver.py"),
            "FIREFLY_DRIVER_CONFIG": str(output / "config.json"),
        }.items()
    )
    planned["DriverConfig.h"] = header.encode()
    output.mkdir(parents=True, exist_ok=True)
    for name, data in planned.items():
        target = output / name
        if not target.exists() or target.read_bytes() != data:
            if check:
                raise ValueError(f"Generated Rust origin output drift: {target}")
            target.write_bytes(data)
    print("PASS pinned Rust origin/root composition, module ownership and exact source restoration")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    parser.add_argument("--driver", type=Path, required=True)
    parser.add_argument("--chain", default="")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    prepare(args.output.resolve(), args.driver.absolute(), args.chain, args.check)


if __name__ == "__main__":
    main()
