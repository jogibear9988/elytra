# node:tls

Status: partial native client TLS. Native Linux/Windows CLI baseline 3981a56
passes the TLS suite as well as local macOS ARM64 acceptance.
[HTTPS clients](https.md) use this transport; TLS servers remain unfinished; [Fetch](fetch.md) now has a partial native client layer.
The newer HTTPS-hook/address suites also pass native Linux/Windows at da5e3e8;
see [current evidence](../status.md). Broader trust/context cases remain unverified. Mite uses OpenSSL and its existing TCP loop; no Node/V8 or LibWeb
runtime is embedded.

```js
import { connect } from 'node:tls';
import { readFileSync } from 'node:fs';
const socket = connect({
  host: 'localhost', port: 8443, servername: 'localhost',
  ca: readFileSync('ca.pem'), signal: AbortSignal.timeout(5000)
}, () => {
  console.log(socket.authorized, socket.getProtocol());
  socket.end('Hello through TLS');
});
socket.on('data', (data) => console.log(data.toString()));
socket.on('error', (error) => console.error(error.code));
```

## connect

`connect(options[, secureConnectListener])` returns a TLSSocket. The positional
`connect(port[, host][, options][, listener])` form is also implemented. The callback
subscribes once to secureConnect. ESM and CommonJS share the builtin instance.

| Option | Contract |
| --- | --- |
| host / port | TCP target; host defaults to localhost; port is required |
| servername | DNS SNI name and verified peer identity; otherwise verify host, without implicit SNI |
| ca | PEM string, byte buffer or array; replaces default trust, not an addition; empty array trusts no CA |
| rejectUnauthorized | Default true; explicit false permits verification failure and records authorizationError |
| minVersion / maxVersion | TLSv1.2 / TLSv1.3 only; defaults 1.2–1.3 |
| ALPNProtocols | Array of nonempty strings, each at most 255 UTF-8 bytes; total wire list at most 65535 bytes |
| signal | Realm-local AbortSignal; abort closes the actual pending/established transport with AbortError/ABORT_ERR and original reason cause |
| timeout | Socket inactivity notification; does not itself abort; use a signal for a deadline |
| family / lookup / hints / autoSelectFamily / autoSelectFamilyAttemptTimeout | Forwarded to the supported TCP connector |
| allowHalfOpen / highWaterMark / noDelay / keepAlive / keepAliveInitialDelay | Selected Duplex/TCP controls |

Certificate verification happens before secureConnect or application data. DNS/IP
SANs are checked; partial-label wildcards and common-name fallback are disabled.
An IP host is verified against IP SANs. Legacy numeric IPv4 spellings such as
127.1, octal/hex components and a single decimal integer reject explicitly before
connection; they cannot select DNS-SAN verification for an IP route. Explicit IP-valued servername is rejected;
use the IP host and omit SNI. Internationalized names and all normalization edge
cases are not certified; use ASCII/IDNA DNS names for this slice.

## Trust and errors

Default anchors come from macOS Security anchors, OpenSSL's Windows ROOT store
provider or Linux distribution/OpenSSL trust locations. This differs from Node's
bundled Mozilla CA list. It does not reproduce every OS trust-setting override or
revocation policy. Local tests prove explicit custom-CA validation and rejection
under default trust, not every publicly trusted endpoint/enterprise configuration.
Default trust-load failure rejects explicitly; it never silently disables checking.
NODE_TLS_REJECT_UNAUTHORIZED does not implicitly bypass verification.

Custom ca input is limited to one MiB. Malformed/no-certificate input rejects;
provider certificate records and CA options use copied bytes. Relevant error codes
include DEPTH_ZERO_SELF_SIGNED_CERT, UNABLE_TO_GET_ISSUER_CERT_LOCALLY,
UNABLE_TO_VERIFY_LEAF_SIGNATURE, CERT_HAS_EXPIRED, CERT_NOT_YET_VALID and
ERR_TLS_CERT_ALTNAME_INVALID. Other certificate/provider errors fail with explicit
TLS/SSL codes; exact error text, priority and all Node codes are not certified.

`rejectUnauthorized:false` is an explicit permissive mode. authorized remains false
and authorizationError records the verification failure; secureConnect is then
allowed. Chain and hostname failure modes are tested separately. An unreferenced
signal deadline alone does not keep a program alive, but a live TLS transport can.

## TLSSocket

Returned sockets are TLSSocket and net.Socket instances with encrypted=true.
Observe secureConnecting, authorized, authorizationError, alpnProtocol and
getProtocol(). The connect event represents TCP establishment; secureConnect
represents completed TLS validation. A successful TCP connection alone does not
make the peer authenticated.

Supported stream workflows include writes queued before secureConnect, write/end,
pipe, readable encoding, pause/resume, backpressure and destruction. Address getters,
address(), setNoDelay, setKeepAlive, ref/unref and setTimeout proxy the native TCP
transport. Application writes are serialized in 16 KiB record operations. Pending
native encrypted input/output and plaintext snapshots are bounded at one MiB per
operation; aggregate application transfers can be larger. Fatal callback and native
teardown rules are those of the shared runtime.

end sends close_notify before TCP half-close. Missing peer close_notify rejects
with ERR_SSL_UNEXPECTED_EOF_WHILE_READING; complete Node truncation tolerance is
not claimed. Destroy disposes signal/provider state and pending native work before
close. Pending handshake cancellation is checked against an independent OS peer;
forced GC checks native record ownership in the non-GUI host.

## Unsupported modes

Direct TLSSocket construction/wrapping, TLS servers, reusable SecureContext,
client certificates/key/PFX, custom ciphers, custom checkServerIdentity, sessions,
PSK callbacks, renegotiation, tracing, handshakeTimeout and key material export
remain unsupported. Unavailable provider policies (secureOptions, sigalgs,
ecdhCurve, OCSP, partial-chain settings and client-key engines) and TCP local
binding/descriptor/IPC options reject explicitly. Use AbortSignal.timeout for a handshake deadline. createServer,
createSecureContext and the exposed unsupported methods throw ERR_MITE_UNSUPPORTED_API.
Peer-certificate/cipher inspection is not yet implemented. Full Node TLS option,
metadata, half-close/error and native platform parity remain unfinished.

Run the [TLS client example](../../examples/mite-tls/client.mjs) with a TLS echo peer
and an explicit test CA for local certificates. See [TCP](net.md),
[cancellation](abort.md) and [the implementation decision](../architecture/0079-native-tls-client-records.md).
