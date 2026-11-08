# ADR 0156: OS-held keys and authenticated application-profile storage

Status: partial, locally accepted on macOS; other OS providers, Electron ciphertext
migration and async API remain unsupported.

## Context

The actual Code-OSS main graph imports safeStorage. Its unchanged EncryptionMainService
requires synchronous encryptString/decryptString, availability and genuine Buffer
JSON persistence. It classifies macOS/Windows providers itself; backend selection
is consulted only on Linux. Resolving the export through plaintext or ephemeral
in-memory encryption would misrepresent persistence and protection of real secrets.

## Decision

Provide a main-only native backend behind a maintained ESM facade. On macOS store
a random 32-byte key in the OS Keychain, scoped by a SHA-256 identity of the
application's userData profile. Initialize lazily after real app readiness. The
production backend uses the default keychain; explicit keychain selection is only
available to native tests. Availability inspects access without creating a key;
encryption may create one, while decryption never manufactures a missing key.

Use OpenSSL AES-256-GCM with a fresh random 12-byte nonce, a 16-byte tag and a
versioned eight-byte Firefly header. Authenticate both header and profile digest.
Bound plaintext to 1 MiB and validate copied native input/output views. Fail on
foreign format, wrong key/profile, tampering, invalid sizes and provider failures.
Return a real Buffer so actual Code-OSS serialization works. Clear temporary native
key/decrypted buffers where owned; no total managed-memory erasure guarantee follows.
No plaintext fallback, hardcoded password, disk key file or Electron OSCrypt
compatibility is offered. Existing Electron ciphertext requires a future explicit
migration path; do not label matching method names as ciphertext compatibility.

Avoid authentication dialogs. The legacy macOS keychain provider requires scoped
use of a process-global interaction setting. Serialize Firefly's own scopes and
restore the exact prior value on normal/error paths. Unrelated in-process credential
consumers can observe the temporary setting; this is a documented integration limit,
not a thread-local setting or coordination guarantee with all native libraries.
Unavailable/locked/denied storage fails explicitly. No host preference is permanently
changed to avoid UI, and production behavior is not changed to a test-only provider.

Return unavailable on Linux/Windows until genuine providers exist. Export Linux
backend selection only on Linux, with unknown until implemented. Reject enabling
plaintext; the only accepted false policy retains the OS-key requirement. Report
async availability false and reject async encryption/decryption explicitly. These
choices preserve honest partial support while the pinned Code-OSS still uses the
synchronous API. Ladybird is unmodified; Mite acquires no Firefly/LibWeb dependency.

## Evidence and remaining work

Native tests use isolated temporary keychains through the production backend and
cover persistence, randomness, authenticated failures, bounds, unavailable storage
and restoration of interaction settings starting both enabled and disabled. Public
facade tests pass write/read/wrong-key phases through genuine LibJS/native crypto.
No tests modify the user's Login keychain. Real Firefly/Electron pre-ready tests pass;
post-ready Electron cryptography is not verified and is not inferred from its source.

The unchanged compiled Code-OSS EncryptionMainService passes write/read across two
native processes, with its original source pins and 39 compiled dependencies checked.
The component host deliberately supplies a minimal ready/command-line lifecycle;
this accepts the encryption component, not production app readiness or a workbench.
The actual full main graph reaches app ready and next fails linking WebContentsView.

Require future acceptance for production post-ready/default-keychain integration,
OS providers and packaged/signed app key access, migration/recovery, async APIs and
platform policy. Native utility/renderer port transfer and authentication forwarding
remain required for actual Code-OSS shared services. No goal completion, full Electron
support or editor/open-edit-save acceptance follows from this storage component.
See the [API contract](../api/safe-storage.md) and [current status](../status.md).


References for the legacy macOS interaction scope:
[Apple Security globals](https://github.com/apple-oss-distributions/Security/blob/main/OSX/libsecurity_keychain/lib/Globals.h),
[Apple SecKeychain implementation](https://github.com/apple-oss-distributions/Security/blob/main/OSX/libsecurity_keychain/lib/SecKeychain.cpp),
[Chromium scoped interaction adapter](https://chromium.googlesource.com/chromium/src/crypto/+/refs/heads/main/apple/scoped_keychain_user_interaction_allowed.cc).
