# Building and staging Code-OSS built-in extensions

The diagnostic compiler currently stages application sources and renderer assets,
not system extensions. This tool fills that packaging boundary with actual
upstream inputs and outputs. It does not patch Code-OSS, invent entry modules or
claim extension activation. See the [adaptation tracker](code-oss-adaptations.md)
and [current compatibility status](../compatibility/vscode.md).

`tooling/code_oss/builtin-extensions.py` defaults to a **read-only plan**. Dependency
installation, source preparation and staging are separate explicit actions. The
direct-source static transaction is locally validated on macOS. The full upstream
dependency installation/build route and runtime extension activation remain
unaccepted.

## Immutable inputs and bounded workspace

`builtin-extensions-pin.json` pins the complete upstream archive, commit, archive
size/hash, original Node build version and pnpm version. The tarball's PAX revision
must agree with the source pin. Every member is checked; regular source files are
hashed, unexpected member types/path traversal are rejected, and the one original
internal test-fixture symlink is preserved only in the build workspace.

The pinned archive is 87,934,091 compressed bytes and contains 20,991 regular files
with 351,497,557 logical source bytes. A single reusable owned workspace holds this
source, build dependencies and current upstream output. Required generated output
is bounded to 100,000 files / 1 GiB per inventory. Dependency install size is not
claimed in advance: it depends on the locked platform packages and must be reviewed
before the first install. The existing pnpm store is reused. The accepted root
production dependency tree is never an installation destination.

For the contribution-only first step, reuse the already extracted pinned source
instead of preparing a second 351 MB workspace. This route reads the complete
immutable archive to verify its SHA-256, revision and source inventory, then checks
all **558 files / 7,673,634 bytes** in the exact 60 selected source trees. It neither
creates a workspace marker nor installs packages. Other source trees are untouched.
The maintained pin also contains the normative static file hashes, so later
resource verification does not require keeping an extra source/archive copy.

```sh
python3 tooling/code_oss/builtin-extensions.py \
  --archive /ABSOLUTE/BUILD/code-oss-source.tar.gz \
  --static-source /ABSOLUTE/BUILD/code-oss-source \
  --stage /ABSOLUTE/BUILD/code-oss-window-cache-final \
  --action plan
```

The default `plan` is read-only. After stopping the application using that stage,
replace `plan` with `stage-static` to publish resources. Do not pass `--workspace`
or `--store` to this route. All paths must be physical; on macOS use the canonical
path rather than a `/var` symlink alias. Existing extracted source files must
match the archive, including executable flags; symlinks and unknown selected-tree
files/directories are refused. Regular `.DS_Store` files are ignored and preserved.

The transaction verifies the existing compiler outputs and every existing package
adapter/audit record without changing their ownership sets. It copies only the
7.7 MB static resources into `.elytra-builtin-static.pending`, verifies the copy,
then atomically renames it to `extensions` **without replacing any existing
path**. The sealed `ELYTRA-BUILTIN-EXTENSIONS.json` is published last. The original
`ELYTRA-COMPILE.json` bytes must stay unchanged throughout. A repeated invocation
with an already verified static inventory does nothing. An unknown—even empty—
`extensions` directory, stale transaction/lock, edited files, symlinks or a built
stage cannot be adopted or overwritten. Failed transactions retain their bounded
pending data for explicit inspection; the tool never deletes Finder metadata or
steals a stale lock. The atomic no-replace implementation uses macOS
`renamex_np(RENAME_EXCL)`, Linux `renameat2(RENAME_NOREPLACE)` or Windows rename;
platforms lacking that operation fail explicitly. Only the local macOS source
transaction controls have been exercised so far.

`builtin-extensions-verify.mjs` exports synchronous, read-only
`verifyBuiltinExtensions(stage)`. It returns `null` only when both inventory and
extension roots are absent; otherwise it verifies the seal, source pin, exact
published files, executable flags, identities and absence of unknown entries.
Pending publication is always rejected. It returns a compact inventory summary
on success and throws on drift. Its seal detects accidental metadata drift; it
is not a signature. Static file hashes are independently constrained by the
maintained archive pin. Resource ownership is separate from compiled application
ownership: future legitimate compiler refreshes may change the core manifest
hash while retaining the same pinned source revision. `verifiedRefresh` now
composes this check before allowing compiler reuse, without weakening its orphan
or collision checks. The publisher uses that same composed verification.

Focused temporary-destination controls (no application launch/install/build):

```sh
python3 tooling/code_oss/builtin-extensions-test.py \
  --archive /ABSOLUTE/BUILD/code-oss-source.tar.gz \
  --static-source /ABSOLUTE/BUILD/code-oss-source
```

This remains explicitly partial: 35 source extensions require real build outputs,
and the three original product VSIXs are not downloaded. Static publication does
not establish scanning, activation, workbench readiness or full extension support.
The upstream build route below remains the path to those real outputs.

The current diagnostic stage contains these 60 static extensions: 558 files,
7,673,634 bytes, matching the original archive. Its compiler manifest hash and all
six package-adapter ownership records remain unchanged. Temporary publication
controls pass for collisions, edited resources, recomputed dishonest inventories,
unknown directories, symlinks, partial publication, idempotence and Finder metadata.
This is resource packaging acceptance, not extension activation or editor acceptance.

Keep the current workspace and stage instead of creating one application copy per
trial. The tool does not delete shared stores or dependencies, registered worktrees,
source edits or Finder metadata. It refuses upstream cleanup if Finder metadata is
present in the build workspace. Staging ignores and preserves `.DS_Store` files.
After validation, inspect CMake caches and active processes before any separate
cleanup of superseded caches. No automatic broad cleanup is provided.

## Inspect the plan first

From the active Elytra checkout, supply the existing source archive and shared
store paths. The workspace path must be dedicated and outside the store/stage.
Paths below are examples; reuse the same ones for every action.

```sh
python3 tooling/code_oss/builtin-extensions.py \
  --archive /path/to/elytra/build/code-oss-source.tar.gz \
  --workspace build/code-oss-extension-build \
  --store /path/to/elytra/build/code-oss-pnpm-store
```

The JSON plan prints verified counts, all extension classifications, original
product VSIX versions/checksums and exact argument arrays. It creates no workspace,
store, stage, downloads or dependency installations. Archive/source drift refuses
before a write. It does not infer revision from an unrelated enclosing Git repo;
the existing Code-OSS source directory can be an extracted archive without `.git`.

The pinned source has **60 static contribution extensions** and **35 requiring
builds**. The latter count includes four test extensions and Copilot, which the
standard upstream product packaging excludes. `notebook-renderers` has no `main`
or `browser`, but its contributed `renderer-out/index.js` must be built: it is not
misclassified as a static extension. The original grammars, snippets, language
configuration, icons, themes and localized manifests remain real resources.

## Explicit preparation and upstream build

Repeat the same arguments with `--action prepare` to extract the verified archive
into a fresh workspace. Reuse is allowed only with the tool's matching ownership
marker and unchanged original input hashes. A partially prepared or edited workspace
is refused, not adopted or overwritten. No source changes are repaired silently.

Use `--action build` only after reviewing the plan. `--node /path/to/node` selects
the **build-only** Node executable; its version must match upstream `.nvmrc`
(24.22.1). `--pnpm` may select the pinned pnpm 12.16.0 entry script. These are build
tools and are never embedded or shipped as Mite's runtime.

The tool runs frozen, scripts-disabled installs for three separate upstream roots:

- The repository root, filtered to its root package, including build dev dependencies.
- `build`, filtered to its root package; this supplies upstream vsce 3.6.1 and
  build esbuild 0.27.2.
- The complete `extensions` workspace, with its independent lock/workspace/pnpmfile
  inputs; it supplies extension esbuild 0.28.1 and actual extension dependencies.

All use the same `--store-dir`, `--frozen-lockfile`, `--ignore-scripts` and explicit
dev dependencies. Missing native/build dependencies fail normally. The tool does
not turn install scripts on automatically or rebuild an incompatible addon for
Mite. `NODE_OPTIONS` is refused for reproducible execution. The build revision is
set to the verified pin, and download quality is stable so upstream cannot choose
its checksum-free “latest” route.

The actual upstream task sequence is:

```sh
node --experimental-strip-types --max-old-space-size=8192 \
  WORKSPACE/node_modules/gulp/bin/gulp.js \
  --cwd WORKSPACE --gulpfile WORKSPACE/build/gulpfile.extensions.ts \
  compile-extension-media
node --experimental-strip-types --max-old-space-size=8192 \
  WORKSPACE/node_modules/gulp/bin/gulp.js \
  --cwd WORKSPACE --gulpfile WORKSPACE/build/gulpfile.extensions.ts \
  compile-extensions-build
```

The first task builds all eight actual extension renderer/webview media scripts,
including notebook output. The second uses original `build/lib/extensions.ts`:
it bundles real Node extension entrypoints, performs upstream checks, selects actual
vsce resources, rewrites packaged `/out/` entry paths to `/dist/` as upstream does,
includes shared TypeScript and retained external dependencies, and preserves the
standard product exclusions. It does not merely transpile every extension source
into guessed output paths.

The original product pins three debugger/profile VSIXs. The upstream task downloads
and validates those exact archive checksums; no alternate extension catalog or
placeholder package is introduced. Unverified version-only marketplace caches are
refused. Original sources are rechecked after the commands. A complete output hash
inventory is required before later staging; actual desktop and notebook entrypoints
must exist, and the packaged directory set must match the original product.

Copilot remains a separate original upstream exclusion by default. Explicit
`--include-copilot` adds its independent frozen install root and original
`compile-copilot-extension-build` task. Four test extensions remain excluded.
This selection is recorded; it is not a claim that Copilot or any native addon runs
on Mite.

## Stage actual outputs into the existing diagnostic tree

Stop that diagnostic application's processes before changing its resources. Supply
`--stage CURRENT_STAGE` and choose exactly one action:

- `--action stage-static` copies only the 60 original static contribution directories
  after source preparation. Its inventory says `partial-static-only` and lists the
  remaining 35 source extensions as not staged, including the upstream exclusions.
  This is a useful grammar/theme slice, not the full extension goal.
- `--action stage-built` requires a matching verified successful upstream build.
  It stages standard source-built outputs and shared dependencies under
  `extensions/`. The three product VSIX outputs go under
  `.build/builtInExtensions/`, exactly where the original development scanner looks.

The target must already have a matching owned `ELYTRA-COMPILE.json`. The tool owns
only extension resource paths and its separate `ELYTRA-BUILTIN-EXTENSIONS.json`;
it never rewrites the application's compile provenance, package adapters or user
extension/profile directories. Existing unowned files, symlinks and edited owned
outputs are refused. Later static-to-built staging replaces only previously
verified owned files. Previous extension ownership metadata is retained before
replacement. An interrupted staging operation leaves no completed new ownership
claim; inspect its retained metadata before manual recovery rather than adopting
unrecorded files.

The extension inventory records:

- Source revision/archive hash, source classification and original exclusions.
- Actual staged manifests and their SHA-256 hashes, IDs, versions and locations.
- Actual desktop/browser entry presence and packaged native binary paths.
- Every staged file's original bytes/hash and executable flag.
- `runtimeAcceptance: not-established` and per-extension activation `unverified`.

The desktop build does not imply browser entries were built; their presence is
reported separately. Git and Microsoft authentication retain upstream native
runtime requirements. In particular, an embedded `@vscode/fs-copyfile` package must
have an audited genuine Mite boundary before Git activation is accepted; packaging
an Electron/NAPI binary cannot establish compatibility. The full follow-up gate is
unchanged original extension scanning, actual activation and representative language,
Git and extension-host operations. A directory existing or a manifest scanning
successfully does not close that gate.
