"""Exact reversible deferral of the pinned agent diff Worker dependency."""

import hashlib

SOURCE = "diffComputeService.ts"
SOURCE_SHA256 = "d33579abe20a405fdc4737d3e7ef2367eadbc41b7d5854b5fe30f77a78bf74de"
HELPER_SOURCE = "firefly/compat/code-oss/diff-worker.ts"
HELPER_TARGET = "src/vs/base/common/elytraDiffWorker.ts"
IMPORT = "import { Worker } from 'worker_threads';"
REPLACEMENT_IMPORT = "import type { Worker } from 'worker_threads';\nimport { requireAgentDiffWorker } from '../../../base/common/elytraDiffWorker.js';"
CONSTRUCT = "const w = new Worker(workerPath, { name: 'Diff compute worker' });"
REPLACEMENT_CONSTRUCT = (
    "const w = new (requireAgentDiffWorker())(workerPath, { name: 'Diff compute worker' });"
)


def adapt_diff_worker(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS agent diff worker source differs from pinned input")
    source = data.decode("utf-8")
    for seam in (IMPORT, CONSTRUCT):
        if source.count(seam) != 1:
            raise ValueError("Unexpected Code-OSS diff worker seam")
    return (
        source.replace(IMPORT, REPLACEMENT_IMPORT)
        .replace(CONSTRUCT, REPLACEMENT_CONSTRUCT)
        .encode()
    )


def revert_diff_worker(data):
    source = data.decode("utf-8")
    if source.count(REPLACEMENT_IMPORT) != 1 or source.count(REPLACEMENT_CONSTRUCT) != 1:
        raise ValueError("Unexpected adapted Code-OSS diff worker source")
    original = (
        source.replace(REPLACEMENT_IMPORT, IMPORT)
        .replace(REPLACEMENT_CONSTRUCT, CONSTRUCT)
        .encode()
    )
    if adapt_diff_worker(original) != data:
        raise ValueError("Adapted Code-OSS diff worker source drifted")
    return original
