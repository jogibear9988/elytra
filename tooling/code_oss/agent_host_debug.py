"""Reversible pinned opt-out of Code-OSS's implicit agent-host V8 inspector."""

import hashlib

SOURCE = "electronAgentHostStarter.ts"
SOURCE_SHA256 = "50140c223133999f91d336ad2b6ddc0601fa25ee3689455d477879b6cf3e6f31"
HELPER_SOURCE = "firefly/compat/code-oss/agent-host-debug.ts"
HELPER_TARGET = "src/vs/base/common/elytraAgentHostDebug.ts"
IMPORT = "import { parseAgentHostDebugPort } from '../../environment/node/environmentService.js';"
ADDED_IMPORT = (
    "\nimport { libJSAgentHostDebugParams } from '../../../base/common/elytraAgentHostDebug.js';"
)
OPTION = "const inspectParams = parseAgentHostDebugPort(this._environmentMainService.args, this._environmentMainService.isBuilt);"
REPLACEMENT = "const inspectParams = libJSAgentHostDebugParams(this._environmentMainService.args, this._environmentMainService.isBuilt, parseAgentHostDebugPort);"


def adapt_agent_host_debug(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS agent host differs from pinned input")
    source = data.decode("utf-8")
    for seam in (IMPORT, OPTION):
        if source.count(seam) != 1:
            raise ValueError("Unexpected Code-OSS agent-host inspector seam")
    return source.replace(IMPORT, IMPORT + ADDED_IMPORT).replace(OPTION, REPLACEMENT).encode()


def revert_agent_host_debug(data):
    source = data.decode("utf-8")
    if source.count(IMPORT + ADDED_IMPORT) != 1 or source.count(REPLACEMENT) != 1:
        raise ValueError("Unexpected adapted Code-OSS agent-host inspector source")
    original = source.replace(IMPORT + ADDED_IMPORT, IMPORT).replace(REPLACEMENT, OPTION).encode()
    if adapt_agent_host_debug(original) != data:
        raise ValueError("Adapted Code-OSS agent-host inspector source drifted")
    return original
