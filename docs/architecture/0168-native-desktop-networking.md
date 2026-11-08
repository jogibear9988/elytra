# Native desktop networking and private transport capabilities

## Decision

Implement Electron-shaped net in maintained ESM over Mite's real LibJS/libuv HTTP
and verified TLS transport. Share this policy between main and utility roles;
never embed Node, V8 or Chromium or alias node:http.request while claiming Electron
session behavior. Native macOS availability uses passive Network.framework path
updates and bounded SystemConfiguration proxy snapshots. Other platform providers
fail explicitly until implemented and accepted. Ladybird remains unmodified.

The trusted ModuleLoader.web_transport embedding capability captures original
Fetch classes/functions, abort hooks and stream adapters before application entry.
When requested, it snapshots the small HTTP/HTTPS/stream/crypto dependency facade
rather than consulting mutable public exports on later requests. Ordinary standalone
Headers/Response programs retain lazy socket-module construction. This is a private
embedding capability, not a public builtin or a Firefly dependency in Mite.

## Ownership and policy

Track each exchange through headers AND response-body completion/cancellation.
Limit active exchanges and retained ClientRequests separately to 64. Use real
Writable callbacks and buffered uploads (16 MiB maximum), matching Electron's default
buffered path; chunked streaming remains explicitly unsupported. Native responses
become Readables with actual status/headers and pressure; unavailable raw HTTP
metadata fails instead of being synthesized from Fetch.

Follow redirects manually to recheck native proxy state and strip sensitive
headers on every origin change, including port changes. The observed Electron
reference forwarded Authorization in the tested cases; Firefly deliberately keeps
stricter stripping. Native configured or unknown proxy/PAC/WPAD routing cannot be
silently bypassed. Sessions, cookies, authentication caches, custom interception
and shared browser HTTP caching remain explicit gaps: the initial transport is
stateless and separate from renderer/default-session state.

Shutdown uses captured internal cleanup operations even if application methods have
been replaced, aborts each exchange independently and retires the native provider.
NWPathMonitor callbacks retain only weak, mutex-protected native state; no VM/Qt
callback crosses threads. Before a first observation, availability is pending,
never invented offline. Availability means route state, not Internet reachability.

## Utility boundary

Initialize utility networking before Qt/desktop construction. Register only the
owned utility net module alongside its existing process.parentPort capability.
The parent's explicit Electron compatibility mode selects a private utility role
flag; both flags validate inherited fd3 before runtime setup and disappear from
public arguments. Main APIs/windows are not installed in the utility realm.
Stop JavaScript network bodies while its realm is still alive, then retire native
monitor state; ordinary standalone/fork roles receive no Firefly capability.

## Dependencies and evidence

The actual Code-OSS request consumer also needs Node legacy URL parsing and global
WebCrypto randomness. Add genuine source-pinned parsing and native entropy APIs;
do not replace consumer imports or fabricate crypto. See the
[URL decision](0169-node-legacy-url.md), [crypto contract](../api/crypto.md),
[desktop net contract](../api/desktop-net.md) and [progress](../PROGRESS.md) for exact
component and platform validation. Complete workbench, browser sessions/proxies,
physical network transitions and broader platform acceptance remain separate work.
