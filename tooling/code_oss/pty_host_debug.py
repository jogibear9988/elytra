"""Reversible pinned deferral of only the implicit PTY-host dev inspector (CA31)."""

import hashlib

SOURCE = "electronPtyHostStarter.ts"
SOURCE_SHA256 = "20776b27d13f59641b0d51407fa8e0b48874e36d6226876253e3f0c3d6963e47"
HELPER_SOURCE = "firefly/compat/code-oss/pty-host-debug.ts"
HELPER_TARGET = "src/vs/base/common/elytraPtyHostDebug.ts"
IMPORT = "import { parsePtyHostDebugPort } from '../../environment/node/environmentService.js';"
ADDED_IMPORT = (
    "\nimport { libJSPtyHostDebugParams } from '../../../base/common/elytraPtyHostDebug.js';"
)
OPTION = "const inspectParams = parsePtyHostDebugPort(this._environmentMainService.args, this._environmentMainService.isBuilt);"
REPLACEMENT = "const inspectParams = libJSPtyHostDebugParams(this._environmentMainService.args, this._environmentMainService.isBuilt, parsePtyHostDebugPort);"


def adapt_pty_host_debug(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS PTY host differs from pinned input")
    source = data.decode("utf-8")
    for seam in (IMPORT, OPTION):
        if source.count(seam) != 1:
            raise ValueError("Unexpected Code-OSS PTY-host inspector seam")
    return source.replace(IMPORT, IMPORT + ADDED_IMPORT).replace(OPTION, REPLACEMENT).encode()


def revert_pty_host_debug(data):
    source = data.decode("utf-8")
    if source.count(IMPORT + ADDED_IMPORT) != 1 or source.count(REPLACEMENT) != 1:
        raise ValueError("Unexpected adapted Code-OSS PTY-host inspector source")
    original = source.replace(IMPORT + ADDED_IMPORT, IMPORT).replace(REPLACEMENT, OPTION).encode()
    if adapt_pty_host_debug(original) != data:
        raise ValueError("Adapted Code-OSS PTY-host inspector source drifted")
    return original
