# ADR 0192: Native services for Code-OSS packages

Status: implemented and validated in standalone Mite on macOS; combined editor
startup remains under development.

## Context and decision

Pinned Code-OSS imports @vscode/sqlite3 5.1.12-vscode and native-keymap 3.3.9.
Their Node native addons cannot run in LibJS. Keep N-API, Node and V8 unsupported;
provide explicit Mite services and opt-in, exact-source application adapters.
Ordinary Node/Electron retains the original package implementation.

Use C++ at this boundary: the runtime already embeds LibJS and libuv, while SQLite
and Carbon expose C interfaces. Another Rust/FFI ownership layer would not improve
this integration. Mite must not gain a Qt, Firefly or LibWeb dependency.

## Storage

mite:sqlite implements the Code-OSS node-sqlite3 subset over actual SQLite 3.53.4,
pinned to vcpkg port revision 2. Use real connections, prepared statements, bindings,
transactions and file persistence. No JSON database or implicit in-memory fallback
is permitted. Explicit :memory: requests retain SQLite's own semantics.

Serialize bounded work on libuv workers; enter JavaScript only on the owner loop.
Trace callbacks until completion, revoke them at shutdown, bound statements/results
and interrupt stopped work. Statement/database finalizers release real resources.
Maintained ESM supplies callback/FIFO/EventEmitter behavior, safe ordinary row
objects and Buffer conversion. Unsupported operations fail explicitly. This is not
an implementation of node:sqlite or general Node addon compatibility.

## Keyboard layout

mite:native-keymap reads the actual selected macOS TIS source, physical-key
translations/dead-key state and ISO keyboard classification. The pinned Chromium
mapping table and original notices remain intact. Changes are detected by real
500 ms owner-loop polling. Its timer remains referenced, matching the original
package's referenced callback source, and is closed at shutdown. Queries alone
create no timer. No keyboard input is synthesized or intercepted. Other platform
providers currently fail explicitly.

## Application staging and validation

Adapters verify exact loader/package hashes. An existing compiled stage can add
them only after validating every prior adapter and the new original files; user
drift, adapter removal and escaping targets are refused. Reuse the dependency tree
and retain all package/SQLite/table notices. No .node placeholder is created.

Actual standalone Release Mite and Firefly build. The focused SQLite fixture
persists text with NUL, Unicode and binary values, rolls back, closes and reopens;
independent Python SQLite reads the same rows and passes integrity_check. The
actual standalone macOS query returns layout metadata, 119 key mappings and ISO
classification. Dynamic physical layout changes and other OS backends are not
certified by that query. A genuine editor startup is still required to establish
Code-OSS integration; no workbench/open-edit-save pass follows from these services.
