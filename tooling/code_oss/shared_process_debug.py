"""Reversible pinned opt-out of Code-OSS's implicit shared-process V8 inspector."""

import hashlib

SOURCE = "shared-process-main.ts"
SOURCE_SHA256 = "ef759e2e771b0a6e16be5dc579a56200fe131b6ae9a5763d6d42259036068bd7"
HELPER_SOURCE = "firefly/compat/code-oss/shared-process-debug.ts"
HELPER_TARGET = "src/vs/base/common/elytraSharedProcessDebug.ts"
IMPORT = (
    "import { parseSharedProcessDebugPort } from '../../environment/node/environmentService.js';"
)
ADDED_IMPORT = "\nimport { libJSSharedProcessDebugParams } from '../../../base/common/elytraSharedProcessDebug.js';"
OPTION = "const inspectParams = parseSharedProcessDebugPort(this.environmentMainService.args, this.environmentMainService.isBuilt);"
REPLACEMENT = "const inspectParams = libJSSharedProcessDebugParams(this.environmentMainService.args, this.environmentMainService.isBuilt, parseSharedProcessDebugPort);"


def adapt_shared_process_debug(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS shared process differs from pinned input")
    source = data.decode("utf-8")
    for seam in (IMPORT, OPTION):
        if source.count(seam) != 1:
            raise ValueError("Unexpected Code-OSS shared-process inspector seam")
    return source.replace(IMPORT, IMPORT + ADDED_IMPORT).replace(OPTION, REPLACEMENT).encode()


def revert_shared_process_debug(data):
    source = data.decode("utf-8")
    if source.count(IMPORT + ADDED_IMPORT) != 1 or source.count(REPLACEMENT) != 1:
        raise ValueError("Unexpected adapted Code-OSS shared-process inspector source")
    original = source.replace(IMPORT + ADDED_IMPORT, IMPORT).replace(REPLACEMENT, OPTION).encode()
    if adapt_shared_process_debug(original) != data:
        raise ValueError("Adapted Code-OSS shared-process inspector source drifted")
    return original
