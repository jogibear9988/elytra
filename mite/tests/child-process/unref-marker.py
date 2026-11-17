"""Publish complete child-survival evidence after the independent parent exits."""

from pathlib import Path
import sys
import time

ack = Path(sys.argv[1])
result = Path(sys.argv[2])
for _ in range(400):
    if ack.exists():
        # Existence is the reader's ready signal. A direct write exposes an empty
        # file between creation and write completion, causing a false CI failure.
        temporary = result.with_suffix(".tmp")
        temporary.write_text("CHILD_SURVIVED", encoding="utf-8")
        temporary.replace(result)
        break
    time.sleep(0.02)
