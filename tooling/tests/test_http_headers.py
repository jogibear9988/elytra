#!/usr/bin/env python3
"""Compare HTTP header APIs and constants without starting network listeners."""

import _support  # Shared build-helper import path for standalone test CLIs.

import argparse
import json
from pathlib import Path
import shutil
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    args = parser.parse_args()
    if not args.node:
        parser.error("Node is required")
    cases = []
    for name in [
        '"X-Test"',
        '"x_test"',
        '""',
        '"bad name"',
        '"bad:header"',
        '"\\u00e9"',
        "null",
        "undefined",
        "12",
        'Symbol("x")',
    ]:
        cases.append(f"http.validateHeaderName({name})")
        for value in [
            "undefined",
            "null",
            '"ok"',
            "12",
            "true",
            '"\\tallowed"',
            '"bad\\r\\nvalue"',
            '"\\u00ff"',
            '"\\u0100"',
            '["one","two"]',
        ]:
            cases.append(f"res.setHeader({name},{value})===res")
    for value in [
        "undefined",
        "null",
        '""',
        "true",
        "12",
        '"ok"',
        '"\\u0000"',
        '"bad\\nvalue"',
        '"\\u00ff"',
        '"\\u0100"',
        '["one","two"]',
        '["ok","bad\\r\\n"]',
    ]:
        cases.append(f'http.validateHeaderValue("x-test",{value})')
    cases += [
        '(()=>{res.setHeader("X-Test",1);res.appendHeader("x-test",[2,3]);return [res.getHeader("X-TEST"),res.getHeaders(),res.getHeaderNames(),res.getRawHeaderNames(),Object.getPrototypeOf(res.getHeaders())===null];})()',
        '(()=>{res.setHeader("X-Test","a");res.removeHeader("X-TEST");return [res.hasHeader("x-test"),res.getHeader("x-test")];})()',
        '(()=>{res.sendDate=false;res.writeHead(201,"Custom",{"X-Test":"yes"});return [res.headersSent,res.statusCode,res.statusMessage];})()',
        '(()=>{res.writeHead(200);res.setHeader("late","no");})()',
        '(()=>{res.writeHead(200);res.removeHeader("late");})()',
        "res.writeHead(99)",
        "res.writeHead(1000)",
        'res.writeHead(200,"bad\\r\\nmessage")',
        '(()=>{res.writeHead(200);return [res.getHeaders(),res.getHeaderNames(),res.getHeader("date")];})()',
        "http.METHODS",
        "http.STATUS_CODES",
        "http.maxHeaderSize",
        "process.versions.llhttp",
    ]
    policies = [
        '(()=>{const values=["ok"];res.setHeader("X-Test",values);values.push("bad\\r\\nInjected: yes");res.writeHead(200);return res.headersSent;})()',
        '(()=>{res.setHeader("Content-Length",1);res.setHeader("Transfer-Encoding","chunked");res.writeHead(200);return res.headersSent;})()',
    ]
    source = "import http from 'node:http';import {Socket} from 'node:net';\n" + "\n".join(
        f"""{{const res=new http.ServerResponse(new http.IncomingMessage(new Socket()));try{{console.log(JSON.stringify({{value:({expr})}}));}}catch(e){{console.log(JSON.stringify({{error:e.name,code:e.code}}));}}}}"""
        for expr in cases + policies
    )
    with tempfile.TemporaryDirectory(prefix="mite-http-") as directory:
        fixture = Path(directory) / "headers.mjs"
        fixture.write_text(source)
        results = [
            subprocess.run([exe, str(fixture)], capture_output=True, text=True, timeout=10)
            for exe in [args.node, args.mite]
        ]
    for result in results:
        if result.returncode or result.stderr:
            print(result)
            return 1
    rows = [[json.loads(line) for line in result.stdout.splitlines()] for result in results]
    failures = 0
    for expr, expected, actual in zip(cases, *rows):
        if expected != actual:
            failures += 1
            print(f"FAIL {expr}: Node={expected!r} Mite={actual!r}")
    if any(len(row) != len(cases) + len(policies) for row in rows):
        failures += 1
    print(f"{len(cases)} HTTP header/constant comparisons, {failures} differences")
    expected = [
        {"error": "TypeError", "code": "ERR_INVALID_CHAR"},
        {"error": "Error", "code": "ERR_HTTP_INVALID_HEADER_VALUE"},
    ]
    for index, (label, actual) in enumerate(
        zip(["mutated header array", "ambiguous response framing"], expected)
    ):
        passed = (
            rows[0][len(cases) + index] == {"value": True} and rows[1][len(cases) + index] == actual
        )
        print(f"{'PASS' if passed else 'FAIL'} recorded stricter HTTP policy: {label}")
        failures += int(not passed)
    return int(bool(failures))


if __name__ == "__main__":
    raise SystemExit(main())
