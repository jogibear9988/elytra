#!/usr/bin/env python3
"""Refuse native/ambiguous/missing generated Linux CPU policies; these are not execution tests."""

import _support  # noqa: F401 — adds the shared tooling directory for standalone execution.

import contextlib
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from check_linux_cpu import check
from _cpu_autoplay import check_autoplay_compositions


CPU_FLAG = "-march=x86-64-v2"
RUST_CPU_FLAG = "-Ctarget-cpu=x86-64-v2"
RUST_ENV = "CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_RUSTFLAGS"


class LinuxCpuPolicy(unittest.TestCase):
    def test_generated_autoplay_composition(self):
        check_autoplay_compositions(self, check, CPU_FLAG, RUST_CPU_FLAG, RUST_ENV)

    def test_generated_policy_and_refusals(self):
        with tempfile.TemporaryDirectory() as directory:
            root = Path(directory)
            cache = root / "CMakeCache.txt"
            commands = root / "compile_commands.json"
            ninja = root / "build.ninja"
            cache.write_text("ENABLE_CI_BASELINE_CPU:BOOL=ON\n")
            records = [
                {
                    "file": "engine.cpp",
                    "arguments": ["clang++", "-march=x86-64-v2", "-c", "engine.cpp"],
                }
            ]
            commands.write_text(json.dumps(records))
            cargo = 'COMMAND = cmake -E env "CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_RUSTFLAGS=-Dwarnings -Ctarget-cpu=x86-64-v2 -Cforce-frame-pointers=yes" cargo build\n'
            ninja.write_text(cargo)
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)
                ninja.write_text(cargo.replace(" -Cforce-frame-pointers=yes", ""))
                check(root)
            origin = root / "firefly-adapters/rust-origin/lib.rs"
            origin.parent.mkdir(parents=True)
            origin.write_bytes(b"// generated fixture input\n")
            fingerprint = hashlib.sha256(origin.read_bytes()).hexdigest()
            option = f" --cfg=firefly_origin_build_{fingerprint}"
            configured = cargo.replace('" cargo build', option + '" cargo build')
            ninja.write_text(configured)
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)
            for invalid in [
                cargo,
                configured.replace(fingerprint, "0" * 64),
                configured.replace(option, option + option),
                configured.replace(option, option + " --cfg=unrelated"),
                configured.replace("x86-64-v2", "native"),
                configured.replace("-Dwarnings ", ""),
            ]:
                ninja.write_text(invalid)
                with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                    check(root)
            origin.write_bytes(b"// changed composition\n")
            ninja.write_text(configured)
            with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                check(root)
            origin.unlink()
            ninja.write_text(cargo)
            for arguments in [
                ["-march=native"],
                [],
                ["-march=x86-64-v2", "-mcpu=native"],
                ["-march=x86-64-v3"],
            ]:
                records[0]["arguments"] = ["clang++", *arguments, "-c", "engine.cpp"]
                commands.write_text(json.dumps(records))
                with self.assertRaisesRegex(ValueError, "Unexpected project CPU"):
                    check(root)
            records[0]["arguments"] = ["clang++", "-march=x86-64-v2", "-c", "engine.cpp"]
            commands.write_text(json.dumps(records))
            for text in [
                cargo.replace("x86-64-v2", "native"),
                cargo.replace("-Dwarnings ", ""),
                cargo.replace("-Cforce-frame-pointers=yes", "-Ctarget-cpu=native"),
                cargo.replace("-Cforce-frame-pointers=yes", "-Cforce-frame-pointers=no"),
            ]:
                ninja.write_text(text)
                with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                    check(root)
            ninja.write_text("")
            with self.assertRaisesRegex(ValueError, "Missing generated baseline"):
                check(root)
            ninja.write_text(cargo)
            cache.write_text("ENABLE_CI_BASELINE_CPU:BOOL=OFF\n")
            with self.assertRaisesRegex(ValueError, "baseline is not enabled"):
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
                    # The actual pinned Linux backend also emits frame pointers.
                    ninja.write_text(
                        command([*flags[:2], "-Cforce-frame-pointers=yes", *flags[2:]])
                    )
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
