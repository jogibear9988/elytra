#!/usr/bin/env python3
"""Batch actual public file-handler acceptance in one hidden-window Firefly application."""

import _support

import argparse
from pathlib import Path
import tempfile
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly public bundle ä ") as temporary:
        root = Path(temporary)
        (root / "index.html").write_text(
            '<!doctype html><meta charset="utf-8"><title>PUBLIC_PROTOCOL_PENDING</title>'
            '<link rel="stylesheet" href="theme.css"><script type="module" src="main.mjs"></script><body>Bundle</body>',
            encoding="utf-8",
        )
        (root / "math.mjs").write_text("export const answer = 42;\n", encoding="utf-8")
        (root / "theme.css").write_text(":root { --resource-flag: loaded; }\n", encoding="utf-8")
        pattern = bytes((i * 37 + 11) & 255 for i in range(256))
        length = 4194323
        (root / "payload.bin").write_bytes((pattern * (length // 256 + 1))[:length])
        (root / "main.mjs").write_text(
            """import { answer } from './math.mjs';
try {
    const response = await fetch('./payload.bin');
    if (response.headers.get('set-cookie') !== null || response.headers.get('cache-control') !== 'no-store') throw Error('headers');
    const bytes = new Uint8Array(await response.arrayBuffer());
    if (bytes.length !== 4194323 || !bytes.every((value, i) => value === ((i * 37 + 11) & 255))) throw Error('bytes');
    const head = await fetch('./payload.bin', { method: 'HEAD' });
    if ((await head.arrayBuffer()).byteLength || head.headers.get('content-length') !== String(bytes.length)) throw Error('HEAD');
    for (const [url, options] of [['./denied.bin', {}], ['app://other/payload.bin', {}], ['./payload.bin', {method: 'POST'}]]) {
        let denied = false;
        try { await fetch(url, options); } catch { denied = true; }
        if (!denied) throw Error('request scope');
    }
    if (answer !== 42 || getComputedStyle(document.documentElement).getPropertyValue('--resource-flag').trim() !== 'loaded' ||
        location.origin !== 'app://bundle' || !isSecureContext ||
        !['process', 'require', 'firefly', 'electron'].every(k => typeof globalThis[k] === 'undefined')) throw Error('document');
    document.title = 'PUBLIC_PROTOCOL_OK';
} catch (error) { document.title = 'PUBLIC_PROTOCOL_ERROR ' + error; }
""",
            encoding="utf-8",
        )
        (root / "pending.html").write_text(
            '<!doctype html><title>PENDING</title><script type="module" src="pending.mjs"></script>',
            encoding="utf-8",
        )
        (root / "pending.mjs").write_text(
            "fetch('./late.bin').then(() => { document.title='UNEXPECTED_SUCCESS'; }, () => { document.title='PUBLIC_PROTOCOL_CANCELLED'; });",
            encoding="utf-8",
        )
        result = run(args.firefly.resolve(), ROOT / "firefly/tests/protocol/public/main.mjs", root)
        assert result.returncode == 0, result
        assert "PASS public default-session file handler:" in result.stdout, result
        assert "PASS public file handler: load failures" in result.stdout, result
        assert "VERIFICATION FAILED" not in result.stderr, result
        print(result.stdout.strip())


if __name__ == "__main__":
    main()
