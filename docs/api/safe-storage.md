# safeStorage

Status: **partial, locally accepted on macOS**. Genuine OS-backed encryption passes
native and public-facade tests against isolated temporary keychains. The actual
compiled Code-OSS EncryptionMainService passes write/read across two OS processes
with 39 verified compiled dependencies. Actual Firefly/Electron comparison covers
pre-ready behavior only; post-ready Electron cryptography and a complete Code-OSS
workbench are not accepted by these tests. Linux/Windows encryption is unavailable.

```js
import { app, safeStorage } from 'firefly';

app.whenReady().then(() => {
  if (!safeStorage.isEncryptionAvailable()) {
    throw new Error('OS key storage is unavailable');
  }
  const encrypted = safeStorage.encryptString('A local application secret');
  const restored = safeStorage.decryptString(encrypted);
  console.log(restored === 'A local application secret');
});
```

The Electron compatibility alias exports the same main-process object. Renderer
and ordinary Mite scripts receive no safeStorage/native keychain hooks. This API
protects stored data using an OS-held key; it is not a sandbox against privileged
code in the application or a guarantee against other processes with keychain access.

## Methods

| Method | Current contract |
| --- | --- |
| isEncryptionAvailable() | Boolean; false before app ready, on unavailable/locked/inaccessible key storage, and on Linux/Windows. Availability checks do not create a key. |
| encryptString(plainText) | Requires a string and ready/available storage. Returns a Buffer containing Firefly ciphertext. Limit: 1 MiB UTF-8 plaintext. |
| decryptString(encrypted) | Requires ready/available storage and a Buffer. Returns its decrypted UTF-8 string. An empty Buffer returns an empty string after availability checks. Foreign, malformed, tampered or wrong-profile ciphertext fails explicitly. |
| setUsePlainTextEncryption(value) | Requires boolean; false retains the sole OS-key policy, true explicitly refuses plaintext fallback. |
| isAsyncEncryptionAvailable() | Promise resolving false; async storage is not implemented. |
| encryptStringAsync(), decryptStringAsync() | Reject with ERR_FIREFLY_UNSUPPORTED_API. |
| getSelectedStorageBackend() | Linux-only, returns unknown because no Linux provider is implemented. Absent on macOS/Windows, matching Electron's platform export. |

The macOS backend uses the application's userData profile identity, selected lazily
when the backend is first accessed after readiness. Configure application paths
before app readiness, when the native profile is sealed. A SHA-256 profile identity scopes a randomly generated 32-byte key
stored in the default macOS Keychain. A new key is created during encryption only.
Decryption never creates a replacement for a missing key. Keep the same profile and
accessible OS key to read existing ciphertext; profile/key recovery and migration
are not implemented. Do not treat the profile name as a secret or access boundary.

Ciphertext uses a versioned Firefly envelope, AES-256-GCM, a random 12-byte nonce and
a 16-byte authentication tag. Header and profile identity are authenticated. The
current envelope adds 36 bytes to UTF-8 plaintext. Encryption copies bytes into a
Buffer; persisted Buffer JSON has the usual { type: 'Buffer', data: [...] } form
used by Code-OSS. This representation does not imply Electron ciphertext
compatibility: its OSCrypt format and migration of existing Electron secrets are
not implemented. There is no plaintext password, hardcoded key or disk key fallback.

## Keychain interaction and errors

Operations refuse keychain requests requiring user interaction. The macOS Security
API used here has a process-wide interaction switch: Firefly serializes its own
scoped changes, temporarily disables interaction, and restores the exact previous
value, including failure paths. Other credential consumers in the same process may
observe that temporary setting; this is not a thread-local or process-wide atomic
coordination guarantee with unrelated libraries. Locked/denied storage returns
unavailable rather than opening an authentication dialog. Synchronous operations
still involve OS calls; nonblocking behavior is not promised.

Before readiness, encryption/decryption throws ERR_FIREFLY_NOT_READY. Unavailable
storage reports ERR_FIREFLY_SAFE_STORAGE_UNAVAILABLE; invalid/authentication failures
use ERR_FIREFLY_SAFE_STORAGE_INVALID_DATA; native crypto failures use
ERR_FIREFLY_SAFE_STORAGE_CRYPTO. Type and budget errors use ERR_INVALID_ARG_TYPE and
ERR_OUT_OF_RANGE. Error messages/order are not full Electron parity. Availability
is a snapshot and does not guarantee a later operation will succeed.

## Validation and remaining scope

Tests create isolated temporary keychains with the production backend's key-storage
and encryption path. They cover persistence, nonce variation, Unicode/NUL/empty
strings, tampering, wrong key/profile, byte limits, locked/unavailable paths and
restoration of both initially allowed/denied interaction settings. They do not
mutate the user's Login keychain. Test-only keychain overrides are confined to
native test hosts and are absent from production Firefly.

The real public facade passes write/read/wrong-key phases, and the unchanged actual
Code-OSS service passes persisted Buffer serialization and error handling across two
native processes. Its component host supplies a minimal ready lifecycle; this is
not acceptance of the full production post-ready app or Electron OS crypto.
The actual main graph passes safeStorage and the protocol service. The linked
Code-OSS status tracks subsequent startup blockers. Native utility/renderer MessagePort handoff and demanded utility
authentication forwarding remain separate requirements; no workbench is accepted.

See [ADR 0156](../architecture/0156-os-backed-safe-storage.md),
[Code-OSS status](../compatibility/vscode.md) and
[Electron safeStorage](https://www.electronjs.org/docs/latest/api/safe-storage).
The pinned Code-OSS uses synchronous methods; Electron 45 deprecates those in favor
of a separate async API. Firefly does not claim that newer async contract.
