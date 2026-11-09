"""Exact pinned application seam for disabled V8-only window cache preferences."""

import hashlib

SOURCE_SHA256 = "1cbccb2910b4763702be823fb5e49e532f49845694e473893f3f52cbb6ee0e0a"
SOURCE = "window-impl-main.ts"
HELPER_SOURCE = "firefly/compat/code-oss/window-preferences.ts"
HELPER_TARGET = "src/vs/base/common/elytraWindowPreferences.ts"
IMPORT = "import electron, { BrowserWindowConstructorOptions, Display } from 'electron';"
ADDED_IMPORT = "\nimport { libJSWindowCachePreferences } from '../../../base/common/elytraWindowPreferences.js';"
PREFERENCE = "v8CacheOptions: this.environmentMainService.useCodeCache ? 'bypassHeatCheck' : 'none'"
REPLACEMENT = "...libJSWindowCachePreferences(this.environmentMainService.useCodeCache ? 'bypassHeatCheck' : 'none')"


def adapt_window_preferences(data):
    """Change no preference except the single audited V8-only assignment."""
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS window preferences differ from pinned input")
    source = data.decode("utf-8")
    for seam in (IMPORT, PREFERENCE):
        if source.count(seam) != 1:
            raise ValueError(f"Unexpected Code-OSS window preference seam: {seam}")
    return source.replace(IMPORT, IMPORT + ADDED_IMPORT).replace(PREFERENCE, REPLACEMENT).encode()


def revert_window_preferences(data):
    """Prove this overlay preserves every original source byte outside its seams."""
    source = data.decode("utf-8")
    if source.count(IMPORT + ADDED_IMPORT) != 1 or source.count(REPLACEMENT) != 1:
        raise ValueError("Unexpected adapted Code-OSS window preferences")
    original = (
        source.replace(IMPORT + ADDED_IMPORT, IMPORT).replace(REPLACEMENT, PREFERENCE).encode()
    )
    if adapt_window_preferences(original) != data:
        raise ValueError("Adapted Code-OSS window preferences drifted")
    return original
