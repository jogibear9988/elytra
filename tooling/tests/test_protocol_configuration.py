#!/usr/bin/env python3
"""Check actual native parent/owned-helper origin configuration before first page script."""

import _support

import argparse
import json
from pathlib import Path
import re
import subprocess
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    parser.add_argument("--helper", type=Path, required=True)
    parser.add_argument("--runtime", action="store_true")
    args = parser.parse_args()
    # The owned entry rejects malformed configuration before trying to connect to a parent.
    for options, error in [
        (["--firefly-origin-policy={}"], "Unknown origin configuration schema"),
        (
            ["--firefly-origin-policy=bad", "--firefly-origin-policy=bad"],
            "Duplicate origin configuration option",
        ),
    ]:
        result = subprocess.run(
            [str(args.helper.resolve()), *options], capture_output=True, text=True, timeout=10
        )
        assert result.returncode != 0 and error in result.stderr, result
    page = (ROOT / "firefly/tests/protocol/configuration.html").as_uri()
    cases = [([], False), (["--protocol-policy"], True)]
    if args.runtime:
        cases += [(["--protocol-runtime-helper", "--protocol-policy"], True)]
    pids = set()
    for options, configured in cases:
        result = run(args.probe.resolve(), *options, "--headless=text", page)
        match = re.search(r"^WebContent PID: (\d+)$", result.stdout, re.MULTILINE)
        assert result.returncode == 0 and match, result
        pid = int(match[1])
        assert pid > 0 and pid != result.pid, result
        pids.add(pid)
        native = re.findall(
            r"FIREFLY_ORIGIN_POLICY_READY pid=(\d+) configuration=(\{.*\})", result.stderr
        )
        if configured:
            matching = [json.loads(payload) for child, payload in native if int(child) == pid]
            assert matching == [
                {"schema": 1, "schemes": [{"scheme": "app", "standard": True, "secure": True}]}
            ], result
            assert "FIRST_SCRIPT origin=app://bundle other=app://other" in result.stdout, result
        else:
            assert not native and "FIRST_SCRIPT origin=null other=null" in result.stdout, result
        origins = re.search(r"ORIGINS=(\[.*\]) BRAND=true", result.stdout)
        assert origins, result
        custom = [
            "app://bundle:0",
            "app://bundle:443",
            "app://bundle",
            "app://xn--bcher-kva.example",
            "app://[::1]:9000",
        ]
        expected = custom if configured else ["null"] * 5
        expected += ["null", "null", "https://example.com", "http://example.com:8080"]
        expected += ["app://bundle" if configured else "null"]
        assert json.loads(origins[1]) == expected, result
        assert "NATIVE_ABSENT=true" in result.stdout, result
        assert "crash" not in result.stderr.lower(), result
    assert len(pids) == len(cases), pids
    print("PASS actual owned helper entry rejects invalid/duplicate policy before bootstrap")
    print(
        "PASS native parent-to-owned-renderer configuration before first script; default denial, exact child policy and absent page privileges"
    )
    print(
        "PASS actual Rust DOM origins: normalized tuples, port/host separation, IDNA, IPv6, relative URLs, builtins and getter brand check"
    )
    if args.runtime:
        print(
            "PASS actual production preload helper receives the same bounded native configuration"
        )


if __name__ == "__main__":
    main()
