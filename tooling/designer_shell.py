"""Record and verify the maintained Firefly shell around the pinned Designer demo."""

import hashlib
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
EXAMPLE = ROOT / "examples/web-component-designer"
SOURCE_FILES = {
    "main.mjs": "main.mjs",
    "server.mjs": "server.mjs",
    "smoke.mjs": "smoke.mjs",
    "upstream.json": "upstream.json",
    "web/window-controls.mjs": "window-controls.mjs",
    "web/window-controls.css": "window-controls.css",
}
HEAD_OVERLAY = '<link rel="stylesheet" href="./window-controls.css">\n<script type="module" src="./window-controls.mjs"></script>\n'
CONTROLS_OVERLAY = """<div class="designer-window-controls" aria-label="Window controls">
      <button id="window-minimize" type="button" aria-label="Minimize window" title="Minimize window">−</button>
      <button id="window-maximize" type="button" aria-label="Maximize window" title="Maximize window">□</button>
      <button id="window-close" type="button" aria-label="Close window" title="Close window">×</button>
    </div>"""


def digest(data):
    return hashlib.sha256(data).hexdigest()


def physical_bytes(file):
    if file.is_symlink() or not file.is_file():
        raise ValueError("Designer shell requires a physical file: " + str(file))
    return file.read_bytes()


def apply_index_overlay(html):
    """Apply the reviewed toolbar exactly once; source drift must stop the build."""
    if html.count("</head>") != 1 or html.count("</nav>") != 1:
        raise ValueError("Review Designer toolbar overlay against upstream HTML")
    if "window-controls" in html or "window-minimize" in html:
        raise ValueError("Designer toolbar overlay is already present or conflicts with upstream")
    return html.replace("</head>", HEAD_OVERLAY + "</head>").replace(
        "</nav>", CONTROLS_OVERLAY + "</nav>"
    )


def stage_index_overlay(file):
    """Write deterministic UTF-8/LF bytes, including on native Windows."""
    file = Path(file)
    file.write_bytes(apply_index_overlay(file.read_text(encoding="utf-8")).encode("utf-8"))


def check_index(application):
    file = application / "web/index.html"
    data = physical_bytes(file)
    html = data.decode("utf-8")
    for fragment in [HEAD_OVERLAY, CONTROLS_OVERLAY]:
        if html.count(fragment) != 1:
            raise ValueError("Designer index does not contain the current owned toolbar overlay")
    for name in ["window-minimize", "window-maximize", "window-close"]:
        if html.count('id="' + name + '"') != 1:
            raise ValueError("Designer index has missing/duplicate window controls: " + name)
    return digest(data)


def shell_metadata(application, example=EXAMPLE):
    """Hash actual staged bytes, requiring equality with current maintained sources."""
    application, example = Path(application), Path(example)
    files = {}
    for target, source in SOURCE_FILES.items():
        expected = digest(physical_bytes(example / source))
        if digest(physical_bytes(application / target)) != expected:
            raise ValueError("Stale Designer shell; rebuild application: " + target)
        files[target] = expected
    overlay = json.dumps([HEAD_OVERLAY, CONTROLS_OVERLAY], ensure_ascii=False).encode("utf-8")
    return {
        "schema": 1,
        "files": files,
        "overlaySha256": digest(overlay),
        "indexSha256": check_index(application),
    }


def repository(value):
    return value.get("repository") if isinstance(value, dict) else None


def verify_designer_shell(application, *, required=False, example=EXAMPLE):
    """Reject stale Designer payloads before a packager creates its output directory.

    Generic application packagers remain usable for other apps. Designer's lock,
    upstream pin or shell record identifies this maintained sample independently
    of the caller's chosen application name.
    """
    application, example = Path(application), Path(example)
    expected_pin = json.loads((example / "upstream.json").read_text())
    manifest = application / "BUILD.json"
    identified = required or (application / "designer-package-lock.json").exists()
    pin = application / "upstream.json"
    if pin.is_file():
        identified |= repository(json.loads(pin.read_text())) == repository(expected_pin)
    if not manifest.is_file():
        if identified:
            raise ValueError("Designer shell BUILD.json is missing; rebuild application")
        return None
    if manifest.stat().st_size > 1024 * 1024:
        raise ValueError("Application BUILD.json exceeds the metadata budget")
    metadata = json.loads(physical_bytes(manifest))
    if not isinstance(metadata, dict):
        raise ValueError("Application BUILD.json must contain an object")
    identified |= "designerShell" in metadata or repository(metadata.get("upstream")) == repository(
        expected_pin
    )
    if not identified:
        return None
    if metadata.get("runtime") != "firefly" or metadata.get("webRoot") != "web":
        raise ValueError("Designer build runtime/web root differs; rebuild application")
    expected = shell_metadata(application, example)
    if metadata.get("designerShell") != expected:
        raise ValueError("Designer shell metadata is stale/missing/mismatched; rebuild application")
    if metadata.get("upstream") != expected_pin:
        raise ValueError("Designer upstream metadata differs from the maintained pin")
    return expected
