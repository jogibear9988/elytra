"""Authenticate this exclusively owned signal target over its private parent pipe."""

import json
import os
import sys
import time

print(
    json.dumps({"pid": os.getpid(), "parent": os.getppid(), "nonce": sys.argv[1]}),
    flush=True,
)
# The independent parent must observe real signal termination before this expires.
time.sleep(120)
