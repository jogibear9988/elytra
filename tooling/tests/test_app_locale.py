#!/usr/bin/env python3
"""Test native public locale reads without windows, including real isolated OS preferences."""

import _support
import argparse
import json
import os
from pathlib import Path
import subprocess
import sys
import tempfile
from unittest.mock import patch
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    binary = args.firefly.resolve()
    oracle = None
    native_args = []
    if sys.platform == "darwin":
        # Foundation's process argument domain changes no user defaults on disk.
        # Deliberately separate regional format, preferred languages and --lang.
        native_args = ["-AppleLanguages", "(fr-CA,en-US,zh-Hans,es-419)", "-AppleLocale", "en_FI"]
        reference = subprocess.run(
            [
                "/usr/bin/swift",
                str(ROOT / "firefly/tests/locale/system-oracle.swift"),
                *native_args,
            ],
            capture_output=True,
            text=True,
            timeout=45,
            check=True,
        )
        oracle = json.loads(reference.stdout)
        assert oracle["country"] == "FI" and oracle["languages"][:2] == ["fr-CA", "en-US"], oracle
    with tempfile.TemporaryDirectory(prefix="firefly-locale-") as temporary:
        root = Path(temporary)
        common = [ROOT / "firefly/tests/locale/main.mjs"]
        cases = [
            ("launch", "de-DE", ["--lang=de-DE"]),
            ("append", "sr-Latn-RS", ["--lang=de-DE"]),
            ("remove", "ja-JP", []),
        ]
        if oracle is not None:
            cases.append(("system", oracle["languages"][0], []))
        for mode, expected, flags in cases:
            # Each isolated profile starts with an actual stored language preference.
            profile = root / mode
            (profile / "config").mkdir(parents=True)
            if mode != "system":
                (profile / "config/Settings.json").write_text(
                    json.dumps({"languages": ["ja-JP", "ja"]})
                )
            result = run(
                binary, "--electron-compat", *common, mode, expected, profile, *flags, *native_args
            )
            assert result.returncode == 0, result
            assert "PASS actual public locale APIs" in result.stdout, result
            record = next(
                json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")
            )
            assert record["application"] == expected, record
            if oracle is not None:
                assert {key: record[key] for key in oracle} == oracle, (record, oracle)
            print(result.stdout.strip())
    if sys.platform.startswith("linux"):
        for value, expected in [("fr_CA.UTF-8", "fr-CA"), ("C", "en-US-POSIX")]:
            with patch.dict(os.environ, {"LC_ALL": value, "LANG": value, "LANGUAGE": ""}):
                result = run(
                    binary, "--electron-compat", *common, "launch", "de-DE", "", "--lang=de-DE"
                )
            assert result.returncode == 0, result
            record = next(
                json.loads(line) for line in result.stdout.splitlines() if line.startswith("{")
            )
            assert record["system"] == expected, record
            if value == "C":
                assert record["languages"] == [] and record["country"] == "", record
    print(
        "PASS native regional/UI language separation, isolated OS preference oracle and profile/override restoration; no windows"
    )


if __name__ == "__main__":
    main()
