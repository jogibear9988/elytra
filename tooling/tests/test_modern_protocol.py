#!/usr/bin/env python3
"""Exercise public modern handlers and file interception in one isolated hidden window."""

import _support

import argparse
from pathlib import Path
import tempfile
from test_renderer import ROOT, run


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--firefly", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly modern protocol ") as temporary:
        root = Path(temporary)
        for name, title in [("native", "NATIVE_OK"), ("legacy", "LEGACY_OK")]:
            (root / f"{name}.html").write_text(f"<!doctype html><title>{title}</title>")
        (root / "native.html").write_text("""<!doctype html><title>NATIVE_OK</title>
<script>
setTimeout(() => {
  fetch(location.href).then(() => document.title='BAD', () => document.title='FILE_DENIED');
}, 1000);
</script>""")
        (root / "pending.html").write_text("""<!doctype html><title>PENDING</title>
<script>
fetch('./pending').then(r=>r.arrayBuffer()).then(()=>document.title='BAD',()=>document.title='RETIRED');
</script>""")
        (root / "modern.html").write_text(
            """<!doctype html><title>MODERN_PENDING</title>
<script type="module">
try {
  const response = await fetch('./binary');
  if (response.status !== 404 || response.headers.has('content-length') || response.headers.has('set-cookie') || response.headers.get('content-type') !== 'application/octet-stream') throw Error('metadata');
  const bytes = new Uint8Array(await response.arrayBuffer());
  if (bytes.length !== 4194323 || !bytes.every((value, i) => value === ((i * 37 + 11) & 255))) throw Error('bytes');
  const head = await fetch('./binary', {method:'HEAD'});
  if (head.status !== 404 || head.headers.get('content-length') !== '4194323' || (await head.arrayBuffer()).byteLength) throw Error('HEAD');
  const failure = await fetch('./failure');
  if (failure.status !== 500 || await failure.text() !== 'unavailable') throw Error('500');
  if (!['process','require','firefly','electron'].every(key => typeof globalThis[key] === 'undefined')) throw Error('capability');
  document.title='MODERN_OK';
} catch (error) { document.title='MODERN_ERROR '+error; }
</script>""",
            encoding="utf-8",
        )
        result = run(
            args.firefly.resolve(), ROOT / "firefly/tests/protocol/public/modern.mjs", root
        )
        assert result.returncode == 0, result
        for marker in [
            "PASS public modern main net.fetch:",
            "PASS public modern renderer:",
            "PASS public modern native cancellation:",
            "PASS public file policy:",
            "PASS public file response:",
            "PASS public modern closed owner:",
        ]:
            assert marker in result.stdout, result
        assert "VERIFICATION FAILED" not in result.stderr, result
        print(result.stdout.strip())


if __name__ == "__main__":
    main()
