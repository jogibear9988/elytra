#!/usr/bin/env python3
"""Refuse native/ambiguous/missing generated Linux CPU policies; these are not execution tests."""

import _support

import contextlib
import io
import json
from pathlib import Path
import tempfile
import unittest

from check_linux_cpu import check


class LinuxCpuPolicy(unittest.TestCase):
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
            cargo = 'COMMAND = cmake -E env "CARGO_TARGET_X86_64_UNKNOWN_LINUX_GNU_RUSTFLAGS=-Dwarnings -Ctarget-cpu=x86-64-v2" cargo build\n'
            ninja.write_text(cargo)
            with contextlib.redirect_stdout(io.StringIO()):
                check(root)
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
            for text in [cargo.replace("x86-64-v2", "native"), cargo.replace("-Dwarnings ", "")]:
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


if __name__ == "__main__":
    unittest.main()
