"""Exact pinned local-application omissions of unavailable engine preferences."""

import hashlib

SOURCE_SHA256 = "1cbccb2910b4763702be823fb5e49e532f49845694e473893f3f52cbb6ee0e0a"
SOURCE = "window-impl-main.ts"
HELPER_SOURCE = "firefly/compat/code-oss/window-preferences.ts"
HELPER_TARGET = "src/vs/base/common/elytraWindowPreferences.ts"
IMPORT = "import electron, { BrowserWindowConstructorOptions, Display } from 'electron';"
ADDED_IMPORT = "\nimport { libJSWindowCachePreferences, skipLibJSTouchBar, assertLibJSTouchBarSupported } from '../../../base/common/elytraWindowPreferences.js';"
PREFERENCE = "v8CacheOptions: this.environmentMainService.useCodeCache ? 'bypassHeatCheck' : 'none'"
REPLACEMENT = "...libJSWindowCachePreferences(this.environmentMainService.useCodeCache ? 'bypassHeatCheck' : 'none')"
TOUCHBAR_CREATE = "\t\tthis.createTouchBar();"
TOUCHBAR_SKIP = "\t\tif (!skipLibJSTouchBar()) { this.createTouchBar(); }"
TOUCHBAR_UPDATE = "\tupdateTouchBar(groups: ISerializableCommandAction[][]): void {"
TOUCHBAR_CHECK = TOUCHBAR_UPDATE + "\n\t\tassertLibJSTouchBarSupported();"


def adapt_window_preferences(data):
    """Omit only audited cache/optional TouchBar entries in the local app."""
    if hashlib.sha256(data).hexdigest() != SOURCE_SHA256:
        raise ValueError("Code-OSS window preferences differ from pinned input")
    source = data.decode("utf-8")
    for seam in (IMPORT, PREFERENCE, TOUCHBAR_CREATE, TOUCHBAR_UPDATE):
        if source.count(seam) != 1:
            raise ValueError(f"Unexpected Code-OSS window preference seam: {seam}")
    return (
        source.replace(IMPORT, IMPORT + ADDED_IMPORT)
        .replace(PREFERENCE, REPLACEMENT)
        .replace(TOUCHBAR_CREATE, TOUCHBAR_SKIP)
        .replace(TOUCHBAR_UPDATE, TOUCHBAR_CHECK)
        .encode()
    )


def revert_window_preferences(data):
    """Prove this overlay preserves every original source byte outside its seams."""
    source = data.decode("utf-8")
    if (
        source.count(IMPORT + ADDED_IMPORT) != 1
        or source.count(REPLACEMENT) != 1
        or source.count(TOUCHBAR_SKIP) != 1
        or source.count(TOUCHBAR_CHECK) != 1
    ):
        raise ValueError("Unexpected adapted Code-OSS window preferences")
    original = (
        source.replace(IMPORT + ADDED_IMPORT, IMPORT)
        .replace(REPLACEMENT, PREFERENCE)
        .replace(TOUCHBAR_SKIP, TOUCHBAR_CREATE)
        .replace(TOUCHBAR_CHECK, TOUCHBAR_UPDATE)
        .encode()
    )
    if adapt_window_preferences(original) != data:
        raise ValueError("Adapted Code-OSS window preferences drifted")
    return original


# Legacy script paste is distinct from native UI paste. Omit only this pinned
# application opt-in; Firefly's general enableDeprecatedPaste API stays rejected.
PASTE_SOURCE = "window-defaults-main.ts"
PASTE_SHA256 = "087b784eec1f1f565e7eed9cd8b315102a9900888df28c79dc1cd30bde8c36e0"
PASTE_IMPORT = "import electron, { Display, Rectangle } from 'electron';"
PASTE_ADDED_IMPORT = "\nimport { libJSWindowPastePreferences } from '../../../base/common/elytraWindowPreferences.js';"
PASTE_PREFERENCE = "enableDeprecatedPaste: true"
PASTE_REPLACEMENT = "...libJSWindowPastePreferences(true)"


def adapt_paste_preferences(data):
    if hashlib.sha256(data).hexdigest() != PASTE_SHA256:
        raise ValueError("Code-OSS default preferences differ from pinned input")
    source = data.decode("utf-8")
    for seam in (PASTE_IMPORT, PASTE_PREFERENCE):
        if source.count(seam) != 1:
            raise ValueError(f"Unexpected Code-OSS paste preference seam: {seam}")
    return (
        source.replace(PASTE_IMPORT, PASTE_IMPORT + PASTE_ADDED_IMPORT)
        .replace(PASTE_PREFERENCE, PASTE_REPLACEMENT)
        .encode()
    )


def revert_paste_preferences(data):
    source = data.decode("utf-8")
    if source.count(PASTE_IMPORT + PASTE_ADDED_IMPORT) != 1 or source.count(PASTE_REPLACEMENT) != 1:
        raise ValueError("Unexpected adapted Code-OSS paste preferences")
    original = (
        source.replace(PASTE_IMPORT + PASTE_ADDED_IMPORT, PASTE_IMPORT)
        .replace(PASTE_REPLACEMENT, PASTE_PREFERENCE)
        .encode()
    )
    if adapt_paste_preferences(original) != data:
        raise ValueError("Adapted Code-OSS paste preferences drifted")
    return original
