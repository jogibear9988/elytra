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
    for (const [url, options] of [['./denied.bin', {}], ['app://other/payload.bin', {}], ['cors://target/wild', {}], ['./payload.bin', {method: 'POST'}]]) {
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
        for name in ["cors", "cors-pending"]:
            (root / f"{name}.html").write_text(
                f'<!doctype html><title>PUBLIC_CORS_PENDING</title><script type="module" src="{name}.mjs"></script>',
                encoding="utf-8",
            )
        (root / "cors.mjs").write_text(
            """try {
    for (const route of ['wild', 'exact']) {
        const response = await fetch('cors://target/' + route);
        if (response.type !== 'cors' || response.url !== 'cors://target/' + route ||
            response.headers.get('x-public') !== 'visible' || response.headers.has('x-hidden') ||
            response.headers.has('set-cookie') || response.headers.has('access-control-allow-origin')) throw Error('CORS filtering');
        const bytes = new Uint8Array(await response.arrayBuffer());
        if (bytes.length !== 4194323 || !bytes.every((value, i) => value === ((i * 37 + 11) & 255))) throw Error('CORS bytes');
    }
    const head = await fetch('cors://target/exact', {method:'HEAD', credentials:'omit'});
    if (head.type !== 'cors' || (await head.arrayBuffer()).byteLength) throw Error('CORS HEAD');
    for (const [url, options] of [
        ['cors://target/deny', {}], ['cors://target/mismatch', {}], ['cors://target/duplicate', {}],
        ['plain://target/wild', {}], ['app://other/payload.bin', {}],
        ['cors://target/wild', {credentials:'include'}], ['cors://target/wild', {mode:'no-cors'}],
        ['cors://target/wild', {mode:'same-origin'}], ['cors://target/wild', {method:'POST'}],
        ['cors://target/wild', {headers:{'X-Private':'test'}}], ['cors://target/wild', {headers:{Range:'bytes=0-1'}}]
    ]) {
        let denied = false;
        try { await fetch(url, options); } catch { denied = true; }
        if (!denied) throw Error('CORS admission: ' + JSON.stringify(options));
    }
    const afterDenial = await fetch('cors://target/exact', {method:'HEAD'});
    if (afterDenial.type !== 'cors' || (await afterDenial.arrayBuffer()).byteLength) throw Error('channel after refusal');
    document.title = 'PUBLIC_CORS_OK';
} catch (error) { document.title = 'PUBLIC_CORS_ERROR ' + error; }
""",
            encoding="utf-8",
        )
        (root / "cors-pending.mjs").write_text(
            """try {
    let denied = false;
    try { await fetch('cors://target/pending'); } catch { denied = true; }
    if (!denied) throw Error('stale target');
    denied = false;
    try { await fetch('app://bundle/payload.bin'); } catch { denied = true; }
    if (!denied) throw Error('retired source channel');
    document.title = 'PUBLIC_CORS_REVOKED';
} catch (error) { document.title = 'PUBLIC_CORS_ERROR ' + error; }
""",
            encoding="utf-8",
        )
        result = run(args.firefly.resolve(), ROOT / "firefly/tests/protocol/public/main.mjs", root)
        assert result.returncode == 0, result
        assert "PASS public default-session file handler:" in result.stdout, result
        assert "PASS public CORS file handlers:" in result.stdout, result
        assert "PASS public file handler: load failures" in result.stdout, result
        assert "VERIFICATION FAILED" not in result.stderr, result
        print(result.stdout.strip())


if __name__ == "__main__":
    main()
