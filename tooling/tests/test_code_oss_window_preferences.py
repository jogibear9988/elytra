#!/usr/bin/env python3
"""Verify the exact Code-OSS cache-option seam and run its helper in genuine Node and Mite."""

import _support

import argparse
import hashlib
import json
import os
from pathlib import Path
import shutil
import subprocess
import tempfile

from code_oss.local_desktop import PATHS
from code_oss.overlay import FILES, prepare
from code_oss.window_preferences import (
    SOURCE,
    SOURCE_SHA256,
    IMPORT,
    ADDED_IMPORT,
    PREFERENCE,
    REPLACEMENT,
    TOUCHBAR_CREATE,
    TOUCHBAR_SKIP,
    TOUCHBAR_UPDATE,
    TOUCHBAR_CHECK,
    HELPER_SOURCE,
    HELPER_TARGET,
    adapt_window_preferences,
    revert_window_preferences,
    FIRST_MOUSE_TRUE,
    FIRST_MOUSE_FALSE,
    FIRST_MOUSE_TRUE_REPLACEMENT,
    FIRST_MOUSE_FALSE_REPLACEMENT,
    PASTE_SOURCE,
    PASTE_SHA256,
    PASTE_IMPORT,
    PASTE_ADDED_IMPORT,
    PASTE_PREFERENCE,
    PASTE_REPLACEMENT,
    adapt_paste_preferences,
    revert_paste_preferences,
)

ROOT = Path(__file__).resolve().parents[2]
PINNED = ROOT / "vendor/code-oss-tests"


def rejected(action):
    try:
        action()
    except ValueError:
        return
    raise AssertionError("Pinned window-preference adaptation accepted drift")


def projection(data, name):
    source = data.decode("utf-8")
    start = source.index("\t\t\tconst webPreferences: electron.WebPreferences = {")
    end = source.index("\n\t\t\tconst options = instantiationService.invokeFunction(", start)
    return (
        f"export function {name}(FileAccess, config) {{\n"
        + source[start:end]
        + "\nreturn webPreferences;\n}\n"
    )


def paste_projection(data, name):
    source = data.decode("utf-8")
    start = source.index("\t\twebPreferences: {") + len("\t\twebPreferences: ")
    end = source.index("\n\t\t},", start) + len("\n\t\t}")
    return (
        f"export function {name}(webPreferences, windowState, windowSettings, zoomLevelToZoomFactor) {{\nreturn "
        + source[start:end]
        + ";\n}\n"
    )


def touchbar_projection(data, suffix):
    source = data.decode("utf-8")
    creation = TOUCHBAR_SKIP if TOUCHBAR_SKIP in source else TOUCHBAR_CREATE
    start = source.index(TOUCHBAR_UPDATE) + len(TOUCHBAR_UPDATE)
    end = source.index("\n\tprivate createTouchBar():", start)
    return (
        f"export function touchBarCreate{suffix}() {{\n{creation}\n}}\n"
        + f"export function touchBarUpdate{suffix}(groups, isMacintosh) {{"
        + source[start:end]
        + "\n"
    )


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--mite", type=Path, required=True)
    parser.add_argument("--node", default=shutil.which("node"))
    parser.add_argument(
        "--offscreen", type=Path, help="Optional exact offscreen binary in standalone Mite role"
    )
    args = parser.parse_args()
    assert args.node, "Pinned Node build/reference runtime required"
    manifest = json.loads((PINNED / "manifest.json").read_text())
    record = manifest["files"][SOURCE]
    original = (PINNED / SOURCE).read_bytes()
    assert record["upstream"] == "src/vs/platform/windows/electron-main/windowImpl.ts"
    assert hashlib.sha256(original).hexdigest() == record["sha256"] == SOURCE_SHA256
    adapted = adapt_window_preferences(original)
    assert adapted == original.replace(IMPORT.encode(), (IMPORT + ADDED_IMPORT).encode()).replace(
        PREFERENCE.encode(), REPLACEMENT.encode()
    ).replace(TOUCHBAR_CREATE.encode(), TOUCHBAR_SKIP.encode()).replace(
        TOUCHBAR_UPDATE.encode(), TOUCHBAR_CHECK.encode()
    )
    assert revert_window_preferences(adapted) == original
    assert original.split(IMPORT.encode())[0] == adapted.split(IMPORT.encode())[0], (
        "Pinned license bytes preserved"
    )
    for data in [
        original + b"\n",
        original + IMPORT.encode(),
        original + PREFERENCE.encode(),
        original.replace(b"Copyright", b"copyright", 1),
        adapted,
    ]:
        rejected(lambda data=data: adapt_window_preferences(data))
    for data in [
        original,
        adapted + b"\n",
        adapted + ADDED_IMPORT.encode(),
        adapted + REPLACEMENT.encode(),
        adapted.replace(b"Copyright", b"copyright", 1),
    ]:
        rejected(lambda data=data: revert_window_preferences(data))
    print(
        "PASS exact pinned window preference bytes reversible license preservation and duplicate drift refusal",
        flush=True,
    )

    paste_original = (PINNED / PASTE_SOURCE).read_bytes()
    assert hashlib.sha256(paste_original).hexdigest() == PASTE_SHA256
    assert manifest["files"][PASTE_SOURCE]["sha256"] == PASTE_SHA256
    paste_adapted = adapt_paste_preferences(paste_original)
    assert paste_adapted == paste_original.replace(
        PASTE_IMPORT.encode(), (PASTE_IMPORT + PASTE_ADDED_IMPORT).encode()
    ).replace(PASTE_PREFERENCE.encode(), PASTE_REPLACEMENT.encode()).replace(
        FIRST_MOUSE_TRUE.encode(), FIRST_MOUSE_TRUE_REPLACEMENT.encode()
    ).replace(FIRST_MOUSE_FALSE.encode(), FIRST_MOUSE_FALSE_REPLACEMENT.encode())
    assert revert_paste_preferences(paste_adapted) == paste_original
    for data in [paste_original + b"\n", paste_original + PASTE_PREFERENCE.encode(), paste_adapted]:
        rejected(lambda data=data: adapt_paste_preferences(data))
    for data in [paste_original, paste_adapted + b"\n", paste_adapted + PASTE_REPLACEMENT.encode()]:
        rejected(lambda data=data: revert_paste_preferences(data))
    print("PASS exact pinned legacy-paste omission reversal and drift refusal", flush=True)

    with tempfile.TemporaryDirectory(prefix="elytra-window-preferences-") as temporary:
        work = Path(temporary)
        source = work / "source"
        names = {
            *FILES,
            *PATHS,
            SOURCE,
            PASTE_SOURCE,
            "utility-process-main.ts",
            "css-development-service.ts",
            "darwin-update-service.ts",
            "mainImpl.ts",
            "nodeCompileCache.ts",
            "LICENSE.txt",
        }
        for name in names:
            data = (PINNED / name).read_bytes()
            entry = manifest["files"][name]
            assert hashlib.sha256(data).hexdigest() == entry["sha256"]
            target = source / entry["upstream"]
            target.parent.mkdir(parents=True, exist_ok=True)
            target.write_bytes(data)
        forbidden = work / "unapproved-overlay"
        rejected(lambda: prepare(source, forbidden, local_desktop=True))
        assert not forbidden.exists()
        overlay = work / "overlay"
        report = prepare(source, overlay, libjs_cache=True, libjs_engine=True, local_desktop=True)
        assert (overlay / record["upstream"]).read_bytes() == adapted
        assert (source / record["upstream"]).read_bytes() == original
        assert (overlay / manifest["files"][PASTE_SOURCE]["upstream"]).read_bytes() == paste_adapted
        assert (overlay / HELPER_TARGET).read_bytes() == (ROOT / HELPER_SOURCE).read_bytes()
        sources = [item for item in report["files"] if item["path"] == record["upstream"]]
        assert len(sources) == 1 and sources[0]["sourceSha256"] == SOURCE_SHA256
        assert sources[0]["overlaySha256"] == hashlib.sha256(adapted).hexdigest()
        helpers = [item for item in report["files"] if item["path"] == HELPER_TARGET]
        assert len(helpers) == 1 and helpers[0]["ownerSource"] == HELPER_SOURCE
        rejected(
            lambda: prepare(
                source, overlay, libjs_cache=True, libjs_engine=True, local_desktop=True
            )
        )
        plain = work / "plain-overlay"
        plain_report = prepare(source, plain)
        assert not any(
            item["path"] in {HELPER_TARGET, record["upstream"]} for item in plain_report["files"]
        )
        (source / record["upstream"]).write_bytes(original + b"\n")
        drift = work / "drift-overlay"
        rejected(
            lambda: prepare(source, drift, libjs_cache=True, libjs_engine=True, local_desktop=True)
        )
        assert not drift.exists(), "Validate all pinned inputs before writing a partial output"
        print(
            "PASS explicit local desktop overlay provenance fresh output and prewrite rejection preserve upstream source",
            flush=True,
        )

        shutil.copyfile(ROOT / HELPER_SOURCE, work / "window-preferences.ts")
        expressions = (
            "import { libJSWindowCachePreferences, libJSWindowPastePreferences, skipLibJSTouchBar, assertLibJSTouchBarSupported } from './window-preferences.mjs';\n"
            + projection(original, "originalPreferences")
            + projection(adapted, "adaptedPreferences")
            + paste_projection(paste_original, "originalPastePreferences")
            + paste_projection(paste_adapted, "adaptedPastePreferences")
            + touchbar_projection(original, "Original")
            + touchbar_projection(adapted, "Adapted")
        )
        (work / "expressions.ts").write_text(expressions)
        shutil.copyfile(
            ROOT / "firefly/tests/code-oss-window-preferences/main.mjs", work / "main.mjs"
        )
        transform = """
import fs from 'node:fs'; import { pathToFileURL } from 'node:url';
const [directory, compiler] = process.argv.slice(1);
const {transformSync, version} = await import(pathToFileURL(compiler));
if(version !== '0.25.12') throw Error('Unexpected pinned TypeScript build transform');
for(const name of ['window-preferences', 'expressions']) {
  fs.writeFileSync(`${directory}/${name}.mjs`, transformSync(fs.readFileSync(`${directory}/${name}.ts`, 'utf8'), {loader:'ts',format:'esm',target:'es2024'}).code);
}
"""
        subprocess.run(
            [
                args.node,
                "--input-type=module",
                "-e",
                transform,
                str(work),
                str(ROOT / "tooling/runtime-js/node_modules/esbuild/lib/main.js"),
            ],
            check=True,
            capture_output=True,
            text=True,
            timeout=20,
        )
        cases = [
            ("valid", False, False, "1", None),
            ("valid", False, True, "1", None),
            ("valid", True, False, "1", None),
            ("valid", True, True, "1", None),
        ]
        cases += [
            (policy, False, False, "1", None)
            for policy in [
                "missing-local",
                "missing-engine",
                "disabled-local",
                "wrong-local-scope",
                "cache-not-disabled",
                "wrong-engine",
                "wrong-engine-scope",
                "conflict",
                "preexisting-matching",
            ]
        ]
        cases += [("valid", False, False, flag, None) for flag in [None, "0", "2", "true"]]
        cases.append(("valid", False, False, "1", "/owned/requested-code-cache"))
        for policy, use_cache, sessions, flag, cache_path in cases:
            fixture = {"policy": policy, "useCodeCache": use_cache, "sessions": sessions}
            for label, binary in [("Node", args.node), ("Mite", args.mite.resolve())]:
                env = dict(os.environ)
                for key in ["ELYTRA_CODE_OSS_LOCAL_DESKTOP", "VSCODE_CODE_CACHE_PATH"]:
                    env.pop(key, None)
                if flag is not None:
                    env["ELYTRA_CODE_OSS_LOCAL_DESKTOP"] = flag
                if cache_path is not None:
                    env["VSCODE_CODE_CACHE_PATH"] = cache_path
                result = subprocess.run(
                    [str(binary), str(work / "main.mjs"), json.dumps(fixture)],
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert result.returncode == 0 and not result.stderr, (
                    label,
                    fixture,
                    flag,
                    cache_path,
                    result,
                )
                outcome = json.loads(result.stdout)
                assert outcome["runtime"] == label
                allowed = policy == "valid" and not use_cache and flag == "1" and not cache_path
                assert outcome["accepted"] == (label == "Node" or allowed), (
                    label,
                    fixture,
                    outcome,
                )
                assert outcome["omitted"] == (label == "Mite" and allowed), (
                    label,
                    fixture,
                    outcome,
                )
        print(
            f"PASS genuine Node/Mite pinned preference expression {len(cases)} scenarios preserve accessor order security identities and explicit cache refusal",
            flush=True,
        )

        # The actual offscreen executable can run this leaf policy without Qt or
        # creating a window through its existing standalone-Mite role.
        (work / "offscreen.mjs").write_text("""
import assert from 'node:assert/strict';
process._vscodeLibJSLocalDesktopStatus={enabled:true,scope:'experimental-local-desktop',disabled:['engine-code-cache']};
process._vscodeLibJSEngineStatus={engine:'LibJS/LibWeb',scope:'implicit-code-oss-engine-tuning-only'};
const mode=process.argv[2];
if(mode==='bad-policy')delete process._vscodeLibJSEngineStatus;
const {libJSFirstMousePreferences:f}=await import('./window-preferences.mjs');
const node=typeof process.versions.node==='string'||typeof process.versions.electron==='string';
const expected=process.argv[3];
for(const value of [true,false]) {
 if(!node&&expected==='reject')assert.throws(()=>f(value),e=>e.code==='ERR_CODE_OSS_LIBJS_OFFSCREEN_DIAGNOSTIC');
 else assert.deepEqual(f(value),!node&&expected==='omit'?{}:{acceptFirstMouse:value});
}
const status=process._vscodeLibJSOffscreenDiagnosticStatus;
if(!node&&expected==='omit'){
 assert.equal(status.desktopAccepted,false);assert.equal(status.omittedInputPolicy,'acceptFirstMouse');
 assert.equal(status.scope,'code-oss-test-only-offscreen-diagnostic');assert.ok(Object.isFrozen(status));
 const d=Object.getOwnPropertyDescriptor(process,'_vscodeLibJSOffscreenDiagnosticStatus');
 assert.equal(d.writable,false);assert.equal(d.configurable,false);assert.equal(d.enumerable,false);
 assert.equal(f(true).acceptFirstMouse,undefined);assert.equal(process._vscodeLibJSOffscreenDiagnosticStatus,status);
}else assert.equal(status,undefined);
console.log('PASS test-only offscreen first-mouse policy');
""")
        frontends = [(args.node, False), (str(args.mite.resolve()), False)]
        if args.offscreen:
            frontends.append((str(args.offscreen.resolve()), True))
        scenarios = [
            (None, "offscreen", "valid"),
            ("1", "offscreen", "valid"),
            ("0", "offscreen", "valid"),
            ("1", "cocoa", "valid"),
            ("1", "offscreen", "bad-policy"),
        ]
        for binary, offscreen in frontends:
            for flag, platform, mode in scenarios:
                env = dict(os.environ)
                env.pop("ELYTRA_CODE_OSS_OFFSCREEN_DIAGNOSTIC", None)
                env.pop("VSCODE_CODE_CACHE_PATH", None)
                env["ELYTRA_CODE_OSS_LOCAL_DESKTOP"] = "1"
                env["ELECTRON_RUN_AS_NODE"] = "1"
                env["QT_QPA_PLATFORM"] = platform
                if flag is not None:
                    env["ELYTRA_CODE_OSS_OFFSCREEN_DIAGNOSTIC"] = flag
                expected = (
                    "preserve"
                    if flag is None
                    else "omit"
                    if offscreen and flag == "1" and platform == "offscreen" and mode == "valid"
                    else "reject"
                )
                result = subprocess.run(
                    [binary, str(work / "offscreen.mjs"), mode, expected],
                    env=env,
                    capture_output=True,
                    text=True,
                    timeout=15,
                )
                assert (
                    result.returncode == 0
                    and not result.stderr
                    and result.stdout.strip() == "PASS test-only offscreen first-mouse policy"
                ), result
        print(
            "PASS genuine Node/Mite first-mouse preservation and explicit diagnostic refusal"
            + (
                " plus exact offscreen omission"
                if args.offscreen
                else "; exact offscreen positive pending"
            ),
            flush=True,
        )


if __name__ == "__main__":
    main()
