#!/usr/bin/env python3
"""Prepare an audited Code-OSS source overlay without changing upstream files."""

import argparse
import hashlib
import json
from pathlib import Path

if __package__:
    from . import (
        window_preferences,
        css_development,
        disabled_updater,
        utility_auth,
        shared_process_debug,
        agent_host_debug,
        extension_host_flags,
    )
    from .engine import adapt_engine
    from .local_desktop import adapt_local, PATHS, OWNED, HELPER
else:
    import window_preferences
    import css_development
    import disabled_updater
    import utility_auth
    import shared_process_debug
    import agent_host_debug
    import extension_host_flags
    from engine import adapt_engine
    from local_desktop import adapt_local, PATHS, OWNED, HELPER

ROOT = Path(__file__).resolve().parents[2]
MANIFEST = ROOT / "vendor/code-oss-tests/manifest.json"
NODE_CHECK = "typeof process?.versions?.node === 'string'"
NATIVE_CHECK = (
    "(typeof process?.versions?.node === 'string' || typeof process?.versions?.mite === 'string')"
)
FILES = ("platform.ts", "process.ts")
CACHE_SOURCE = "firefly/compat/code-oss/compile-cache.ts"
CACHE_TARGET = "src/vs/base/node/elytraCompileCache.ts"
ENGINE_SOURCE = "firefly/compat/code-oss/engine-policy.ts"
ENGINE_TARGET = "src/elytraEnginePolicy.ts"


def adapt_cache(data):
    """Redirect engine-specific imports and guard unsupported optional modes."""
    source = data.decode("utf-8")
    replacements = {
        "import { constants, enableCompileCache, flushCompileCache, getCompileCacheDir } from 'node:module';": "import { constants, enableCompileCache, flushCompileCache, getCompileCacheDir } from './elytraCompileCache.js';",
        "import { getHeapCodeStatistics } from 'node:v8';": "import { assertLibJSCacheMode, getHeapCodeStatistics } from './elytraCompileCache.js';",
        "export function enableNodeCompileCache(kind: NodeCompileCacheKind): boolean {": "export function enableNodeCompileCache(kind: NodeCompileCacheKind): boolean {\n\tassertLibJSCacheMode('enable');",
        "export function markNodeCompileCacheReady(log?: (message: string) => void): void {": "export function markNodeCompileCacheReady(log?: (message: string) => void): void {\n\tassertLibJSCacheMode('ready');",
        "export async function waitForNodeCompileCacheReady(): Promise<void> {": "export async function waitForNodeCompileCacheReady(): Promise<void> {\n\tassertLibJSCacheMode('wait');",
    }
    for original, patched in replacements.items():
        if source.count(original) != 1:
            raise ValueError(f"Unexpected Code-OSS compile-cache seam: {original}")
        source = source.replace(original, patched)
    return source.encode("utf-8")


def sha256(data):
    return hashlib.sha256(data).hexdigest()


def adapt(name, data):
    """Change only declared identity checks and their matching TypeScript field."""
    source = data.decode("utf-8")
    if name not in FILES or source.count(NODE_CHECK) != 1:
        raise ValueError(f"Unexpected Code-OSS native identity seam: {name}")
    source = source.replace(NODE_CHECK, NATIVE_CHECK)
    if name == "platform.ts":
        field = "\t\tnode?: string;"
        if source.count(field) != 1:
            raise ValueError("Unexpected Code-OSS native process interface")
        source = source.replace(field, field + "\n\t\tmite?: string;")
    return source.encode("utf-8")


def prepare(source, output, *, libjs_cache=False, libjs_engine=False, local_desktop=False):
    """Validate every input before creating a fresh, explicitly partial overlay."""
    if local_desktop and not (libjs_cache and libjs_engine):
        raise ValueError("Local desktop requires --libjs-cache and --libjs-engine")
    source = Path(source).resolve(strict=True)
    output = Path(output).absolute()
    target = output.resolve()
    if target == source or source in target.parents or target in source.parents:
        raise ValueError("Overlay output must be separate from its source tree")
    if output.exists() or output.is_symlink():
        raise ValueError("Overlay output must not already exist")
    manifest = json.loads(MANIFEST.read_text())
    planned = {}
    records = []
    names = (
        *FILES,
        *(["nodeCompileCache.ts"] if libjs_cache else []),
        *(["mainImpl.ts"] if libjs_engine else []),
        *(PATHS if local_desktop else []),
        *(
            [
                window_preferences.SOURCE,
                window_preferences.PASTE_SOURCE,
                css_development.SOURCE,
                disabled_updater.SOURCE,
                utility_auth.SOURCE,
                shared_process_debug.SOURCE,
                agent_host_debug.SOURCE,
                extension_host_flags.SOURCE,
            ]
            if local_desktop
            else []
        ),
        "LICENSE.txt",
    )
    for name in names:
        entry = manifest["files"][name]
        original = (source / entry["upstream"]).read_bytes()
        # Upstream explicitly checks LICENSE.txt out with CRLF. Preserve either
        # exact pinned form; do not normalize sources or accept arbitrary drift.
        accepted = {entry["sha256"]}
        if name == "LICENSE.txt":
            accepted.add(entry["crlfCheckoutSha256"])
        if sha256(original) not in accepted:
            raise ValueError(f"Code-OSS source differs from pinned input: {entry['upstream']}")
        if name in FILES:
            patched = adapt(name, original)
        elif name == "nodeCompileCache.ts":
            patched = adapt_cache(original)
        elif name == "mainImpl.ts":
            patched = adapt_engine(original)
        elif name == window_preferences.SOURCE:
            patched = window_preferences.adapt_window_preferences(original)
        elif name == window_preferences.PASTE_SOURCE:
            patched = window_preferences.adapt_paste_preferences(original)
        elif name == utility_auth.SOURCE:
            patched = utility_auth.adapt_utility_auth(original)
        elif name == extension_host_flags.SOURCE:
            patched = extension_host_flags.adapt_extension_host_flags(original)
        elif name == agent_host_debug.SOURCE:
            patched = agent_host_debug.adapt_agent_host_debug(original)
        elif name == shared_process_debug.SOURCE:
            patched = shared_process_debug.adapt_shared_process_debug(original)
        elif name == css_development.SOURCE:
            patched = css_development.adapt_css_development(original)
        elif name == disabled_updater.SOURCE:
            patched = disabled_updater.adapt_disabled_updater(original)
        else:
            patched = original
        if local_desktop and (name == "mainImpl.ts" or name in PATHS):
            patched = adapt_local(name, patched)
        planned[entry["upstream"]] = patched
        records.append(
            {
                "path": entry["upstream"],
                "sourceSha256": sha256(original),
                "overlaySha256": sha256(patched),
            }
        )
    for enabled, owned, target in (
        (libjs_cache, CACHE_SOURCE, CACHE_TARGET),
        (libjs_engine, ENGINE_SOURCE, ENGINE_TARGET),
        (local_desktop, OWNED, HELPER),
        (local_desktop, window_preferences.HELPER_SOURCE, window_preferences.HELPER_TARGET),
        (local_desktop, css_development.HELPER_SOURCE, css_development.HELPER_TARGET),
        (local_desktop, utility_auth.HELPER_SOURCE, utility_auth.HELPER_TARGET),
        (local_desktop, shared_process_debug.HELPER_SOURCE, shared_process_debug.HELPER_TARGET),
        (local_desktop, agent_host_debug.HELPER_SOURCE, agent_host_debug.HELPER_TARGET),
        (local_desktop, extension_host_flags.HELPER_SOURCE, extension_host_flags.HELPER_TARGET),
    ):
        if enabled:
            helper = (ROOT / owned).read_bytes()
            planned[target] = helper
            records.append(
                {
                    "path": target,
                    "ownerSource": owned,
                    "sourceSha256": sha256(helper),
                    "overlaySha256": sha256(helper),
                }
            )
    report = {
        "schema": 1,
        "kind": "partial-source-overlay",
        "repository": manifest["repository"],
        "revision": manifest["revision"],
        "adaptations": [
            "Recognize process.versions.mite as native without claiming Node or Electron identity"
        ],
        "files": records,
    }
    if libjs_cache:
        report["adaptations"].append(
            "Disable optional Node/V8 cache on LibJS; explicitly refuse generation, measurements and readiness waits"
        )
    if libjs_engine:
        report["adaptations"].append(
            "Separate audited implicit Chromium/Blink defaults from LibJS/LibWeb; refuse unsupported user engine/sandbox requests before mutations"
        )
    if local_desktop:
        report["adaptations"].append(
            "Explicit experimental local desktop: ELYTRA_CODE_OSS_LOCAL_DESKTOP=1 disables webviews, remote resources, display capture and engine caches; omit disabled V8 window cache and unavailable deprecated synchronous script paste preferences after verified LibJS/local/engine policy; native webContents.paste remains needed; disable optional utility authentication forwarding and the implicit development shared-process, agent-host and extension-host inspectors with immutable unavailable status; extension-host implicit DNS ordering/network-inspection defaults are omitted, never claimed applied; unsigned-library relaxation is unavailable and opted out; disable optional TouchBar initialization and explicitly refuse updates, no clipboard/workbench acceptance; local CSS discovery reads compiler-inventoried actual staged stylesheets, not a user-file search replacement; skip unavailable Darwin autoUpdater subscriptions only after genuine disabled state"
        )
    output.mkdir(parents=True)
    for name, data in planned.items():
        destination = output / name
        destination.parent.mkdir(parents=True, exist_ok=True)
        destination.write_bytes(data)
    (output / "ELYTRA-ADAPTATIONS.json").write_text(
        json.dumps(report, indent=2) + "\n", encoding="utf-8"
    )
    return report


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True, help="Pinned Code-OSS checkout")
    parser.add_argument("--output", type=Path, required=True, help="Fresh partial source overlay")
    parser.add_argument(
        "--libjs-cache", action="store_true", help="Explicitly disable the optional Node/V8 cache"
    )
    parser.add_argument(
        "--libjs-engine",
        action="store_true",
        help="Explicitly separate implicit Chromium tuning from LibJS and reject unsupported user engine options",
    )
    parser.add_argument(
        "--local-desktop",
        action="store_true",
        help="Prepare explicit limited local desktop experiment (requires cache and engine adaptations)",
    )
    args = parser.parse_args()
    prepare(
        args.source,
        args.output,
        libjs_cache=args.libjs_cache,
        libjs_engine=args.libjs_engine,
        local_desktop=args.local_desktop,
    )
    print(f"Prepared audited Code-OSS overlay in {args.output}")


if __name__ == "__main__":
    main()
