"""Check symbol/version requirements before selecting one declared ELF provider.

This is a loader ABI check, not proof of arbitrary semantic compatibility. Callers
must declare the provider policy and run their relocated application gates too.
"""

from __future__ import annotations

from dataclasses import dataclass
from pathlib import Path
import re


@dataclass(frozen=True)
class Symbol:
    name: str
    version: str | None
    default: bool
    undefined: bool
    kind: str
    binding: str
    visibility: str


def parse_symbols(output):
    symbols = []
    found_table = False
    for line in output.splitlines():
        line = line.strip()
        if line.startswith("Symbol table '.dynsym'"):
            found_table = True
            continue
        if not line or line.startswith("Num:"):
            continue
        fields = line.split()
        if len(fields) == 7 and fields[0] == "0:" and fields[6] == "UND":
            continue  # ELF's reserved empty symbol.
        if len(fields) < 8 or not re.fullmatch(r"\d+:", fields[0]):
            raise ValueError("Unrecognized ELF dynamic symbol entry: " + line)
        match = re.fullmatch(r"([^@\s]+)(?:(@@?)([^@\s]+))?", fields[7])
        if not match:
            raise ValueError("Ambiguous ELF symbol/version: " + fields[7])
        name, separator, version = match.groups()
        symbols.append(
            Symbol(
                name, version, separator != "@", fields[6] == "UND", fields[3], fields[4], fields[5]
            )
        )
    if not found_table:
        raise ValueError("Missing ELF dynamic symbol table")
    return symbols


def symbols(path, command):
    return parse_symbols(command("readelf", "--dyn-syms", "--wide", path))


def exported(symbol):
    return (
        not symbol.undefined
        and symbol.binding in {"GLOBAL", "WEAK", "UNIQUE"}
        and symbol.visibility in {"DEFAULT", "PROTECTED"}
    )


def label(name, version):
    return name if version is None else name + "@" + version


def compatible_kind(required, provided):
    return required == "NOTYPE" or required == provided or {required, provided} <= {"FUNC", "IFUNC"}


def validate_provider(name, provider, alternatives, consumers, command):
    """Require all used exports of the original providers in the selected provider.

    Exact versioned imports must remain available; an unversioned import requires
    a default/unversioned export. Check actual selected consumers, including the
    independently launched executables and plugin DSOs, before copying anything.
    """
    provider = Path(provider)
    available = {}
    original_names = set()
    for path in alternatives:
        if command("patchelf", "--print-soname", path).strip() != name:
            raise ValueError(f"ELF provider SONAME differs from policy: {name}: {path}")
        for symbol in symbols(path, command):
            if exported(symbol):
                original_names.add(symbol.name)
                if path == provider:
                    available.setdefault((symbol.name, symbol.version), set()).add(symbol.kind)
                    if symbol.default:
                        available.setdefault((symbol.name, None), set()).add(symbol.kind)
    requirements = {}
    for consumer in sorted(set(consumers)):
        required = set()
        for symbol in symbols(consumer, command):
            if not symbol.undefined or symbol.name not in original_names:
                continue
            entry = label(symbol.name, symbol.version)
            kinds = available.get((symbol.name, symbol.version), set())
            if not any(compatible_kind(symbol.kind, kind) for kind in kinds):
                raise ValueError(f"ELF provider lacks required ABI: {name}: {entry}: {consumer}")
            required.add(entry)
        if required:
            requirements[str(consumer)] = sorted(required)
    if not requirements:
        raise ValueError(f"ELF provider policy has no verifiable consumer imports: {name}")
    return requirements
