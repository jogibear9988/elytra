# Legacy url.parse and Url

Mite exports parse and Url from node:url alongside its existing WHATWG URL and
URLSearchParams. The maintained ESM parser is derived from pinned Node 24.15.0
source, with full MIT notices and immutable source/license hashes. It runs entirely
on LibJS; there is no Node runtime or V8 dependency.

```js
import { parse } from 'node:url';

const address = parse('https://user:pass@example.com:443/path?a=1&a=2#part', true);
console.log(address.hostname, address.port, address.query.a);
```

parse(urlString[, parseQueryString[, slashesDenoteHost]]) returns a legacy Url.
It preserves legacy protocol/slashes/auth/host/port/hostname/hash/search/query/
pathname/path/href fields rather than replacing the operation with new URL().
An existing Url input is returned unchanged. Url.prototype.parse mutates its
receiver; format and parseHost implement the corresponding maintained Node-derived
behavior. Module-level format, resolve/resolveObject and broader legacy URL APIs
remain unsupported or absent; Url resolve methods fail explicitly.

parseQueryString selects a null-prototype query object, with repeated values stored
as arrays and Node's 1000-pair limit. It does not publish Object.prototype properties.
Without it, query remains the legacy string/null value. Relative inputs, authority
syntax, escaped auth, IPv6, IDNA, default ports, escaping and legacy numeric hosts
follow the differential fixture. A separate native ICU IDNA operation avoids WHATWG
numeric-IP normalization without changing public domainToASCII/URL behavior.

All 64 focused Node/Mite cases pass locally on macOS; native Linux/Windows acceptance
for this new parser remains unverified. The Node reference suppresses deprecation
warnings in this comparison; Mite does not emit Node's legacy-API deprecation warnings.
These legacy APIs are compatibility tools, not validation for untrusted URLs. Prefer
WHATWG URL in new code, and separately validate scheme, authority and intended access.

```sh
python3 tooling/tests/test_url_legacy.py --mite build/release-mite/bin/mite
```

See [Node's legacy URL reference](https://nodejs.org/download/release/v24.15.0/docs/api/url.html#legacy-url-api)
and [runtime compatibility](../compatibility/mite-0.1.md).
