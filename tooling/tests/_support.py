"""Let standalone regression CLIs import the repository's shared build helpers.

Python puts this tests directory first when a test is launched by filename.
Keep sibling test helpers there and add the build-tool directory immediately
behind it, independent of the caller's working directory. Runtime subprocess
imports and environment variables are unaffected.
"""

from pathlib import Path
import sys

TOOLING_DIRECTORY = Path(__file__).resolve().parents[1]
sys.path.insert(1, str(TOOLING_DIRECTORY))
