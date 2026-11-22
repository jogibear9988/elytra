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
            cache.write_text("LADYBIRD_BASELINE_CPU_TARGET:BOOL=ON\n")
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
            cache.write_text("LADYBIRD_BASELINE_CPU_TARGET:BOOL=OFF\n")
            with self.assertRaisesRegex(ValueError, "baseline is not enabled"):
                check(root)

    def test_retired_nojit_flags_rejected(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakeCache.txt").write_text("LADYBIRD_BASELINE_CPU_TARGET:BOOL=ON\n")
            (root / "compile_commands.json").write_text(
                json.dumps(
                    [{"file": "engine.cpp", "arguments": ["clang++", CPU_FLAG, "-c", "engine.cpp"]}]
                )
            )
            # Reused build trees may retain this retired generated file. It can
            # no longer authorize a source-selection cfg in a Cargo command.
            retired = root / "mite-adapters/rust-nojit/lib.rs"
            retired.parent.mkdir(parents=True)
            retired.write_bytes(b"// obsolete non-JIT source root\n")
            stale = "--cfg=mite_nojit_build_" + hashlib.sha256(retired.read_bytes()).hexdigest()
            prefix = f'COMMAND = cmake -E env "{RUST_ENV}='
            ninja = root / "build.ninja"
            flags = f"-Dwarnings {RUST_CPU_FLAG}"
            ninja.write_text(prefix + flags + '" cargo build\n')
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)
            for suffix in [stale, "-Adead_code", "--cfg=unrelated"]:
                ninja.write_text(prefix + flags + " " + suffix + '" cargo build\n')
                with self.assertRaisesRegex(ValueError, "Unexpected generated Cargo"):
                    check(root)


if __name__ == "__main__":
    unittest.main()
