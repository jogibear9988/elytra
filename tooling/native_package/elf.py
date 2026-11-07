"""Stage a checked ELF dependency closure for native Linux x64 development packages.

Only trusted outputs from our native build are inspected with ldd. Keep the host
loader/glibc ABI external; copy other resolved dependencies once into lib/ and
rewrite each owned object's RUNPATH relative to itself. This is not an application
packager: callers must provide resources, dlopen plugins and dependency notices.
"""

import hashlib
import os
from pathlib import Path
import platform
import re
import shutil
import stat
import subprocess
import sys

# These are host ABI requirements, not copied application libraries. Additional
# native/plugin dependencies must be supplied explicitly rather than hidden here.
HOST_ABI = frozenset(
    {
        "libc.so.6",
        "libm.so.6",
        "libdl.so.2",
        "libpthread.so.0",
        "librt.so.1",
        "libutil.so.1",
        "libanl.so.1",
        "libresolv.so.2",
        "ld-linux-x86-64.so.2",
    }
)


def digest(path):
    result = hashlib.sha256()
    with Path(path).open("rb") as file:
        for chunk in iter(lambda: file.read(1024 * 1024), b""):
            result.update(chunk)
    return result.hexdigest()


def native_tools():
    """Fail before mutation or loader execution on unverified hosts."""
    if sys.platform != "linux" or platform.machine() != "x86_64":
        raise RuntimeError("ELF closure staging requires native Linux x64")
    for name in ["ldd", "patchelf", "readelf"]:
        if not shutil.which(name):
            raise RuntimeError(f"Missing native packaging tool: {name}")


def command(*args):
    # Loader environment overrides would make a successful dependency check
    # depend on the caller's shell instead of the package's recorded RUNPATH.
    env = {key: value for key, value in os.environ.items() if not key.startswith("LD_")}
    return subprocess.check_output(
        list(map(str, args)),
        env={**env, "LC_ALL": "C"},
        text=True,
        stderr=subprocess.PIPE,
        timeout=30,
    )


def elf_x64(path):
    with Path(path).open("rb") as file:
        header = file.read(20)
    if (
        len(header) != 20
        or header[:4] != b"\x7fELF"
        or header[4:6] != b"\x02\x01"
        or header[7] not in (0, 3)
        or int.from_bytes(header[18:20], "little") != 62
    ):
        raise ValueError(f"Expected a little-endian Linux x64 ELF object: {path}")


def parse_dependencies(output):
    """Parse the C-locale glibc ldd form, refusing unresolved or ambiguous entries."""
    result = {}
    for line in output.splitlines():
        line = line.strip()
        if re.fullmatch(r"linux-vdso\.so\.1 \(0x[0-9a-fA-F]+\)", line):
            continue
        if "=> not found" in line:
            raise ValueError(f"Unresolved ELF dependency: {line}")
        match = re.fullmatch(r"([^\s/\\]+)\s+=>\s+(.+?)\s+\(0x[0-9a-fA-F]+\)", line)
        if match:
            name, location = match.groups()
        else:
            loader = re.fullmatch(r"(/.+?)\s+\(0x[0-9a-fA-F]+\)", line)
            if not loader or Path(loader[1]).name != "ld-linux-x86-64.so.2":
                raise ValueError(f"Unrecognized ELF dependency entry: {line}")
            name, location = Path(loader[1]).name, loader[1]
        if name in {".", ".."} or any(ord(char) < 32 for char in name + location):
            raise ValueError("Invalid ELF dependency name/path")
        path = Path(location)
        if not path.is_absolute() or not path.is_file():
            raise ValueError(f"Missing ELF dependency file: {location}")
        path = path.resolve(strict=True)
        if name in result and result[name] != path:
            raise ValueError(f"Ambiguous ELF dependency: {name}")
        result[name] = path
    if not result:
        raise ValueError("Expected a dynamically linked ELF dependency list")
    return result


def dependencies(path):
    elf_x64(path)
    try:
        needed = command("patchelf", "--print-needed", path).splitlines()
    except subprocess.CalledProcessError as error:
        raise ValueError(f"Missing ELF dynamic dependency metadata: {path}") from error
    if not needed:
        # ldd calls dependency-free DSOs "statically linked". Do not infer this
        # from that ambiguous text: require ET_DYN and an actual shared SONAME.
        with Path(path).open("rb") as file:
            header = file.read(20)
        soname = command("patchelf", "--print-soname", path).strip()
        if int.from_bytes(header[16:18], "little") != 3 or not re.fullmatch(r"[^\s/\\]+", soname):
            raise ValueError(f"Dependency-free ELF must be a shared object with SONAME: {path}")
        return {}
    try:
        return parse_dependencies(command("ldd", path))
    except ValueError as error:
        raise ValueError(f"{path}: {error}") from error


def owned_objects(root, binaries):
    root = Path(root).resolve(strict=True)
    result = set()
    for binary in binaries:
        path = Path(binary)
        resolved = path.resolve(strict=True)
        if path.is_symlink() or not resolved.is_relative_to(root) or not resolved.is_file():
            raise ValueError(f"ELF input must be a physical staged file inside the package: {path}")
        elf_x64(resolved)
        result.add(resolved)
    if not result:
        raise ValueError("No staged ELF inputs")
    return root, sorted(result)


def relative_runpath(path, libraries):
    relative = os.path.relpath(libraries, path.parent)
    return "$ORIGIN" if relative == "." else "$ORIGIN/" + relative


def verify(root, binaries):
    """Require loader resolution inside this relocated package, except the listed host ABI."""
    native_tools()
    root, binaries = owned_objects(root, binaries)
    libraries = root / "lib"
    external = set()
    for binary in binaries:
        expected = relative_runpath(binary, libraries)
        if command("patchelf", "--print-rpath", binary).strip() != expected:
            raise ValueError(f"Unexpected packaged ELF RUNPATH: {binary}")
        for name, dependency in dependencies(binary).items():
            if name in HOST_ABI:
                if dependency.is_relative_to(root):
                    raise ValueError(f"Host ABI unexpectedly bundled: {name}")
                external.add(name)
            elif (
                not dependency.is_relative_to(libraries)
                or dependency != (libraries / name).resolve()
            ):
                raise ValueError(f"ELF dependency escaped package lib/: {name}: {dependency}")
    return sorted(external)


def stage(root, binaries, *, preferred_providers=None):
    """Stage destination-to-original mappings and independently verify the closure.

    Inspect originals before copying libraries so both absolute and $ORIGIN build
    RUNPATHs resolve correctly. Only separate physical staged copies are patched.
    lib/ must be absent, avoiding accidental modification of a build tree or
    silent resumption of a partially staged library set. Name collisions are
    checked against original file hashes before any copy or RUNPATH mutation.
    """
    native_tools()
    if not isinstance(binaries, dict):
        raise ValueError("ELF staging requires destination-to-original-source mappings")
    originals = {
        Path(target).resolve(strict=True): Path(source).resolve(strict=True)
        for target, source in binaries.items()
    }
    if len(originals) != len(binaries):
        raise ValueError("Ambiguous staged ELF destinations")
    root, binaries = owned_objects(root, binaries)
    for target in binaries:
        source = originals[target]
        if source.is_relative_to(root) or os.path.samefile(source, target):
            raise ValueError("ELF originals must be separate files outside the staged package")
        elf_x64(source)
        if digest(target) != digest(source):
            raise ValueError(f"Staged ELF differs from original source: {target}")
    libraries = root / "lib"
    if libraries.exists() or libraries.is_symlink():
        raise ValueError("ELF staging requires a fresh package lib/ directory")
    sources = {}
    candidates = {}
    hashes = {}
    for binary in binaries:
        # Inspect the original location: build RUNPATHs can already use $ORIGIN
        # and cannot resolve before the staged package's lib/ exists.
        for name, dependency in dependencies(originals[binary]).items():
            if name in HOST_ABI:
                continue
            elf_x64(dependency)
            if dependency not in hashes:
                hashes[dependency] = digest(dependency)
            candidates.setdefault(name, set()).add(dependency)
    if preferred_providers is None:
        preferred_providers = {}
    overrides = {}
    if not isinstance(preferred_providers, dict):
        raise ValueError("ELF provider policy must be a name-to-source mapping")
    for name, path in preferred_providers.items():
        if name in HOST_ABI or name not in candidates:
            raise ValueError(
                f"ELF provider policy must select an observed non-host dependency: {name}"
            )
        provider = Path(path).resolve(strict=True)
        if provider not in candidates[name]:
            raise ValueError(
                f"ELF provider policy must select an observed non-host dependency: {name}"
            )
        overrides[name] = provider
    for name, paths in candidates.items():
        if name in overrides:
            sources[name] = overrides[name]
        elif len({hashes[path] for path in paths}) > 1:
            details = "; ".join(f"{path} ({hashes[path]})" for path in sorted(paths))
            raise ValueError(f"Conflicting ELF dependency contents: {name}: {details}")
        else:
            sources[name] = sorted(paths)[0]
    # An explicit policy is never an automatic same-name collision bypass. Verify
    # SONAME and all used symbol/version/type requirements of the selected graph.
    from .elf_symbols import validate_provider

    provider_inventory = {}
    consumers = set(originals.values()) | set(sources.values())
    for name, provider in sorted(overrides.items()):
        required = validate_provider(name, provider, candidates[name], consumers, command)
        provider_inventory[name] = {
            "selectedSource": str(provider),
            "selectedSourceSha256": hashes[provider],
            "candidates": [
                {"source": str(path), "sourceSha256": hashes[path]}
                for path in sorted(candidates[name])
            ],
            "consumerImports": required,
        }
    libraries.mkdir()
    physical = {}
    inventory = {}
    for name, source in sorted(sources.items()):
        checksum = hashes[source]
        target = libraries / name
        if checksum in physical:
            target.symlink_to(physical[checksum].name)
        else:
            shutil.copy2(source, target)
            if digest(target) != checksum:
                raise ValueError(f"ELF dependency changed while copying: {source}")
            physical[checksum] = target
        inventory[name] = {"source": str(source), "sourceSha256": checksum}
    objects = sorted(set(binaries) | set(physical.values()))
    for binary in objects:
        mode = stat.S_IMODE(binary.stat().st_mode)
        binary.chmod(mode | stat.S_IWUSR)
        try:
            command("patchelf", "--set-rpath", relative_runpath(binary, libraries), binary)
        finally:
            binary.chmod(mode)
    external = verify(root, objects)
    for name in inventory:
        inventory[name]["sha256"] = digest(libraries / name)
    return {
        "platform": "linux-x64",
        "tools": {
            "patchelf": command("patchelf", "--version").strip(),
            "ldd": command("ldd", "--version").splitlines()[0],
            "readelf": command("readelf", "--version").splitlines()[0],
        },
        "hostABI": external,
        "binaries": [str(path.relative_to(root)) for path in binaries],
        "binarySources": {str(path.relative_to(root)): str(originals[path]) for path in binaries},
        "libraries": inventory,
        "physicalLibraries": len(physical),
        "providerOverrides": provider_inventory,
    }
