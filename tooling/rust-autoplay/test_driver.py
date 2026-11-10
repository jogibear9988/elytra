#!/usr/bin/env python3
"""Check exact Rust routing and actual native argv transport; no WebAudio support claim."""

import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile

import prepare as composition
from driver import CRATE, command
from audio import PINS as AUDIO_PINS, compose
from prepare import (
    AUDIO_MODULES,
    ROOT,
    ROOT_MODULES,
    SOURCE,
    input_fingerprint,
    module_source,
    prepare,
)


def rejects(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Unreviewed WebAudio source selection accepted")


def compile_driver(compiler, directory, executable):
    native = ROOT / "tooling/rust-autoplay/Driver.cpp"
    if Path(compiler).name.lower() in ("cl", "cl.exe", "clang-cl", "clang-cl.exe"):
        arguments = [
            compiler,
            "/nologo",
            "/std:c++17",
            "/EHsc",
            "/utf-8",
            f"/I{directory}",
            str(native),
            f"/Fe{executable}",
            f"/Fo{directory / 'driver.obj'}",
        ]
    else:
        arguments = [compiler, "-std=c++17", f"-I{directory}", str(native), "-o", str(executable)]
    subprocess.run(arguments, cwd=directory, check=True, timeout=60)


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--compiler", required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly Rust WebAudio ä # ") as temporary:
        directory = Path(temporary).resolve()
        executable = directory / ("driver.exe" if os.name == "nt" else "driver")
        prepare(directory, executable)
        config = json.loads((directory / "config.json").read_text())
        rustc = str(directory / "real rustc")
        source = str((SOURCE / "lib.rs").resolve())
        ordinary = ["--crate-name", "ordinary", "", "Grüße 世界", "space value", 'a\\"b', "end\\"]
        assert command(config, rustc, ordinary) == [rustc, *ordinary]
        for name in (["--crate-name", CRATE], [f"--crate-name={CRATE}"]):
            inputs = [*name, source, "--emit=dep-info,link", "", "Grüße"]
            expected = [rustc, *name, config["adapted"], *inputs[len(name) + 1 :]]
            assert command(config, rustc, inputs) == expected
            cache = str(directory / "existing nojit origin cache chain")
            assert command(config | {"chain": cache}, rustc, inputs) == [cache, *expected]
        selected = [f"--crate-name={CRATE}", source, "--emit=metadata"]
        for invalid in (
            [f"--crate-name={CRATE}"],
            [f"--crate-name={CRATE}", source, source],
            ["--crate-name=other", source],
            ["--crate-name=other", config["adapted"]],
            [source],
            [f"--crate-name={CRATE}", config["adapted"]],
            ["--crate-name"],
            ["--crate-name="],
            [*selected, "--crate-name=other"],
        ):
            rejects(lambda: command(config, rustc, invalid))
        rejects(lambda: command(config, str(executable), ordinary))
        rejects(lambda: command(config | {"chain": str(executable)}, rustc, ordinary))
        rejects(lambda: command(config | {"schema": 2}, rustc, ordinary))
        rejects(lambda: command(config | {"extra": True}, rustc, ordinary))

        # Every non-routing edit is separately pinned and byte-reversed by the
        # real leaf composer; source-start has two independent upstream paths.
        for name in AUDIO_PINS:
            original = (SOURCE / "web_audio" / name).read_text()
            assert (directory / name).read_text() == compose(name, original)
            rejects(lambda: compose(name, original + "// drift"))
            rejects(lambda: compose(name, compose(name, original)))
        root = (directory / "lib.rs").read_text()
        audio = (directory / "web_audio.rs").read_text()
        assert str(directory / "web_audio.rs").replace("\\", "/") in root
        assert str(directory / "audio_context.rs").replace("\\", "/") in audio
        for untouched in ("base_audio_context", "offline_audio_context", "oscillator_node"):
            assert str(SOURCE / "web_audio" / (untouched + ".rs")).replace("\\", "/") in audio
        assert root.count("#[path = ") == len(ROOT_MODULES)
        assert audio.count("#[path = ") == len(AUDIO_MODULES)
        # Independently reconstruct the actual footer commitment from all three
        # generated source bodies, not merely the wrapper's input file list.
        root_body, marker = (
            (directory / "lib.rs").read_bytes().split(("\n" + composition.FOOTER).encode())
        )
        paths = {str(directory / name) for name in ("lib.rs", "web_audio.rs", *AUDIO_PINS)}
        source_inputs = {
            name: value for name, value in config["files"].items() if name not in paths
        }
        assert marker.strip().decode() == input_fingerprint(
            source_inputs,
            "",
            {
                "lib.rs": root_body,
                "web_audio.rs": audio.encode(),
                **{name: (directory / name).read_bytes() for name in AUDIO_PINS},
            },
        )
        for name in ("lib.rs", "web_audio/mod.rs"):
            base = SOURCE if name == "lib.rs" else SOURCE / "web_audio"
            expected = ROOT_MODULES if name == "lib.rs" else AUDIO_MODULES
            original = (SOURCE / name).read_text()
            first = next(iter(expected))
            rejects(lambda: module_source(original + f"\nmod {first};\n", base, expected, {}))
            rejects(lambda: module_source(original + "\nmod unreviewed;\n", base, expected, {}))
            rejects(lambda: module_source(original, base, expected | {"absent"}, {}))
        missing = composition.MODULE.sub("", (SOURCE / "lib.rs").read_text(), count=1)
        rejects(lambda: module_source(missing, SOURCE, ROOT_MODULES, {}))
        rejects(lambda: module_source(root, SOURCE, ROOT_MODULES, {}))
        rejects(lambda: module_source("mod missing;\n", SOURCE, {"missing"}, {}))
        rejects(
            lambda: module_source(
                "mod ak_format;\n",
                SOURCE,
                {"ak_format"},
                {SOURCE / "absent.rs": directory / "absent.rs"},
            )
        )

        # Every planned source, including the future modified leaf, contributes
        # to Cargo's root freshness marker. A wrapper/config change alone is not
        # relied on to invalidate Cargo artifacts.
        planned = {
            name: (directory / name).read_bytes()
            for name in ("lib.rs", "web_audio.rs", *AUDIO_PINS)
        }
        before = input_fingerprint({"checked": "input"}, "chain", planned)
        for name in planned:
            mutated = planned | {name: planned[name] + b"\n// changed generated bytes\n"}
            assert input_fingerprint({"checked": "input"}, "chain", mutated) != before
        for name in ("lib.rs", "web_audio.rs", *AUDIO_PINS):
            target = directory / name
            original = target.read_bytes()
            target.write_bytes(original + b"\n// drift\n")
            rejects(lambda: command(config, rustc, selected))
            rejects(lambda: prepare(directory, executable, check=True))
            target.write_bytes(original)
            target.unlink()
            rejects(lambda: command(config, rustc, selected))
            rejects(lambda: prepare(directory, executable, check=True))
            target.write_bytes(original)
        prepare(directory, executable, check=True)
        timestamps = {p.name: p.stat().st_mtime_ns for p in directory.iterdir() if p.is_file()}
        prepare(directory, executable)
        assert all(
            (directory / name).stat().st_mtime_ns == stamp for name, stamp in timestamps.items()
        )
        absent = directory / "never-created"
        rejects(lambda: prepare(absent, executable, check=True))
        assert not absent.exists()
        pins = composition.PINS
        for filename in pins:
            composition.PINS = pins | {filename: "0" * 64}
            try:
                rejects(lambda: prepare(absent, executable))
                assert not absent.exists()
            finally:
                composition.PINS = pins

        # Actual rustc proves that routing a file-module and its child preserves
        # Rust's original lexical module base; this is not a WebAudio execution.
        nested = directory / "nested"
        (nested / "foo").mkdir(parents=True)
        (nested / "foo.rs").write_text("pub mod child;\npub fn value() -> u32 { child::VALUE }\n")
        (nested / "foo/child.rs").write_text("pub const VALUE: u32 = 42;\n")
        routed = directory / "foo-routed.rs"
        routed.write_text(
            module_source((nested / "foo.rs").read_text(), nested / "foo", {"child"}, {})
        )
        root_fixture = directory / "root-routed.rs"
        root_fixture.write_text(
            module_source(
                "pub mod foo;\n", nested, {"foo"}, {(nested / "foo.rs").resolve(): routed}
            )
        )
        rejects(lambda: module_source("pub mod foo;\n", nested, {"foo"}, {}))
        subprocess.run(
            [
                "rustc",
                "--crate-name",
                "routing_fixture",
                "--crate-type=lib",
                "--emit=metadata",
                str(root_fixture),
                "-o",
                str(directory / "fixture.rmeta"),
            ],
            check=True,
            timeout=30,
        )

        # Native launch/argv tests use a labelled Python observer, not fake
        # compiler acceptance. The same C++ entry runs real CRT quoting on Windows.
        observer = directory / "argv observer.py"
        observer.write_text(
            "import json,sys\nfrom pathlib import Path\nPath(sys.argv[1]).write_text(json.dumps(sys.argv[2:]),encoding='utf-8')\nraise SystemExit(37)\n",
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
        compile_driver(args.compiler, directory, executable)
        values = [
            "",
            "Grüße 世界",
            "space value",
            '"quoted"',
            'a\\"b',
            "end\\",
            "two\\\\",
            "$HOME;`x`",
            "tab\tvalue",
            "line\nbreak",
        ]
        result = subprocess.run([str(executable), *values], timeout=15)
        assert result.returncode == 37
        assert json.loads(recorded.read_text()) == values
        # Exercise the PRODUCTION Python selector and native entry together.
        prepare(directory, executable, chain=sys.executable)
        compile_driver(args.compiler, directory, executable)
        for name, original in ((CRATE, source), ("ordinary", "ordinary.rs")):
            values = [str(recorded), "--crate-name=" + name, original, "Grüße", ""]
            result = subprocess.run([str(executable), str(observer), *values], timeout=15)
            assert result.returncode == 37
            actual = json.loads(recorded.read_text())
            assert actual == [
                values[1],
                config["adapted"] if name == CRATE else original,
                *values[3:],
            ]
    print(
        "PASS pinned WebAudio routing, reversible leaf edits, freshness/drift/refusal, native argv and exit transport"
    )


if __name__ == "__main__":
    main()
