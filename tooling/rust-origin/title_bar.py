"""Compose document-scoped titlebar environment lengths into the pinned CSS resolver."""

import os
from pathlib import Path
import re

ROOT = Path(__file__).resolve().parents[2]
UPSTREAM = Path(os.environ.get("FIREFLY_LADYBIRD_RUST_SOURCE", ROOT / "vendor/ladybird"))
SOURCE = UPSTREAM / "Libraries/LibWeb/Rust/src"
PINS = {
    "lib.rs": "ad8a0fd5b5bd4a5896652ff302397e08ec07bf71850d7b7a7b491344a8f914a9",
    "css/mod.rs": "91f0d31f1fec42804072e3206df3654348f53df9073832ba8e7cac8525cc674c",
    "css/custom_properties.rs": "df7030ea1bb569889022f09d1d34cc368200d2954a008604186bfad7a176de80",
    "css/parser/mod.rs": "0d7692324c7d61389613d058d86fe47c72a8738a23d25a92be42ccce4314bd06",
    "css/parser/query_parser.rs": "39ea86deea73d10460e5c11edd6a37293677e80d9887e50697f807d645e89714",
}
MODULE = re.compile(r'^(?:#\[path = "([^"\n]+)"\]\n)?((?:pub(?:\([^)]*\))? )?mod (\w+);)$', re.M)

NAME_LOOKUP = """fn titlebar_area_component(name: &[u16]) -> Option<usize> {
    ["titlebar-area-x", "titlebar-area-y", "titlebar-area-width", "titlebar-area-height"]
        .iter()
        .position(|component| name.iter().copied().eq(component.bytes().map(u16::from)))
}

"""

VALUE_LOOKUP = """    if indices.is_empty()
        && let Some(component) = titlebar_area_component(name)
        && let Some(length) = context.media_environment.and_then(|environment| environment.titlebar_area_length(component))
    {
        return TokenResolution::from_source(format!("{length}px").as_bytes());
    }

"""

MEDIA_ACCESSOR = """    // Firefly appends the four CSS-pixel lengths to the ordinary document media snapshot.
    // The retained style engine copies that snapshot, including for worker-thread style computation.
    pub(crate) fn titlebar_area_length(&self, component: usize) -> Option<f64> {
        let (values, _) = self.parts()?;
        let lengths = values.get(MEDIA_FEATURES.len()..)?;
        if lengths.len() != 4 {
            return None;
        }
        let value = lengths.get(component)?;
        (value.kind == FfiMediaFeatureValueKind::Length).then_some(value.value)
    }

"""


def replace(source, before, after):
    if source.count(before) != 1:
        raise ValueError(f"Review titlebar environment seam: {before!r}")
    result = source.replace(before, after, 1)
    if result.count(after) != 1 or result.replace(after, before, 1) != source:
        raise ValueError("Titlebar environment composition is not reversible")
    return result


def route(source, path, redirects, literal, output, routed):
    selected = set()
    changes = []

    def module(match):
        old_path, declaration, name = match.groups()
        base = path.parent
        if old_path:
            target = (base / old_path).resolve()
        else:
            if path.name not in {"mod.rs", "lib.rs"}:
                base /= path.stem
            file = base / f"{name}.rs"
            target = file if file.is_file() else base / name / "mod.rs"
        if not target.is_file():
            raise ValueError(f"Missing upstream CSS module: {target}")
        if name in redirects:
            if name in selected or target != SOURCE / redirects[name][0]:
                raise ValueError(f"Review titlebar CSS module routing: {target}")
            selected.add(name)
            target = redirects[name][1]
        elif not old_path and target.suffix == ".rs" and target.with_suffix("").is_dir():
            contents = target.read_text(encoding="utf-8")
            if MODULE.search(contents):
                # An explicit #[path] drops rustc's implicit <stem>/ child directory.
                # Route file parents recursively using their original implicit paths.
                name = "routed-" + target.relative_to(SOURCE).as_posix().replace("/", "--")
                if name not in routed:
                    routed[name] = route(contents, target, {}, literal, output, routed)
                target = output / name
        replacement = f"#[path = {literal(target)}]\n{declaration}"
        changes.append((match.group(), replacement))
        return replacement

    result = MODULE.sub(module, source)
    if selected != redirects.keys():
        raise ValueError("Missing titlebar CSS module")
    restored = result
    for before, after in reversed(changes):
        if restored.count(after) != 1:
            raise ValueError("Ambiguous titlebar CSS module reversal")
        restored = restored.replace(after, before, 1)
    if restored != source:
        raise ValueError("Titlebar CSS module routing changed non-path bytes")
    return result


def compose(output, literal):
    def source(name):
        return (SOURCE / name).read_text(encoding="utf-8")

    known = "pub(crate) fn environment_variable_is_known(name: &[u16]) -> bool {\n"
    substitution = "    let variable = ENVIRONMENT_VARIABLES\n"
    parts = "    /// The feature values and the length context, or none"
    custom_properties = replace(
        source("css/custom_properties.rs"),
        known,
        NAME_LOOKUP
        + known
        + "    if titlebar_area_component(name).is_some() {\n        return true;\n    }\n",
    )
    routed = {}
    planned = {
        "web.rs": route(
            source("lib.rs"),
            SOURCE / "lib.rs",
            {
                "css": ("css/mod.rs", output / "css.rs"),
            },
            literal,
            output,
            routed,
        ),
        "css.rs": route(
            source("css/mod.rs"),
            SOURCE / "css/mod.rs",
            {
                "custom_properties": ("css/custom_properties.rs", output / "custom_properties.rs"),
                "parser": ("css/parser/mod.rs", output / "parser.rs"),
            },
            literal,
            output,
            routed,
        ),
        "parser.rs": route(
            source("css/parser/mod.rs"),
            SOURCE / "css/parser/mod.rs",
            {
                "query_parser": ("css/parser/query_parser.rs", output / "query_parser.rs"),
            },
            literal,
            output,
            routed,
        ),
        "custom_properties.rs": replace(
            custom_properties, substitution, VALUE_LOOKUP + substitution
        ),
        "query_parser.rs": replace(
            source("css/parser/query_parser.rs"), parts, MEDIA_ACCESSOR + parts
        ),
    }
    return planned | routed
