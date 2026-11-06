#!/usr/bin/env python3
"""Build real binaries to prove cache hits and header/source/flag invalidation; no GUI is used."""

import _support

import argparse
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

import compiler_cache
from check_upstream import check

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    program = shutil.which("ccache")
    if not program:
        parser.error("Install ccache to run its actual native acceptance gate")
    check(ROOT)
    with tempfile.TemporaryDirectory(prefix="elytra-compiler-cache-") as directory:
        root = Path(directory)
        build = root / "build"
        env = {
            **os.environ,
            "CCACHE_DIR": str(root / "cache"),
            "CCACHE_CONFIGPATH": str(root / "ccache.conf"),
            "CCACHE_COMPILERCHECK": "content",
            "CCACHE_SLOPPINESS": "",
        }
        (root / "ccache.conf").write_text(
            "compiler_check = content\nsloppiness =\nmax_size = 64M\n"
        )
        (root / "value.h").write_text("#define VALUE 1\n")
        (root / "helper.c").write_text("int helper(void) { return 5; }\n")
        (root / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.30)
project(CacheAcceptance LANGUAGES C CXX)
if(ENABLE_LAGOM_CCACHE)
    include("{(ROOT / "vendor/ladybird/Meta/CMake/setup_ccache.cmake").as_posix()}")
endif()
set(PROBE_FLAG 2 CACHE STRING "Actual compiler flag under test")
add_executable(probe main.cpp helper.c)
target_compile_definitions(probe PRIVATE PROBE_FLAG=${{PROBE_FLAG}})
if(BYPASS_CACHE)
    set_property(TARGET probe PROPERTY CXX_COMPILER_LAUNCHER "")
endif()
''')

        def source(value):
            (root / "main.cpp").write_text(f"""
#include "value.h"
#include <stdio.h>
extern "C" int helper(void);
int main() {{ printf("%d\\n", VALUE + PROBE_FLAG + helper() + {value}); }}
""")

        def run(*args):
            return subprocess.run(
                list(map(str, args)), env=env, capture_output=True, text=True, check=True
            )

        def configure(enabled=True, flag=2, bypass=False):
            run(
                "cmake",
                "-S",
                root,
                "-B",
                build,
                "-G",
                "Ninja",
                "-DCMAKE_BUILD_TYPE=Release",
                f"-DPROBE_FLAG={flag}",
                f"-DBYPASS_CACHE={'ON' if bypass else 'OFF'}",
                *compiler_cache.arguments(enabled),
            )

        def compile_and_execute(expected):
            run("cmake", "--build", build, "--target", "clean")
            run("cmake", "--build", build, "--target", "probe")
            assert run(build / "probe").stdout == f"{expected}\n", (
                "Stale native object was executed"
            )

        def stats():
            # Machine-readable tab counters work across the CI's ccache 4.x tools.
            return {
                key: int(value)
                for key, value in (
                    line.split() for line in run(program, "--print-stats").stdout.splitlines()
                )
            }

        source(10)
        configure()
        compiler_cache.check(build, "probe")
        compile_and_execute(18)
        cold = stats()
        compile_and_execute(18)
        hot = stats()
        hits = lambda value: value["direct_cache_hit"] + value["preprocessed_cache_hit"]
        assert hits(hot) >= hits(cold) + 2, (cold, hot)
        (root / "value.h").write_text("#define VALUE 4\n")
        compile_and_execute(21)
        source(20)
        compile_and_execute(31)
        configure(flag=3)
        compile_and_execute(32)
        configured = stats()
        configure(enabled=False, flag=3)
        try:
            compiler_cache.check(build, "probe")
        except ValueError:
            pass
        else:
            raise AssertionError("Disabled cache was advertised as enabled")
        compile_and_execute(32)
        assert stats()["cache_miss"] == configured["cache_miss"], (
            "Opt-out retained a cached launcher"
        )
        assert hits(stats()) == hits(configured), "Opt-out retained a cached launcher"
        configure(flag=3, bypass=True)
        try:
            compiler_cache.check(build, "probe")
        except ValueError as error:
            assert "bypasses" in str(error), error
        else:
            raise AssertionError("A target bypassed ccache despite configured global launchers")
        print(
            "PASS actual C/C++ hits, executed header/source/flag changes, opt-out and bypass refusal"
        )


if __name__ == "__main__":
    main()
