# Inspector state

`inspector` and `node:inspector` share one builtin instance. `url()` queries the
native runtime and returns `undefined`: Mite has no active inspector endpoint.
No debugger socket, V8 backend or Chrome DevTools Protocol session is installed.

`open()`, `close()`, `waitForDebugger()` and construction of `Session` throw
`ERR_MITE_UNSUPPORTED_API`. Other inspector APIs, including inspector console,
network inspection and the promises submodule, remain unsupported.

The importable inactive-state query is an intentional difference from Node built
without inspector support, which rejects importing the entire module. It permits
applications to report unavailable debugging without claiming a working backend.
Node's ordinary inactive-endpoint query also returns `undefined`.

References: [Node 24 inspector API](https://github.com/nodejs/node/blob/v24.15.0/doc/api/inspector.md#inspectorurl),
[Node's inspector availability guard](https://github.com/nodejs/node/blob/v24.15.0/lib/inspector.js).
