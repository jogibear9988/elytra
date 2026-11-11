#!/usr/bin/env python3
"""Route exact pinned WebAudio leaves through reversible native autoplay edits.

Cargo retains its manifest, bindings and dependency graph. All uncomposed modules
continue to use upstream sources; no vendor source is changed.
"""

import argparse
import hashlib
import importlib.util
import json
from pathlib import Path
import re
import sys

# This script is also loaded by path by CPU policy tests; never depend on the
# caller's sys.path or import an unrelated package named "audio".
_audio_spec = importlib.util.spec_from_file_location(
    "_firefly_web_audio_composition", Path(__file__).with_name("audio.py")
)
_audio = importlib.util.module_from_spec(_audio_spec)
_audio_spec.loader.exec_module(_audio)
AUDIO_PINS, compose = _audio.PINS, _audio.compose
_ports_spec = importlib.util.spec_from_file_location(
    "_firefly_native_port_composition", Path(__file__).parent.parent / "rust-ports/observer.py"
)
_ports = importlib.util.module_from_spec(_ports_spec)
_ports_spec.loader.exec_module(_ports)


ROOT = Path(__file__).resolve().parents[2]
SOURCE = ROOT / "vendor/ladybird/Libraries/LibWeb/Bindings/Rust/src"
PINS = {
    "lib.rs": "87a59d6bbc27d74b83b07da15dddb84cda74e1e220a38f07d9edf3a0f4f60c59",
    "web_audio/mod.rs": "bfcbe81444c74d578d7861d711486534fe3c043e32b7895d51849e4084963b0e",
    **{f"web_audio/{name}": digest for name, digest in AUDIO_PINS.items()},
    **_ports.PINS,
}
ROOT_MODULES = frozenset(
    """
    rust_demangle rust_panic ak_format ffi_test_stubs bindings canvas_2d compression
    crypto css dom domurl encoding fetch file_api geometry html indexed_db
    notifications_api server_timing speech storage_api streams url_pattern
    web_assembly web_gl web_locks webidl webrtc xhr web_audio webgl_constants
    webgl_proxy webgl_wire_model
""".split()
)
AUDIO_MODULES = frozenset(
    """
    analyser_node audio_array audio_buffer audio_buffer_source_node audio_context
    audio_destination_node audio_listener audio_node audio_param audio_param_map
    audio_processing_event audio_scheduled_source_node audio_worklet_node
    audio_worklet_processor base_audio_context biquad_filter_node channel_merger_node
    channel_splitter_node constant_source_node convolver_node delay_node
    dynamics_compressor_node gain_node iir_filter_node media_element_audio_source_node
    media_stream_audio_destination_node media_stream_audio_source_node
    offline_audio_completion_event offline_audio_context oscillator_node panner_node
    periodic_wave script_processor_node stereo_panner_node wave_shaper_node
""".split()
)
HTML_MODULES = frozenset(
    "canvas_path color_stops predefined_color_space broadcast_channel canvas_compositing canvas_draw_image canvas_draw_path canvas_fill_stroke_styles canvas_filters canvas_gradient canvas_image_data canvas_image_smoothing canvas_path_drawing_styles canvas_pattern canvas_rect canvas_rendering_context2d canvas_rendering_context2d_settings canvas_settings canvas_shadow_styles canvas_state canvas_text canvas_text_drawing_styles canvas_transform exports image_bitmap image_data message_channel message_port numbers offscreen_canvas offscreen_canvas_rendering_context2d path2d port_channel storage storage_event structured_serialize text_metrics window window_or_worker_global_scope".split()
)
MODULE = re.compile(r'^(?:#\[path = "([^"\n]+)"\]\n)?((?:pub(?:\([^)]*\))? )?mod (\w+);)$', re.M)
FOOTER = "// Owned WebAudio routing build input: "


def digest(data):
    return hashlib.sha256(data).hexdigest()


def literal(path):
    value = str(path).replace("\\", "/")
    hashes = "#"
    while '"' + hashes in value:
        hashes += "#"
    return f'r{hashes}"{value}"{hashes}'


def module_source(original, base, expected, redirects):
    """Preserve every non-path byte and refuse unknown/ambiguous module routing."""
    if "#[path = r" in original or FOOTER in original:
        raise ValueError("Repeated WebAudio Rust module composition")
    seen, redirected, changes = set(), set(), []

    def replace(match):
        old_path, declaration, name = match.groups()
        if name in seen or name not in expected:
            raise ValueError(f"Unknown/duplicate WebAudio routing module: {name}")
        seen.add(name)
        target = (base / old_path).resolve() if old_path else base / name
        if not old_path:
            target = (
                target.with_suffix(".rs")
                if target.with_suffix(".rs").is_file()
                else target / "mod.rs"
            )
        target = target.resolve()
        if not target.is_file():
            raise ValueError(f"Missing upstream WebAudio routing module: {target}")
        # Explicit foo.rs paths change implicit child lookup. Such a module must
        # be a reviewed routing root itself; mod.rs retains its directory base.
        if (
            target.name not in {"mod.rs", "lib.rs"}
            and MODULE.search(target.read_text())
            and target not in redirects
        ):
            raise ValueError(f"Unreviewed nested Rust file-module routing: {target}")
        if target in redirects:
            redirected.add(target)
        selected = redirects.get(target, target)
        replacement = f"#[path = {literal(selected)}]\n{declaration}"
        changes.append((match.group(), replacement))
        return replacement

    generated = MODULE.sub(replace, original)
    if seen != set(expected) or redirected != set(redirects):
        raise ValueError("Missing/unknown WebAudio Rust module or redirect")
    restored = generated
    for before, after in reversed(changes):
        if restored.count(after) != 1:
            raise ValueError("Ambiguous WebAudio Rust module reversal")
        restored = restored.replace(after, before, 1)
    if restored != original:
        raise AssertionError("WebAudio routing changed bytes outside module paths")
    return generated


def input_fingerprint(inputs, chain, sources):
    """Bind actual generated routing/leaf bytes, not merely source paths or config."""
    manifest = {name: digest(data) for name, data in sources.items()}
    return digest(
        json.dumps({"inputs": inputs, "chain": chain, "sources": manifest}, sort_keys=True).encode()
    )


def prepare(output, driver, chain="", check=False):
    output, driver = output.resolve(), driver.absolute()
    inputs = {}
    for name, expected in PINS.items():
        path = SOURCE / name
        if digest(path.read_bytes()) != expected:
            raise ValueError(f"Review pinned WebAudio source: {path}")
        inputs[str(path.resolve())] = expected
    for name in ("prepare.py", "driver.py", "Driver.cpp", "audio.py", "../rust-ports/observer.py"):
        path = Path(__file__).parent / name
        inputs[str(path.resolve())] = digest(path.read_bytes())
    root = module_source(
        (SOURCE / "lib.rs").read_text(),
        SOURCE,
        ROOT_MODULES,
        {
            (SOURCE / "web_audio/mod.rs").resolve(): output / "web_audio.rs",
            (SOURCE / "html/mod.rs").resolve(): output / "html.rs",
            (SOURCE / "ffi_test_stubs.rs").resolve(): output / "ffi_test_stubs.rs",
        },
    )
    audio = module_source(
        (SOURCE / "web_audio/mod.rs").read_text(),
        SOURCE / "web_audio",
        AUDIO_MODULES,
        {(SOURCE / "web_audio" / name).resolve(): output / name for name in AUDIO_PINS},
    )
    html = module_source(
        (SOURCE / "html/mod.rs").read_text(),
        SOURCE / "html",
        HTML_MODULES,
        {(SOURCE / "html/message_port.rs").resolve(): output / "message_port.rs"},
    )
    planned = {
        "html.rs": html.encode(),
        "message_port.rs": _ports.compose(
            "html/message_port.rs", (SOURCE / "html/message_port.rs").read_text()
        ).encode(),
        "ffi_test_stubs.rs": _ports.compose(
            "ffi_test_stubs.rs", (SOURCE / "ffi_test_stubs.rs").read_text()
        ).encode(),
        "lib.rs": root.encode(),
        "web_audio.rs": audio.encode(),
        **{
            name: compose(name, (SOURCE / "web_audio" / name).read_text()).encode()
            for name in AUDIO_PINS
        },
    }
    provenance = input_fingerprint(inputs, chain, planned)
    planned["lib.rs"] += f"\n{FOOTER}{provenance}\n".encode()
    config = {
        "schema": 1,
        "kind": "firefly-rust-autoplay-driver",
        "original": str((SOURCE / "lib.rs").resolve()),
        "adapted": str(output / "lib.rs"),
        "driver": str(driver),
        "chain": chain,
        "files": inputs | {str(output / name): digest(data) for name, data in planned.items()},
    }
    planned["config.json"] = (json.dumps(config, indent=2) + "\n").encode()
    planned["DriverConfig.h"] = "".join(
        f"#define {key} {json.dumps(value, ensure_ascii=False)}\n"
        for key, value in {
            "FIREFLY_DRIVER_PYTHON": sys.executable,
            "FIREFLY_DRIVER_SCRIPT": str(Path(__file__).parent / "driver.py"),
            "FIREFLY_DRIVER_CONFIG": str(output / "config.json"),
        }.items()
    ).encode()
    # Validate the entire plan before touching any output, including check mode.
    if check:
        for name, data in planned.items():
            path = output / name
            if not path.is_file() or path.read_bytes() != data:
                raise ValueError(f"Generated WebAudio routing drift: {path}")
        return
    output.mkdir(parents=True, exist_ok=True)
    for name, data in planned.items():
        path = output / name
        if not path.exists() or path.read_bytes() != data:
            path.write_bytes(data)
    print("PASS pinned WebAudio Rust routing and reversible native admission edits")


if __name__ == "__main__":
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", required=True, type=Path)
    parser.add_argument("--driver", required=True, type=Path)
    parser.add_argument("--chain", default="")
    parser.add_argument("--check", action="store_true")
    args = parser.parse_args()
    prepare(args.output, args.driver, args.chain, args.check)
