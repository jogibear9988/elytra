# Run an HTTP server

Mite implements a partial Node-style HTTP server on its native TCP/runtime loop.
Client HTTP APIs, HTTPS/TLS, upgrades and fetch remain unsupported. Run this guide
with a native Mite build; [getting started](../getting-started.md) covers prerequisites.

## Start the repository example

```sh
build/dev/bin/mite examples/mite-http/server.mjs 3000
```

The process prints its loopback listening address and stays alive. In another terminal:

```sh
curl http://127.0.0.1:3000/hello
curl --data-binary 'hello' http://127.0.0.1:3000/echo
```

The first response is JSON describing runtime/method/URL; POST /echo streams the
request body to the response. Pass 0 instead of 3000 to obtain an available port,
then use the actual address printed by the server.

## Minimal handler

```js
import {createServer} from 'node:http';
const server = createServer((request, response) => {
  response.setHeader('Content-Type', 'text/plain; charset=utf-8');
  response.end('Hello from Mite\n');
});
server.on('error', error => {
  console.error(error.code);
  process.exitCode = 1;
});
server.listen(3000, '127.0.0.1');
```

Bind loopback for local experiments. The handler receives stream-shaped request and
response objects. Treat request data as untrusted and bound application payloads;
application security/production hardening is not established by this example.

## Lifetime and errors

A listening server keeps the program alive. Close it when your application no longer
needs it. For tests, listen on port 0 and await the actual listening callback before
sending a request. Handle native bind/connect errors and request/response errors.
Read [async/resource lifetime](../concepts/async-and-lifetime.md) before adding long
synchronous work or large streams to a handler.

## Desktop use

The [Designer sample](../../examples/web-component-designer/README.md) uses a Mite
loopback server for its compiled public assets, preserving module/import-map/worker
origin semantics in the renderer. It grants no Node/filesystem globals to that page.
Its bounded, tokenized smoke report exists only in explicit test mode; it is not a
public Firefly privileged IPC API.

See [Mite reference](../api/mite.md#tcp-dns-and-http) and the compatibility matrix for
the tested parser, headers, timeout, streaming and connection-policy limits.
