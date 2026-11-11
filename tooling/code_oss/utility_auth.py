"""Pinned local utility-process opt-out of unsupported authentication forwarding."""

import hashlib

SOURCE = "utility-process-main.ts"
SOURCE_SHA256 = "991465758e745c162bb97506fdbc2227b38d40704042367cde4c9fa38a14aea8"
HELPER_SOURCE = "firefly/compat/code-oss/utility-auth.ts"
HELPER_TARGET = "src/vs/base/common/elytraUtilityAuth.ts"
IMPORT = "import { BrowserWindow, Details, MessageChannelMain, app, utilityProcess, UtilityProcess as ElectronUtilityProcess } from 'electron';"
ADDED_IMPORT = "\nimport { libJSUtilityAuthForwarding, libJSUtilityUnsignedLibraries } from '../../../base/common/elytraUtilityAuth.js';"
OPTION = "const respondToAuthRequestsFromMainProcess = this.configuration.respondToAuthRequestsFromMainProcess;"
REPLACEMENT = "const respondToAuthRequestsFromMainProcess = libJSUtilityAuthForwarding(this.configuration.respondToAuthRequestsFromMainProcess);"
UNSIGNED_OPTION = (
    "const allowLoadingUnsignedLibraries = this.configuration.allowLoadingUnsignedLibraries;"
)
UNSIGNED_REPLACEMENT = "const allowLoadingUnsignedLibraries = libJSUtilityUnsignedLibraries(this.configuration.allowLoadingUnsignedLibraries);"


def adapt_utility_auth(data):
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS utility process differs from pinned input")
    source = data.decode("utf-8")
    for seam in (IMPORT, OPTION, UNSIGNED_OPTION):
        if source.count(seam) != 1:
            raise ValueError("Unexpected Code-OSS utility authentication seam")
    return (
        source.replace(IMPORT, IMPORT + ADDED_IMPORT)
        .replace(OPTION, REPLACEMENT)
        .replace(UNSIGNED_OPTION, UNSIGNED_REPLACEMENT)
        .encode()
    )


def revert_utility_auth(data):
    source = data.decode("utf-8")
    if source.count(IMPORT + ADDED_IMPORT) != 1 or source.count(REPLACEMENT) != 1:
        raise ValueError("Unexpected adapted Code-OSS utility authentication source")
    original = (
        source.replace(IMPORT + ADDED_IMPORT, IMPORT)
        .replace(REPLACEMENT, OPTION)
        .replace(UNSIGNED_REPLACEMENT, UNSIGNED_OPTION)
        .encode()
    )
    if adapt_utility_auth(original) != data:
        raise ValueError("Adapted Code-OSS utility authentication source drifted")
    return original
