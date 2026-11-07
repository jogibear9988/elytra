#!/usr/bin/env python3
"""Drive actual checked file bytes through LibWeb pending responses, GC and body streams."""

import argparse
from pathlib import Path
import subprocess
import tempfile


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--probe", type=Path, required=True)
    args = parser.parse_args()
    with tempfile.TemporaryDirectory(prefix="firefly fetch ä ") as temporary:
        fixture = Path(temporary) / "body 世界.bin"
        length = 4 * 1024 * 1024 + 19
        pattern = bytes((i * 37 + 11) & 255 for i in range(256))
        fixture.write_bytes((pattern * (length // 256 + 1))[:length])
        result = subprocess.run(
            [str(args.probe.resolve()), str(fixture)],
            capture_output=True,
            text=True,
            encoding="utf-8",
            timeout=60,
        )
        assert result.returncode == 0, result
        assert f"{length} bytes, bounded credit and GC" in result.stdout, result
        assert "FetchController abort before/after headers" in result.stdout, result
        assert "VERIFICATION FAILED" not in result.stderr, result
        print(result.stdout.strip())
    print(
        "PASS actual native LibWeb response adaptation; scheme dispatch, document admission and public protocol remain open"
    )


if __name__ == "__main__":
    main()
