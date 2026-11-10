#!/usr/bin/env python3
"""Keep generated M1/warnings policy strict while accepting exactly the owned origin fingerprint."""

import _support

import contextlib
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from check_macos_cpu import check


CPU_FLAG = "-mcpu=apple-m1"
RUST_CPU_FLAG = "-Ctarget-cpu=apple-m1"
RUST_ENV = "CARGO_TARGET_AARCH64_APPLE_DARWIN_RUSTFLAGS"


class MacCpuPolicy(unittest.TestCase):
    def test_generated_commands(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakeCache.txt").write_text("ENABLE_CI_BASELINE_CPU:BOOL=ON\n")
            command = {
                "file": "engine.cpp",
                "arguments": ["clang++", "-mcpu=apple-m1", "-c", "engine.cpp"],
            }
            (root / "compile_commands.json").write_text(json.dumps([command]))
            cargo = 'COMMAND = cmake -E env "CARGO_TARGET_AARCH64_APPLE_DARWIN_RUSTFLAGS=-Dwarnings -Ctarget-cpu=apple-m1" cargo build\n'
            ninja = root / "build.ninja"
            ninja.write_text(cargo)
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)
            origin = root / "firefly-adapters/rust-origin/lib.rs"
            origin.parent.mkdir(parents=True)
            origin.write_bytes(b"// composed root fixture\n")
            digest = hashlib.sha256(origin.read_bytes()).hexdigest()
            option = f" --cfg=firefly_origin_build_{digest}"
            composed = cargo.replace('" cargo build', option + '" cargo build')
            ninja.write_text(composed)
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)
            for invalid in [
                cargo,
                composed.replace(digest, "0" * 64),
                composed.replace(option, option + option),
                composed.replace(option, option + " --cfg=unrelated"),
                composed.replace("apple-m1", "native"),
                composed.replace("-Dwarnings ", ""),
            ]:
                ninja.write_text(invalid)
                with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                    check(root)
            ninja.write_text(composed)
            origin.write_bytes(b"// changed composition\n")
            with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                check(root)
            ninja.write_text("")
            with self.assertRaisesRegex(ValueError, "Missing generated baseline"):
                check(root)
            ninja.write_text(composed)
            for flags in [["-mcpu=native"], [], ["-mcpu=apple-m1", "-march=native"]]:
                command["arguments"] = ["clang++", *flags, "-c", "engine.cpp"]
                (root / "compile_commands.json").write_text(json.dumps([command]))
                with self.assertRaisesRegex(ValueError, "Unexpected project CPU"):
                    check(root)

    def test_generated_nojit_and_origin_compositions(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakeCache.txt").write_text("ENABLE_CI_BASELINE_CPU:BOOL=ON\n")
            (root / "compile_commands.json").write_text(
                json.dumps(
                    [
                        {
                            "file": "engine.cpp",
                            "arguments": ["clang++", CPU_FLAG, "-c", "engine.cpp"],
                        }
                    ]
                )
            )
            ninja = root / "build.ninja"
            nojit = root / "mite-adapters/rust-nojit/lib.rs"
            nojit.parent.mkdir(parents=True)
            nojit.write_bytes(b"// generated non-JIT source root\n")
            nojit_hash = hashlib.sha256(nojit.read_bytes()).hexdigest()
            nojit_flag = f"--cfg=mite_nojit_build_{nojit_hash}"
            origin = root / "firefly-adapters/rust-origin/lib.rs"
            origin_flag = ""
            for combined in (False, True):
                if combined:
                    origin.parent.mkdir(parents=True)
                    origin.write_bytes(b"// generated renderer origin source root\n")
                    origin_flag = (
                        "--cfg=firefly_origin_build_"
                        + hashlib.sha256(origin.read_bytes()).hexdigest()
                    )
                flags = [
                    "-Dwarnings",
                    RUST_CPU_FLAG,
                    *([origin_flag] if combined else []),
                    nojit_flag,
                ]

                def command(values):
                    return (
                        'COMMAND = cmake -E env "'
                        + RUST_ENV
                        + "="
                        + " ".join(values)
                        + '" cargo build\n'
                    )

                ninja.write_text(command(flags))
                with contextlib.redirect_stdout(io.StringIO()):
                    check(root)
                invalid = [
                    flags[:-1],
                    [*flags[:-1], nojit_flag.replace(nojit_hash, "0" * 64)],
                    [*flags, nojit_flag],
                    [*flags, "--cfg=unrelated"],
                ]
                if combined:
                    invalid.extend(
                        [
                            ["-Dwarnings", RUST_CPU_FLAG, nojit_flag, origin_flag],
                            ["-Dwarnings", RUST_CPU_FLAG, nojit_flag],
                        ]
                    )
                for values in invalid:
                    ninja.write_text(command(values))
                    with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                        check(root)
                ninja.write_text(command(flags))
                before = nojit.read_bytes()
                nojit.write_bytes(before + b"// changed root\n")
                with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                    check(root)
                nojit.write_bytes(before)
                nojit.unlink()
                with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                    check(root)
                nojit.write_bytes(before)
                if combined:
                    original_origin = origin.read_bytes()
                    origin.write_bytes(original_origin + b"// changed root\n")
                    with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                        check(root)
                    origin.write_bytes(original_origin)
                with contextlib.redirect_stdout(io.StringIO()):
                    check(root)


if __name__ == "__main__":
    unittest.main()
