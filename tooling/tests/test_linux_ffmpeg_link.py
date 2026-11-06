#!/usr/bin/env python3
"""Exercise actual ELF archive relocation/export behavior with the owned linker adapter."""

import argparse
from pathlib import Path
import subprocess
import sys
import tempfile

ROOT = Path(__file__).resolve().parents[2]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.parse_args()
    if sys.platform != "linux":
        parser.error("This is a native Linux ELF gate, not a simulated platform test")
    with tempfile.TemporaryDirectory(prefix="firefly-ffmpeg-link-") as directory:
        source = Path(directory)
        (source / "table.S").write_text("""
.section .data
.globl ff_private_table
.type ff_private_table,@object
ff_private_table:
.long 42
.text
.globl ff_private_decoder
.type ff_private_decoder,@function
ff_private_decoder:
lea ff_private_table(%rip), %rax
ret
.section .note.GNU-stack,"",@progbits
""")
        (source / "public.c").write_text("""
extern int *ff_private_decoder(void);
int firefly_public_media(void) { return *ff_private_decoder(); }
""")
        (source / "main.c").write_text("""
extern int firefly_public_media(void);
int main(void) { return firefly_public_media() == 42 ? 0 : 1; }
""")
        adapter = (ROOT / "firefly/renderer/LinuxFFmpeg.cmake").as_posix()
        (source / "CMakeLists.txt").write_text(f'''
cmake_minimum_required(VERSION 3.30)
project(ArchiveVisibility LANGUAGES C ASM)
add_library(codec STATIC table.S)
set_target_properties(codec PROPERTIES OUTPUT_NAME avutil-ladybird)
add_library(control SHARED public.c)
target_link_libraries(control PRIVATE codec)
add_library(owned SHARED public.c)
target_link_libraries(owned PRIVATE codec)
include("{adapter}")
firefly_hide_bundled_ffmpeg(owned)
add_executable(acceptance main.c)
target_link_libraries(acceptance PRIVATE owned)
''')
        build = source / "build"
        subprocess.run(["cmake", "-S", str(source), "-B", str(build), "-G", "Ninja"], check=True)
        control = subprocess.run(
            ["cmake", "--build", str(build), "--target", "control"],
            capture_output=True,
            text=True,
        )
        assert control.returncode and "R_X86_64_PC32" in control.stdout + control.stderr, control
        subprocess.run(["cmake", "--build", str(build), "--target", "acceptance"], check=True)
        subprocess.run([str(build / "acceptance")], check=True)
        symbols = subprocess.check_output(
            ["readelf", "--dyn-syms", "--wide", str(build / "libowned.so")], text=True
        )
        assert "firefly_public_media" in symbols, symbols
        assert "ff_private_table" not in symbols and "ff_private_decoder" not in symbols, symbols
        print(
            "PASS real failing PC32 control, private archive link, callable public API and hidden internals"
        )


if __name__ == "__main__":
    main()
