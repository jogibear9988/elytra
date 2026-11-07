#!/usr/bin/env python3
"""Exercise real committed-document module, CSS and bounded binary resource loading."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly document bundle ä ") as temporary:
        root = Path(temporary)
        path = root / "index 世界.html"
        path.write_text(
            '<!doctype html><meta charset="utf-8"><title>Native bundle</title>'
            '<link rel="stylesheet" href="./theme.css">'
            '<script type="module" src="./main.mjs"></script><body>RESOURCE_BUNDLE_PENDING</body>',
            encoding="utf-8",
        )
        (root / "math.mjs").write_text("export const answer = 42;\n", encoding="utf-8")
        (root / "theme.css").write_text(":root { --resource-flag: loaded; }\n", encoding="utf-8")
        (root / "payload.bin").write_bytes(bytes((i * 37 + 11) & 255 for i in range(4194323)))
        (root / "main.mjs").write_text(
            """import { answer } from './math.mjs';
try {
    const response = await fetch('./payload.bin');
    if (!response.ok || response.headers.get('set-cookie') !== null ||
        response.headers.get('cache-control') !== 'no-store') throw Error('response headers');
    const bytes = new Uint8Array(await response.arrayBuffer());
    if (bytes.length !== 4194323 || !bytes.every((value, i) => value === ((i * 37 + 11) & 255)))
        throw Error('binary contents');
    const head = await fetch('./payload.bin', { method: 'HEAD' });
    const empty = (await head.arrayBuffer()).byteLength === 0;
    if (head.headers.get('content-length') !== String(bytes.length)) throw Error('HEAD length');
    let denied = false;
    try { await fetch('./denied.bin'); } catch { denied = true; }
    const css = getComputedStyle(document.documentElement).getPropertyValue('--resource-flag').trim();
    const absent = ['process', 'require', 'firefly', 'electron'].every(k => typeof globalThis[k] === 'undefined');
    document.body.textContent = 'RESOURCE_BUNDLE_OK origin=' + location.origin + ' secure=' + isSecureContext +
        ' css=' + css + ' answer=' + answer + ' bytes=' + bytes.length + ' head=' + empty +
        ' denied=' + denied + ' capabilities_absent=' + absent;
} catch (error) {
    document.body.textContent = 'RESOURCE_BUNDLE_ERROR ' + error;
}
""",
            encoding="utf-8",
        )
        for mode in ["resources", "document-duplicate", "document-id", "document-missing", "away"]:
            result = subprocess.run(
                [str(args.probe.resolve()), mode, str(path)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=45,
            )
            assert result.returncode == 0, result
            allowed = mode in ("resources", "document-duplicate", "away")
            match = re.search(
                rf"DOCUMENT_RENDERER_{'OK' if allowed else 'DENIED'} mode={mode} resource_requests=(\d+) teardown=(true|false)",
                result.stdout,
            )
            assert match, result
            assert int(match[1]) >= 6 if allowed else int(match[1]) == 0, result
            assert (match[2] == "true") == (mode == "away"), result
            identity = re.search(
                r"RESOURCE_PARENT_PID=(\d+) RESOURCE_SOURCE_PID=(-?\d+) RESOURCE_DOCUMENT_PID=(\d+)",
                result.stdout,
            )
            assert identity, result
            parent, source, document = map(int, identity.groups())
            assert parent > 0 and source > 0 and document > 0 and parent != document, result
            if allowed:
                # Away may select a new host for its replacement data: document.
                ready = re.findall(
                    r"FIREFLY_DOCUMENT_CHANNEL_READY pid=(\d+) epoch=2", result.stderr
                )
                assert len(ready) == 1 and int(ready[0]) != parent, result
                if mode != "away":
                    assert int(ready[0]) == document, result
            else:
                assert "FIREFLY_DOCUMENT_CHANNEL_READY" not in result.stderr, result
            assert "VERIFICATION FAILED" not in result.stderr, result
            print(result.stdout.strip(), flush=True)
    print(
        "PASS production-helper ESM/CSS/binary/HEAD/denial, exact-document grant and navigation teardown"
    )


if __name__ == "__main__":
    main()
