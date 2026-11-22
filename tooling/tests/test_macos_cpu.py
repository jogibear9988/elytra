#!/usr/bin/env python3
"""Keep generated M1/warnings policy strict while accepting exactly the owned origin fingerprint."""

import _support  # noqa: F401 — adds the shared tooling directory for standalone execution.

import contextlib
import hashlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from check_macos_cpu import check
from _cpu_autoplay import check_autoplay_compositions


CPU_FLAG = "-mcpu=apple-m1"
RUST_CPU_FLAG = "-Ctarget-cpu=apple-m1"
RUST_ENV = "CARGO_TARGET_AARCH64_APPLE_DARWIN_RUSTFLAGS"


class MacCpuPolicy(unittest.TestCase):
    def test_generated_autoplay_composition(self):
        check_autoplay_compositions(self, check, CPU_FLAG, RUST_CPU_FLAG, RUST_ENV)

    def test_generated_commands(self):
        with tempfile.TemporaryDirectory() as temporary:
            root = Path(temporary)
            (root / "CMakeCache.txt").write_text("LADYBIRD_BASELINE_CPU_TARGET:BOOL=ON\n")
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
