#!/usr/bin/env python3
"""Exercise source routing/refusals and real native driver argv/exit transport."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

from audit import audit
from driver import command
from prepare import ROOT, SOURCE, prepare


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
    with tempfile.TemporaryDirectory(prefix="firefly origin ä ") as temporary:
        directory = Path(temporary)
        executable = directory / ("driver.exe" if os.name == "nt" else "driver")
        prepare(directory, executable)
        config = json.loads((directory / "config.json").read_text(encoding="utf-8"))
        rustc = str(directory / "real rustc")
        ordinary = ["--crate-name", "ordinary", "", "é", "value with spaces", '"', "end\\"]
        assert command(config, rustc, ordinary) == [rustc, *ordinary]
        source = str(SOURCE / "lib.rs")
        origin = ["--crate-name=liburl_rust", source, "--emit=dep-info,link"]
        assert command(config, rustc, origin) == [rustc, origin[0], config["adapted"], origin[2]]
        chained = config | {"chain": str(directory / "compiler cache")}
        assert command(chained, rustc, origin) == [
            chained["chain"],
            *command(config, rustc, origin),
        ]
        rejects(lambda: command(config, str(executable), ordinary))
        rejects(lambda: command(config | {"chain": str(executable)}, rustc, ordinary))
        rejects(lambda: command(config, rustc, ["--crate-name=liburl_rust"]))
        rejects(lambda: command(config, rustc, origin + [source]))
        rejects(lambda: command(config, rustc, origin + ["--crate-name=ordinary"]))
        generated = directory / "origin.rs"
        generated.write_text("drift", encoding="utf-8")
        rejects(lambda: command(config, rustc, origin))
        rejects(lambda: prepare(directory, executable, check=True))
        prepare(directory, executable)
        prepare(directory, executable, check=True)

        # A missing composition in emitted dependencies must fail even if Cargo
        # itself reports an otherwise successful/fresh archive.
        depfile = directory / "archive.d"
        paths = [directory / name for name in ("lib.rs", "origin.rs", "ffi.rs")]
        paths += [ROOT / "firefly/native/protocol/rust/origin_policy.rs"]
        depfile.write_text(
            "archive: " + " ".join(str(path).replace(" ", "\\ ") for path in paths),
            encoding="utf-8",
        )
        audit(directory / "config.json", [depfile])
        depfile.write_text("archive: upstream-only.rs", encoding="utf-8")
        try:
            audit(directory / "config.json", [depfile])
        except AssertionError:
            pass
        else:
            raise AssertionError("Stale upstream-only Rust archive accepted")

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
                str(ROOT / "tooling/rust-origin/Driver.cpp"),
                f"/Fe{executable}",
                f"/Fo{directory / 'driver.obj'}",
            ]
        else:
            build = [
                args.compiler,
                "-std=c++17",
                f"-I{directory}",
                str(ROOT / "tooling/rust-origin/Driver.cpp"),
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
    print(
        "PASS checked origin routing, chain order, drift/refusal and native Unicode/empty/quote argv + exit transport"
    )


if __name__ == "__main__":
    main()
