# Build, packaging and validation tools

The top-level Python commands are build/packaging/integrity entrypoints referenced
by CMake, CI and developer commands. Shared helpers live alongside those commands;
regression suites are separated into tests. None is an application runtime fallback.

| Location | Responsibility |
| --- | --- |
| bootstrap.py, build_*.py | Pinned dependency setup, native builds and sample staging |
| package_*.py, npm_payload.py | Relocatable native packages, notices and production payload policy |
| check_*.py, audit_openssl.py | Source/provenance/ABI guards and binary dependency validation |
| tests | Native/differential/integration regression CLIs and shared test setup |
| runtime-js | Maintained ESM adapters and reproducible embedding generators |
| code-style | Pinned authoring tools, spacing rules and explicit malformed-input exceptions |
| site | Static documentation compiler and its pinned dependencies |
| npm-fixtures, electron-reference | Locked reference packages and comparison runtime setup |

Run code_style.py after editing maintained sources; use --check for validation.
Formatting and source organization must preserve public API behavior, explicit
unsupported errors, package notices and actual native acceptance gates.
