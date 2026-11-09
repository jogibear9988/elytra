#!/usr/bin/env python3
"""Exercise probe-owned native titlebar profiles with Node and standalone Mite; no GUI."""

import _support

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile

ROOT = Path(__file__).resolve().parents[2]
PROBE = ROOT / "tooling/code_oss/probe.mjs"


def check(runtime):
    owned = []
    try:
        with tempfile.TemporaryDirectory(prefix="elytra-probe-profile-test-") as temporary:
            tree = Path(temporary)
            (tree / "src").mkdir()
            (tree / "package.json").write_text('{"type":"module"}')
            (tree / "src/main.js").write_text(
                "console.log(JSON.stringify({consumerArgv:process.argv.slice(3)}));\n"
            )

            def run(*options):
                result = subprocess.run(
                    [str(runtime), str(PROBE), str(tree), *options],
                    text=True,
                    capture_output=True,
                    timeout=10,
                )
                rows = [
                    json.loads(line)
                    for line in (result.stdout + result.stderr).splitlines()
                    if line.startswith("{")
                ]
                for row in rows:
                    profile = row.get("diagnosticProfile", {})
                    if profile.get("isolated"):
                        directory = Path(profile["userDataDirectory"])
                        if directory not in owned:
                            owned.append(directory)
                return result, rows

            result, rows = run("--trace", "a b")
            assert result.returncode == 0, result
            assert rows[0]["diagnosticProfile"] == {
                "titleBarStyle": "default",
                "isolated": False,
                "workbenchAccepted": False,
            }, rows
            assert rows[1]["consumerArgv"] == ["--trace", "a b"], rows

            paths = []
            for options in (("--native-title-bar", "--trace"), ("--trace", "--native-title-bar")):
                result, rows = run(*options)
                assert result.returncode == 0, result
                profile = rows[0]["diagnosticProfile"]
                assert profile["titleBarStyle"] == "native" and profile["isolated"], rows
                assert profile["workbenchAccepted"] is False and profile["retainedForInspection"]
                directory = Path(profile["userDataDirectory"])
                paths.append(directory)
                assert directory.is_absolute() and directory not in (tree, Path.home())
                assert json.loads((directory / "User/settings.json").read_text()) == {
                    "window.titleBarStyle": "native"
                }
                extensions = Path(profile["extensionsDirectory"])
                assert extensions == directory / "extensions" and extensions.is_dir()
                assert not list(extensions.iterdir())
                assert rows[1]["consumerArgv"] == [
                    "--trace",
                    "--user-data-dir",
                    str(directory),
                    "--extensions-dir",
                    str(extensions),
                ]
                assert rows[2]["entryCompleted"] is True and rows[2]["workbenchAccepted"] is False
            assert paths[0] != paths[1], "Each run must exclusively own a fresh profile"

            existing = tree / "existing"
            (existing / "User").mkdir(parents=True)
            settings = existing / "User/settings.json"
            original = b'{"window.titleBarStyle":"custom","untouched":true}\n'
            settings.write_bytes(original)
            provided_extensions = tree / "existing-extensions"
            provided_extensions.mkdir()
            sentinel = provided_extensions / "untouched.txt"
            sentinel.write_bytes(b"personal extension sentinel")
            for options in (
                ("--native-title-bar", "--user-data-dir", str(existing)),
                ("--user-data-dir=" + str(existing), "--native-title-bar"),
                ("--native-title-bar", "--native-title-bar"),
                ("--native-title-bar", "--extensions-dir", str(provided_extensions)),
                ("--extensions-dir=" + str(provided_extensions), "--native-title-bar"),
            ):
                result, rows = run(*options)
                assert result.returncode != 0 and not rows, result
                assert "no existing --user-data-dir" in result.stderr, result
                assert settings.read_bytes() == original
                assert sentinel.read_bytes() == b"personal extension sentinel"

            # Existing caller profiles are entirely untouched on the default path.
            caller_options = [
                "--user-data-dir",
                str(existing),
                "--extensions-dir",
                str(provided_extensions),
            ]
            result, rows = run(*caller_options)
            assert result.returncode == 0 and rows[1]["consumerArgv"] == caller_options
            assert settings.read_bytes() == original
            # A probe flag after -- is a literal file argument, never an opt-in.
            literal = ["--", "--native-title-bar", "--extensions-dir", str(provided_extensions)]
            result, rows = run(*literal)
            assert result.returncode == 0 and rows[1]["consumerArgv"] == literal
            assert rows[0]["diagnosticProfile"]["isolated"] is False
            # With a real opt-in before --, injected options stay before it.
            result, rows = run("--native-title-bar", *literal)
            assert result.returncode == 0, result
            profile = rows[0]["diagnosticProfile"]
            assert rows[1]["consumerArgv"] == [
                "--user-data-dir",
                profile["userDataDirectory"],
                "--extensions-dir",
                profile["extensionsDirectory"],
                *literal,
            ]
            assert sentinel.read_bytes() == b"personal extension sentinel"
            (tree / "src/main.js").write_text("throw new Error('intentional consumer failure');\n")
            result, rows = run("--native-title-bar")
            assert result.returncode == 1, result
            assert rows[1]["entryCompleted"] is False and rows[1]["workbenchAccepted"] is False
            assert rows[1]["diagnosticProfile"] == rows[0]["diagnosticProfile"]
    finally:
        for directory in owned:
            shutil.rmtree(directory)
    print(f"PASS {runtime}: isolated native titlebar/default/conflict/failure probe profiles")


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    assert args.node
    for runtime in (args.node, args.mite.resolve()):
        check(runtime)


if __name__ == "__main__":
    main()
