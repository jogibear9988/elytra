#!/usr/bin/env python3
"""Select the owned control-message generator at pinned CMake codegen seams only."""

import argparse
import hashlib
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXPECTED = "014b41001599bf8ead631b5672ab35f699c8b8c24e8d09f80af1b5a932a024f8"
SEAM = "    find_package(Python3 REQUIRED COMPONENTS Interpreter)\n    add_custom_command("
INSERT = """    set(firefly_ipc_generator "${LADYBIRD_SOURCE_DIR}/Meta/Generators/generate_ipc_definitions.py")
    set(firefly_ipc_dependency)
    if(source STREQUAL "${LADYBIRD_SOURCE_DIR}/Services/WebContent/WebContentServer.ipc")
        set(firefly_ipc_generator "${FIREFLY_SOURCE_DIR}/tooling/generate_byte_bootstrap.py")
        set(firefly_ipc_dependency "${FIREFLY_SOURCE_DIR}/tooling/generate_byte_bootstrap.py")
        set_property(GLOBAL PROPERTY FIREFLY_BYTE_GENERATOR_SELECTED TRUE)
    endif()
"""
COMMAND = (
    '"${Python3_EXECUTABLE}" "${LADYBIRD_SOURCE_DIR}/Meta/Generators/generate_ipc_definitions.py"'
)
DEPEND = '        DEPENDS "${LADYBIRD_SOURCE_DIR}/Meta/Generators/generate_ipc_definitions.py"'


def adapt(source):
    if source.count(SEAM) != 1 or source.count(COMMAND) != 1 or source.count(DEPEND) != 1:
        raise ValueError("Review pinned IPC CMake generator seams")
    result = source.replace(SEAM, INSERT + SEAM).replace(
        COMMAND, '"${Python3_EXECUTABLE}" "${firefly_ipc_generator}"'
    )
    return result.replace(DEPEND, DEPEND + " ${firefly_ipc_dependency}")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, required=True)
    args = parser.parse_args()
    source = ROOT / "vendor/ladybird/Meta/CMake/code_generators.cmake"
    raw = source.read_bytes()
    if hashlib.sha256(raw).hexdigest() != EXPECTED:
        raise SystemExit("Review byte bootstrap CMake adapter after Ladybird update")
    original = raw.decode()
    result = adapt(original)
    restored = (
        result.replace(INSERT, "")
        .replace('"${Python3_EXECUTABLE}" "${firefly_ipc_generator}"', COMMAND)
        .replace(DEPEND + " ${firefly_ipc_dependency}", DEPEND)
    )
    assert restored == original
    for invalid in ("", original + SEAM):
        try:
            adapt(invalid)
        except ValueError:
            pass
        else:
            raise AssertionError("Ambiguous CMake generator seam accepted")
    args.output.parent.mkdir(parents=True, exist_ok=True)
    if not args.output.exists() or args.output.read_text() != result:
        args.output.write_text(result)
    print("PASS owned byte bootstrap codegen seam and exact upstream byte preservation")


if __name__ == "__main__":
    main()
