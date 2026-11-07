# ADR 0124: Publish main-process scheme registration through the native table

Status: implemented subset with local macOS Release native acceptance. Public
resource/session handlers and a Code-OSS desktop launch remain unimplemented.

Subsequent public default-session file integration is recorded in
[ADR 0126](0126-public-file-protocol-routes.md). The status below describes this step.

## Decision

Export a main-only protocol singleton with registerSchemesAsPrivileged. The maintained
ESM policy validates the entire bounded scheme/privilege batch, calls a private
native binding and consumes registration only after successful publication. Support
standard and secure origin registration; reject requested unimplemented privileges,
unknown options, incorrect types, reserved names and duplicates. Secure nonstandard
origins are not silently accepted. False unsupported flags request no capability.
No resource handler, route, session or document grant follows from registration.

The native binding independently checks the existing 8-KiB/32-entry exact JSON
schema, then publishes the existing immutable process table atomically. It uses
LibJS/LibURL only. Keep the original native once-only/post-helper-launch guard in
addition to the public pre-readiness guard. A second factory cannot bypass native
publication, and invalid batches do not partially configure origins. No reset,
environment configuration or renderer/preload registration is introduced.

Add protocol to actual Firefly host ESM/CommonJS exports and the explicitly enabled
Electron alias, sharing one singleton. Regenerate the API adapter from maintained
ESM with pinned build tooling. Mite remains independent of Firefly/LibWeb. Ladybird
and its origin/parser/launch compositions are unchanged by this step.

## Evidence and limits

The registration probe embeds the real Mite/LibJS VM, public ESM factory, private
native binding and native/Rust URL table without Qt or LibWeb. Fresh processes
exercise publication, an empty successful batch, readiness refusal and native
helper-launch sealing. Require actual normalized tuple/secure/opaque origins and
owned helper configuration. Test unsupported privilege rejection, direct native
schema/budget/type refusal, mutation after publication and native once-only refusal
from a second API factory.

The actual Firefly host test imports the named/default and CommonJS singleton,
registers before readiness, rejects repetition/late registration and exits without
constructing a BrowserWindow. Local macOS Release passes, as do existing Node/Mite
geometry factory regressions. macOS Release requires both targets; experimental
Linux runs them only after configuring its real runtime, using Xvfb for the host
starter. Fresh hosted execution is still required. No Windows desktop claim.

The actual pinned Code-OSS main trace with frozen root production dependencies now
passes the protocol named export and fails at missing crashReporter, retaining
exit 1. Import-link progress is not execution of Code-OSS's registration call or a
workbench launch. That call also requests ServiceWorker/CORS/fetch/cache privileges
which this subset explicitly refuses. Implement demanded privileges and public
resource/session contracts against the existing native navigation/document paths;
continue resolving the actual main imports/services without fake exported objects.
