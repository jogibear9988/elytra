"""Reversible pinned local omission of implicit extension-host engine defaults."""

import hashlib

SOURCE = "local-process-extension-host.ts"
SOURCE_SHA256 = "725eff5c31fd5832e55c0cfa4f131efe0bb3cda4433342e90e56bb93da4f191d"
HELPER_SOURCE = "firefly/compat/code-oss/extension-host-flags.ts"
HELPER_TARGET = "src/vs/base/common/elytraExtensionHostFlags.ts"
IMPORT = "import { timeout } from '../../../../base/common/async.js';"
ADDED_IMPORT = "\nimport { libJSExtensionHostPolicy } from '../../../../base/common/elytraExtensionHostFlags.js';"
START = "\t\tconst [extensionHostCreationResult, portNumber, processEnv] = await Promise.all(["
CHECK = """\t\tconst libJSFlags = libJSExtensionHostPolicy(() => ({
            args: this._environmentService.args,
            isBuilt: this._environmentService.isBuilt,
            debug: this._environmentService.debugExtensionHost,
            development: this._isExtensionDevHost,
            tests: this._environmentService.extensionTestsLocationURI
        }));
"""
FLAGS = "\t\topts.execArgv.unshift('--dns-result-order=ipv4first', '--experimental-network-inspection');"
SELECT = "\n\t\tif (libJSFlags) { opts.execArgv = libJSFlags(opts.execArgv, portNumber); }"


def adapt_extension_host_flags(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS extension host differs from pinned input")
    source = data.decode("utf-8")
    for seam in (IMPORT, START, FLAGS):
        if source.count(seam) != 1:
            raise ValueError("Unexpected Code-OSS extension-host flag seam")
    if source.index(START) >= source.index(FLAGS):
        raise ValueError("Extension-host flag order changed")
    return (
        source.replace(IMPORT, IMPORT + ADDED_IMPORT)
        .replace(START, CHECK + START)
        .replace(FLAGS, FLAGS + SELECT)
        .encode()
    )


def revert_extension_host_flags(data):
    source = data.decode("utf-8")
    for seam in (IMPORT + ADDED_IMPORT, CHECK + START, FLAGS + SELECT):
        if source.count(seam) != 1:
            raise ValueError("Unexpected adapted extension-host flags")
    original = (
        source.replace(IMPORT + ADDED_IMPORT, IMPORT)
        .replace(CHECK + START, START)
        .replace(FLAGS + SELECT, FLAGS)
        .encode()
    )
    if adapt_extension_host_flags(original) != data:
        raise ValueError("Adapted extension-host flags drifted")
    return original
