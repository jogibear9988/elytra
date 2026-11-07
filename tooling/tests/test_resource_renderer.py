#!/usr/bin/env python3
"""Load a custom-origin document in the actual production helper without a desktop window."""

import argparse
from pathlib import Path
import re
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly renderer resources ä ") as temporary:
        path = Path(temporary) / "index 世界.html"
        path.write_text(
            """<!doctype html><meta charset="utf-8"><title>Native resource navigation</title>
<body><script>
document.body.textContent='RESOURCE_DOM_OK origin='+location.origin+' secure='+isSecureContext+
' capabilities_absent='+['process','require','firefly','electron'].every(k=>typeof globalThis[k]==='undefined');
</script></body>""",
            encoding="utf-8",
        )
        for mode in ["body", "duplicate", "id", "page", "missing", "cancel"]:
            result = subprocess.run(
                [str(args.probe.resolve()), mode, str(path)],
                capture_output=True,
                text=True,
                encoding="utf-8",
                timeout=45,
            )
            assert result.returncode == 0, result
            allowed = mode in ("body", "duplicate")
            assert (
                f"RESOURCE_RENDERER_{'OK' if allowed else 'DENIED'} mode={mode} file_admissions={int(allowed)}"
                in result.stdout
            ), result
            match = re.search(
                r"RESOURCE_PARENT_PID=(\d+) RESOURCE_SOURCE_PID=(-?\d+) RESOURCE_DOCUMENT_PID=(\d+)",
                result.stdout,
            )
            assert match, result
            parent, source, document = map(int, match.groups())
            assert parent > 0 and document > 0 and parent != document, result
            if allowed:
                assert source > 0 and source != parent, result
                assert f"FIREFLY_NAVIGATION_CHANNEL_READY pid={source} epoch=1" in result.stderr, (
                    result
                )
            else:
                assert "FIREFLY_NAVIGATION_CHANNEL_READY" not in result.stderr, result
            assert "VERIFICATION FAILED" not in result.stderr, result
            print(result.stdout.strip(), flush=True)
    print(
        "PASS real production-helper custom-origin document navigation; public protocol remains open"
    )


if __name__ == "__main__":
    main()
