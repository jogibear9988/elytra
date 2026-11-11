#!/usr/bin/env python3
"""Configure a temporary injected SDK project to verify the real CMake header graph."""

from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
POLICY = ROOT / "firefly/native/qt/hub/HeaderPolicy.cmake"


def configure(case, extra="", expected_error=None):
    with tempfile.TemporaryDirectory(prefix="elytra-hub-cmake-headers-") as temporary:
        root = Path(temporary)
        vendor, build, overlay = root / "vendor", root / "build", root / "overlay"
        qt = vendor / "UI/Qt"
        qt.mkdir(parents=True)
        overlay.mkdir()
        (vendor / "unit.cpp").write_text("int embedded_header_fixture;\n")
        (qt / "CMakeLists.txt").write_text(
            'add_library(ladybird_impl STATIC "${CMAKE_SOURCE_DIR}/unit.cpp")\n'
            'target_include_directories(ladybird_impl PRIVATE "${CMAKE_SOURCE_DIR}")\n'
        )
        (vendor / "CMakeLists.txt").write_text(
            f'''cmake_minimum_required(VERSION 3.25)
project(HubHeaderFixture LANGUAGES CXX)
set(LADYBIRD_SOURCE_DIR "${{CMAKE_SOURCE_DIR}}")
set(FIREFLY_SOURCE_DIR "{ROOT.as_posix()}")
set(hub_frontend_output "{overlay.as_posix()}")
# These framework targets are injected into the SDK root directory, exactly as
# in production. SOURCE_DIR cannot distinguish them from SDK targets.
add_library(firefly-qt-view STATIC unit.cpp)
target_include_directories(firefly-qt-view PRIVATE "${{CMAKE_SOURCE_DIR}}")
add_library(embedded::frontend ALIAS firefly-qt-view)
add_library(bridge-layer INTERFACE)
target_link_libraries(bridge-layer INTERFACE "$<BUILD_INTERFACE:embedded::frontend>")
add_library(firefly-application OBJECT unit.cpp)
target_include_directories(firefly-application PRIVATE "${{CMAKE_SOURCE_DIR}}")
target_link_libraries(firefly-application PRIVATE bridge-layer)
add_library(unrelated-sdk INTERFACE)
target_link_libraries(unrelated-sdk INTERFACE "$<$<BOOL:1>:some-system-library>")
add_subdirectory(UI/Qt)
include("{POLICY.as_posix()}")
# Declared after the policy include: deferred traversal must still discover them.
add_library(object-consumer STATIC unit.cpp "$<TARGET_OBJECTS:firefly-application>")
target_include_directories(object-consumer PRIVATE "${{CMAKE_SOURCE_DIR}}")
add_library(late-consumer STATIC unit.cpp)
target_include_directories(late-consumer PRIVATE "${{CMAKE_SOURCE_DIR}}")
target_link_libraries(late-consumer PRIVATE object-consumer)
{extra}
function(assert_policy)
    foreach(target firefly-qt-view firefly-application object-consumer late-consumer)
        get_target_property(source "${{target}}" SOURCE_DIR)
        if(NOT source STREQUAL CMAKE_SOURCE_DIR)
            message(FATAL_ERROR "Fixture no longer reproduces injected source directory")
        endif()
        get_target_property(paths "${{target}}" INCLUDE_DIRECTORIES)
        list(GET paths 0 first)
        if(NOT first STREQUAL hub_frontend_output)
            message(FATAL_ERROR "Missing first embedded header on ${{target}}: ${{paths}}")
        endif()
    endforeach()
    get_target_property(paths ladybird_impl INCLUDE_DIRECTORIES)
    if(hub_frontend_output IN_LIST paths)
        message(FATAL_ERROR "Standalone SDK acquired embedded class layout")
    endif()
    file(WRITE "${{CMAKE_BINARY_DIR}}/verified" "actual injected graph verified")
endfunction()
cmake_language(DEFER CALL assert_policy)
'''
        )
        result = subprocess.run(
            [
                shutil.which("cmake") or "cmake",
                "-S",
                str(vendor),
                "-B",
                str(build),
                "-G",
                "Unix Makefiles",
                "-DCMAKE_EXPORT_COMPILE_COMMANDS=OFF",
            ],
            text=True,
            capture_output=True,
            timeout=90,
        )
        output = result.stdout + result.stderr
        if expected_error is None:
            assert result.returncode == 0, output
            assert (build / "verified").is_file(), output
        else:
            assert result.returncode != 0 and expected_error in output, output
        print("PASS Hub CMake header policy " + case)


if __name__ == "__main__":
    configure("injected source-dir, aliases, late and object-source consumers")
    configure(
        "hidden conditional dependency rejected",
        "add_library(hidden STATIC unit.cpp)\n"
        'target_link_libraries(hidden PRIVATE "$<$<BOOL:1>:firefly-application>")',
        "Review Hub header dependency expression",
    )
    configure(
        "hidden object-source dependency rejected",
        'add_library(hidden STATIC unit.cpp "$<$<BOOL:1>:$<TARGET_OBJECTS:firefly-application>>")',
        "Review Hub header dependency expression",
    )
    configure(
        "standalone SDK dependency rejected",
        "target_link_libraries(ladybird_impl PRIVATE firefly-qt-view)",
        "Standalone Ladybird must not consume",
    )
