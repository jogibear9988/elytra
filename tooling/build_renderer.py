#!/usr/bin/env python3
"""Build the separate LibWebView/WebContent embedding probe, leaving Ladybird untouched."""

import argparse
from pathlib import Path
import subprocess
import os
import json
import platform
import sys

from check_upstream import check
from firefly_dependencies import prepare
from renderer_dependencies import prepare as prepare_linux_renderer
import compiler_cache

ROOT = Path(__file__).resolve().parents[1]


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--build-dir", type=Path, default=ROOT / "build/renderer")
    parser.add_argument("--jobs", type=int, default=4)
    parser.add_argument("--configure-only", action="store_true")
    parser.add_argument(
        "--ccache",
        action="store_true",
        help="Cache C/C++ objects; retain all native execution gates",
    )
    parser.add_argument(
        "--baseline-cpu",
        action="store_true",
        help="Use and verify the upstream Apple M1 baseline for macOS ARM64 distribution builds",
    )
    parser.add_argument(
        "--shared-openssl",
        action="store_true",
        help="Experimental macOS renderer graph with one pinned shared OpenSSL provider",
    )
    parser.add_argument(
        "--lto",
        action="store_true",
        help="Enable C++ Release LTO; fail if the compiler/linker cannot provide it",
    )
    parser.add_argument(
        "--optimize-size",
        action="store_true",
        help="Optimize project C++ Release objects for size; Rust and prebuilt dependencies keep their profiles",
    )
    parser.add_argument(
        "--target",
        choices=[
            "firefly-renderer-probe",
            "firefly-preload-probe",
            "firefly-window-probe",
            "firefly",
        ],
        default="firefly-renderer-probe",
    )
    args = parser.parse_args()
    if args.ccache and sys.platform not in ["darwin", "linux"]:
        parser.error("Compiler-cache acceptance currently requires a macOS/Linux Ninja build")
    try:
        cache_options = compiler_cache.arguments(args.ccache)
    except ValueError as error:
        parser.error(str(error))
    if args.shared_openssl and (
        sys.platform != "darwin" or platform.machine() != "arm64" or args.target != "firefly"
    ):
        parser.error("--shared-openssl currently requires native macOS ARM64 --target firefly")
    if args.baseline_cpu and (sys.platform != "darwin" or platform.machine() != "arm64"):
        parser.error("--baseline-cpu is currently verified only for macOS ARM64")
    if args.target == "firefly" and sys.platform not in ["darwin", "linux"]:
        parser.error(
            "The desktop runtime currently supports macOS and experimental Linux X11 builds"
        )
    if args.target == "firefly-window-probe" and sys.platform not in ["darwin", "linux"]:
        parser.error("The native window probe is available only for macOS/Linux")
    if args.target == "firefly-preload-probe" and sys.platform not in ["darwin", "linux"]:
        parser.error("The owned sandboxed helper is available only for macOS/Linux probes")
    if args.jobs < 1:
        parser.error("--jobs must be positive")
    check(ROOT)
    vcpkg = ROOT / "build/vcpkg"
    toolchain = vcpkg / "scripts/buildsystems/vcpkg.cmake"
    if not toolchain.is_file():
        parser.error("Run python3 tooling/bootstrap.py first")
    baseline = json.loads((ROOT / "vendor/ladybird/vcpkg.json").read_text())["builtin-baseline"]
    revision = subprocess.check_output(
        ["git", "-C", str(vcpkg), "rev-parse", "HEAD"], text=True
    ).strip()
    if revision != baseline:
        parser.error("vcpkg checkout does not match Ladybird baseline; run tooling/bootstrap.py")
    build = args.build_dir.resolve()
    build.mkdir(parents=True, exist_ok=True)
    command = [
        "cmake",
        "-S",
        str(ROOT / "vendor/ladybird"),
        "-B",
        str(build),
        "-G",
        "Ninja",
        "-DCMAKE_BUILD_TYPE=Release",
        f"-DCMAKE_TOOLCHAIN_FILE={toolchain}",
        f"-DCMAKE_PROJECT_ladybird_INCLUDE={ROOT}/firefly/renderer/Inject.cmake",
        f"-DFIREFLY_SOURCE_DIR={ROOT}",
        f"-DLADYBIRD_CACHE_DIR={ROOT}/build/renderer-cache",
        "-DBUILD_TESTING=OFF",
        "-DENABLE_JS_JIT=OFF",
        f"-DENABLE_LTO_FOR_RELEASE={'ON' if args.lto else 'OFF'}",
        "-DENABLE_INSTALL_HEADERS=OFF",
        "-DCMAKE_SKIP_INSTALL_RULES=ON",
    ]
    command.extend(cache_options)
    if args.target == "firefly":
        manifest = prepare(ROOT, build / "firefly-manifest", linux=sys.platform == "linux")
        command.extend([f"-DVCPKG_MANIFEST_DIR={manifest}", "-DFIREFLY_BUILD_RUNTIME=ON"])
    elif sys.platform == "linux":
        manifest = prepare_linux_renderer(ROOT, build / "renderer-manifest")
        command.extend([f"-DVCPKG_MANIFEST_DIR={manifest}", "-DFIREFLY_BUILD_RUNTIME=OFF"])
    else:
        command.extend(
            [f"-DVCPKG_MANIFEST_DIR={ROOT}/vendor/ladybird", "-DFIREFLY_BUILD_RUNTIME=OFF"]
        )
    command.append(f"-DFIREFLY_SHARED_OPENSSL={'ON' if args.shared_openssl else 'OFF'}")
    command.append(f"-DELYTRA_OPTIMIZE_SIZE={'ON' if args.optimize_size else 'OFF'}")
    command.append(f"-DENABLE_CI_BASELINE_CPU={'ON' if args.baseline_cpu else 'OFF'}")
    environment = {**os.environ, "VCPKG_MAX_CONCURRENCY": str(args.jobs)}
    subprocess.run(command, cwd=ROOT, env=environment, check=True)
    if args.ccache:
        compiler_cache.check(build, args.target)
    if args.baseline_cpu:
        subprocess.run(
            [sys.executable, str(ROOT / "tooling/check_macos_cpu.py"), "--build", str(build)],
            cwd=ROOT,
            check=True,
        )
    if args.optimize_size:
        subprocess.run(
            [
                sys.executable,
                str(ROOT / "tooling/check_size_optimization.py"),
                "--build",
                str(build),
            ],
            cwd=ROOT,
            check=True,
        )
    if not args.configure_only:
        subprocess.run(
            ["cmake", "--build", str(build), "--target", args.target, "--parallel", str(args.jobs)],
            cwd=ROOT,
            check=True,
        )
    check(ROOT)


if __name__ == "__main__":
    main()
