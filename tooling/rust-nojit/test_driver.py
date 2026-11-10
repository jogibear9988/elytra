#!/usr/bin/env python3
"""Exercise source routing/refusals and real native driver argv/exit transport."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from driver import command
import prepare as composition
from prepare import ROOT, SOURCE, SEAM, REPLACEMENT, item_source, module_source, prepare


def rejects(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Invalid compiler routing accepted")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="mite non-JIT ä ") as temporary:
        directory = Path(temporary)
        executable = directory / ("driver.exe" if os.name == "nt" else "driver")
        prepare(directory, executable)
        config = json.loads((directory / "config.json").read_text(encoding="utf-8"))
        rustc = str(directory / "real rustc")
        ordinary = ["--crate-name", "ordinary", "", "é", "value with spaces", '"', "end\\"]
        assert command(config, rustc, ordinary) == [rustc, *ordinary]
        source = str(SOURCE / "lib.rs")
        origin = ["--crate-name=libjs_rust", source, "--emit=dep-info,link"]
        assert command(config, rustc, origin) == [rustc, origin[0], config["adapted"], origin[2]]
        chained = config | {"chain": str(directory / "compiler cache")}
        assert command(chained, rustc, origin) == [
            chained["chain"],
            *command(config, rustc, origin),
        ]
        rejects(lambda: command(config, str(executable), ordinary))
        rejects(lambda: command(config | {"chain": str(executable)}, rustc, ordinary))
        rejects(lambda: command(config, rustc, ["--crate-name=libjs_rust"]))
        rejects(lambda: command(config, rustc, origin + [source]))
        rejects(lambda: command(config, rustc, origin + ["--crate-name=ordinary"]))
        generated = directory / "ecmascript_function_object.rs"
        generated.write_text("drift", encoding="utf-8")
        rejects(lambda: command(config, rustc, origin))
        rejects(lambda: prepare(directory, executable, check=True))
        prepare(directory, executable)
        prepare(directory, executable, check=True)

        pins = composition.PINS
        composition.PINS = pins | {"lib.rs": "0" * 64}
        try:
            rejects(lambda: prepare(directory, executable))
        finally:
            composition.PINS = pins
        original = (SOURCE / "runtime/ecmascript_function_object.rs").read_text()
        composed = item_source(original)
        assert composed.replace(REPLACEMENT, SEAM) == original
        rejects(lambda: item_source(original.replace(SEAM, "")))
        rejects(lambda: item_source(original + SEAM))
        rejects(lambda: item_source(composed))
        # Module paths select upstream originals except the two owned routing roots.
        root = (directory / "lib.rs").read_text()
        assert (SOURCE / "parser/mod.rs").as_posix() in root
        assert (directory / "runtime.rs").as_posix() in root
        assert (directory / "ecmascript_function_object.rs").as_posix() in (
            directory / "runtime.rs"
        ).read_text()
        rejects(lambda: module_source("mod missing;", SOURCE, {}))
        unicode = (directory / "unicode.rs").read_text()
        for name in (
            "calendar",
            "date_time_format",
            "display_names",
            "intl",
            "number_format",
            "relative_time_format",
            "time_zone",
        ):
            assert (SOURCE / "unicode" / f"{name}.rs").as_posix() in unicode
        rejects(lambda: module_source((SOURCE / "lib.rs").read_text(), SOURCE, {}))
        # Compile a real standalone Rust file-module composition: absolute path
        # routing must retain foo.rs -> foo/child.rs resolution, not base/child.rs.
        fixture = directory / "nested"
        fixture.mkdir()
        (fixture / "foo").mkdir()
        (fixture / "foo.rs").write_text("pub mod child;\npub fn value() -> u32 { child::VALUE }\n")
        (fixture / "foo/child.rs").write_text("pub const VALUE: u32 = 42;\n")
        generated = directory / "routed-foo.rs"
        generated.write_text(module_source((fixture / "foo.rs").read_text(), fixture / "foo", {}))
        root_fixture = directory / "routed-root.rs"
        root_fixture.write_text(
            module_source("pub mod foo;\n", fixture, {(fixture / "foo.rs").resolve(): generated})
        )
        subprocess.run(
            [
                "rustc",
                "--crate-name",
                "nested_routing",
                "--crate-type=lib",
                "--emit=metadata",
                str(root_fixture),
                "-o",
                str(directory / "nested.rmeta"),
            ],
            check=True,
            timeout=30,
        )

        # Compile the real native entry with a test-only Python argv observer. This
        # checks the platform process boundary (including Windows CRT quoting),
        # not a mock rustc acceptance claim. Production never uses this observer.
        observer = directory / "argv observer.py"
        observer.write_text(
            "import json,sys\nfrom pathlib import Path\n"
            'Path(sys.argv[1]).write_text(json.dumps(sys.argv[2:]), encoding="utf-8")\n'
            "raise SystemExit(37)\n",
            encoding="utf-8",
        )
        recorded = directory / "recorded.json"
        (directory / "DriverConfig.h").write_text(
            "".join(
                f"#define {key} {json.dumps(str(value), ensure_ascii=False)}\n"
                for key, value in {
                    "FIREFLY_DRIVER_PYTHON": sys.executable,
                    "FIREFLY_DRIVER_SCRIPT": observer,
                    "FIREFLY_DRIVER_CONFIG": recorded,
                }.items()
            ),
            encoding="utf-8",
        )
        compiler = Path(args.compiler).name.lower()
        if compiler in ("cl", "cl.exe", "clang-cl", "clang-cl.exe"):
            build = [
                args.compiler,
                "/nologo",
                "/std:c++17",
                "/EHsc",
                "/utf-8",
                f"/I{directory}",
                str(ROOT / "tooling/rust-nojit/Driver.cpp"),
                f"/Fe{executable}",
                f"/Fo{directory / 'driver.obj'}",
            ]
        else:
            build = [
                args.compiler,
                "-std=c++17",
                f"-I{directory}",
                str(ROOT / "tooling/rust-nojit/Driver.cpp"),
                "-o",
                str(executable),
            ]
        subprocess.run(build, check=True, cwd=directory)
        values = [
            "",
            "Grüße 世界",
            "space value",
            '"quoted"',
            'a\\"b',
            "end\\",
            "two\\\\",
            "$HOME;`x`",
        ]
        result = subprocess.run([str(executable), *values], timeout=15)
        assert result.returncode == 37, result
        assert json.loads(recorded.read_text(encoding="utf-8")) == values
        # Exercise the production Python routing through the real native entry and
        # a controlled cache process. The observer records exact downstream argv;
        # this is a process-boundary check, not a Rust compilation claim.
        prepare(directory, executable, chain=sys.executable)
        subprocess.run(build, check=True, cwd=directory)
        route_values = [str(recorded), "--crate-name=libjs_rust", source, "Grüße", ""]
        result = subprocess.run([str(executable), str(observer), *route_values], timeout=15)
        assert result.returncode == 37, result
        routed = json.loads(recorded.read_text(encoding="utf-8"))
        assert routed == [route_values[1], config["adapted"], *route_values[3:]], routed
        ordinary_values = [str(recorded), "--crate-name=ordinary", "", "value with spaces"]
        result = subprocess.run([str(executable), str(observer), *ordinary_values], timeout=15)
        assert result.returncode == 37, result
        assert json.loads(recorded.read_text(encoding="utf-8")) == ordinary_values[1:]
    print(
        "PASS checked non-JIT routing, chain order, drift/refusal and native Unicode/empty/quote argv + exit transport"
    )


if __name__ == "__main__":
    main()
