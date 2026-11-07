# 0112: Native protocol policy delivery before helper bootstrap

The origin component must reach the actual child process before application code
runs. A parent-only registration is insufficient. Preserve the unmodified
Ladybird bootstrap/sandbox and the existing IPC message IDs; deliver a bounded
immutable snapshot on the existing native process-launch path instead.

Use maintained C++ beside the origin policy and owned helper entry. Reuse AK JSON
and the native argument vector rather than adding a Rust/FFI boundary, new service,
configuration file or environment variable. Scheme names and flags are not secrets;
argument delivery is owned by the native parent, not a renderer/preload message.
The payload is versioned, limited to 8 KiB and 32 schemes, and accepts only the
exact schema/entry keys and typed standard/secure flags. Reject unknown privileges,
reserved/invalid names and duplicates through the same pure validator as native
registration. Parsing/validation alone does not publish state.

Add one fingerprinted helper-launch call to the existing origin/trust composition.
Serialize only a declared parent policy. The launch snapshot seals an undeclared
empty policy as well, without appending an option, so a late registration cannot
leave prelaunched and future helpers with different rules. Empty sealing uses a
real process-lifetime table, not a sentinel pointer. Future public registration
must run before this boundary; spare-process timing still needs real integration.

The owned diagnostic and production entries consume --firefly-origin-policy=JSON
before calling upstream main or creating its VM. Validate duplicates/all arguments
before installation, preserve argv[0] and arguments after --, and own the filtered
UTF-8 strings, views and null-terminated argv through upstream entry completion.
Malformed input exits before bootstrap; no silent fallback or partial policy.
Configured helpers emit one read-only startup record with PID and canonical policy.
It is development evidence, not a new page API or a cryptographic attestation.

Actual headless parent/child tests correlate that record with the selected
WebContent PID, verify the first ordinary script, default denial and absence of
native globals. Both diagnostic and production preload helpers receive the exact
policy. Native component tests cover codec roundtrips, malformed/unknown fields,
budgets, option/delimiter/argv0 handling, temporary-input lifetime, atomic refusal
and late registration after an unconfigured first launch. No desktop windows are
needed. Windows has native codec/origin CI; owned Windows helper delivery remains
unimplemented, and worker-process propagation is not added by this slice.

## A distinct Rust DOM URL blocker

The first end-to-end expectation of app:// origins in new URL(...).origin failed.
The child did receive its native policy, but LibWeb's DOM URL class calls
liburl_rust directly in Bindings/Rust/src/domurl/url.rs. Its origin/extract-origin
methods bypass the C++ URL::origin seam. The Rust parser's SchemeType and
BasicParseOptions provide no custom-standard scheme extension at the pinned
revision. Do not fork/copy that parser or advertise its unknown-scheme behavior as
Chromium standard parsing.

Keep native configuration acceptance separate from application-facing DOM origin
acceptance. check-firefly-protocol-renderer reports the latter as PENDING;
check-firefly-protocol-dom-origins / --require-dom must fail until the actual Rust
binding agrees. The strict gate's failure has been observed, not skipped or
relabelled as compatibility. Public protocol remains absent, and actual Code-OSS
still fails on its import. Next resolve consistent Rust/C++ parser/origin behavior
and main registration timing, then implement bounded resources and fetch/CORS/CSP
contracts before exposing the API.

See [native instructions](../../firefly/native/protocol/README.md),
[prior origin decision](0111-custom-protocol-origins.md) and the
[Code-OSS inventory](../compatibility/vscode.md).
