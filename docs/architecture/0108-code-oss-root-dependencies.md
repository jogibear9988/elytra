# 0108: Reproduce Code-OSS root dependencies before extending startup APIs

Use the pinned application's own pnpm 12.16.0, frozen lockfile, workspace
configuration, configuration hook, native-build environment and package patch.
Copy verified inputs into a fresh directory; leave the source and Ladybird alone.
Do not translate the graph to npm or replace native dependencies with empty modules.

Implement preparation/inventory in build-only ESM JavaScript: it fits the upstream
package-manager workflow and existing esbuild tooling without introducing another
FFI or runtime dependency. Pin the package manager and YAML/AST parsers in a
separate npm lockfile. Node runs these development tools, never Mite or Firefly.

Disable dependency lifecycle scripts explicitly. The upstream pnpm configuration
hook still executes as build configuration; this is not a no-code-execution
sandbox. Traverse exact root-production lock snapshots and retain integrity,
conditional platform edges and upstream native-build policy. Exclude root dev
and other installation/workspace roots explicitly. Installation success does not
establish that the installed packages run on LibJS.

Inspect package data, native loader dependencies, lifecycle strings and binary/
WASM markers without loading packages. Candidate classification is heuristic,
not an exhaustive ABI inventory or compatibility verdict. Preserve provenance and
state in ELYTRA-DEPENDENCIES.json; incomplete/failed/wrong-platform/source trees
cannot enter the diagnostic compiler. Copy only relative links contained in the
prepared module tree. Write completed compilation metadata after dependency copy.
These records track owned build outputs, not a cryptographic attestation against
later arbitrary mutations of node_modules.

The macOS ARM64 install resolves 71 direct production dependencies, 258 frozen
snapshots and 231 installed package directories; 27 carry native/WASM evidence.
Actual compiled upstream parseCLIArgs with exact minimist 1.2.8 passes 14 Node/Mite
cases. This is a real isolated subroutine test, not a substitute mainImpl. The
full-source Firefly probe, with actual installed dependencies, now fails on missing
node:module.registerHooks with exit 1. No workbench window is created. Native
three-platform install CI is a separate required gate, not desktop acceptance.

See the [adaptation guide](../guides/code-oss-adaptation.md) and
[priority inventory](../compatibility/vscode.md).
