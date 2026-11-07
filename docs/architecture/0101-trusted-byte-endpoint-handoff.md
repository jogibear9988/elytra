# ADR 0101: Trusted byte endpoint handoff to WebContent

Status: native diagnostic integration implemented; validation recorded in
[PROGRESS](../PROGRESS.md). Public binary IPC and product routing remain pending.

## Decision

Pass the private [byte channel](0100-native-byte-channel.md) endpoint over the
existing privileged frontend-to-WebContent LibIPC connection. Append one
process-level NotRouted message, firefly_connect_byte_channel, carrying the native
TransportHandle and a two-word correlation cookie. Do not create another named
bootstrap service, expose native handles to JavaScript or reuse a page WebDriver
method as a privileged bootstrap route.

Keep all Ladybird sources unchanged. An owned wrapper invokes the original pinned
IPC generator on a derived definition. It compares against an independently
generated original definition: all 251 existing message IDs must remain identical,
and the new message must be the sole appended ID. The root stub's default method
calls our native registration hook; no page-routing stub is added. Unregistered
helpers and an all-zero cookie close the transferred endpoint. Existing
implementations remain concrete and ordinary control messages continue to work.
All participants must use the same generated protocol/build; this does not promise
wire compatibility with separately built vanilla Ladybird binaries.

The hook lives in LibIPC so its registration is shared by the generated endpoint
and helper. Its declaration is staged under the existing build include root,
avoiding another global include path. Standalone Mite does not load these adapters
or gain a Firefly/LibWeb dependency.

## Build seams and upstream updates

The owned CMake module path wraps targets and hsts_preload, retaining upstream's
implementations before selecting our codegen adapter. HSTS reimports the upstream
codegen functions, so both seams must reapply the selector. Fail configuration if
WebContentServer did not select the wrapper; do not silently build without it.
The derived codegen CMake file changes only the generator selection and dependency
for this endpoint. All other endpoints still use the original generator.

Fingerprint the original codegen CMake, Python generator and endpoint definition.
Check exact source reconstruction, unique replacement seams and duplicate
declaration rejection. Update fingerprints only after reviewing the changed
generator, message IDs, root/routed stub layout and include ordering. No downstream
vendor patch or modification of another command's generated output is involved.

## Diagnostic acceptance and boundaries

Only the separate diagnostic FireflyWebContent registers an echo receiver. It
admits one nonzero-cookie handoff per session, sends the cookie plus its actual OS
PID as 24 little-endian bytes and echoes bounded raw packets. A duplicate endpoint
is closed without replacing the original. Shutdown unregisters the callback and
closes the channel. The product FireflyRuntimeWebContent installs no receiver.

The headless probe loads a real LibWeb document under the existing native renderer
sandbox, verifies native-reported PID/cookie, duplicate rejection and exact 65,536
arbitrary bytes, then requests document text over the original control connection.
Separate negative runs use the ordinary upstream helper and an all-zero cookie.
Require peer EOF followed by a successful document reply: a parser disconnect or
renderer crash must not masquerade as intentional denial. The document confirms
Mite process/require/Buffer globals remain absent. No visible window is needed.
The gate is required by the macOS release helper checks and Linux renderer workflow;
platform-specific executed results remain separate.

The cookie correlates a trusted handoff; it is not proof of a compromised renderer's
honesty. This is not page authorization, a public API, a file service or a benchmark.
The native transport still has the resource-bound limitations in ADR 0100. Next
work must bind messages to native process/document/realm/epoch identities, enforce
control-plane ordering and navigation revocation, schedule decoding in the right
VM context, and connect binary snapshots and Electron-compatible events/ports.
Keep public plain-data rejection until those boundaries and end-to-end gates pass.
