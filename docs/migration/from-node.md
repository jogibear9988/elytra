# Move a Node application to Mite

Mite implements a measured subset of Node APIs on LibJS. It does not embed Node or
V8. The behavioral reference is Node 24.15.0; this is not a claim of full Node 24
compatibility. Start with the [runtime matrix](../compatibility/mite-0.1.md).

## Audit the application first

List entry points, package.json type/exports/imports, dependencies, scripts and
native addons. Search application and installed package code for node: imports,
require calls, process APIs and browser globals. Dynamic imports and conditional
exports mean a text search alone is insufficient: execute the actual paths.

Classify each requirement against the matrix. Files, Buffer, events, streams,
TCP/DNS, HTTP servers, OS information and hashing have partial implementations.
HTTP/HTTPS clients, TLS clients, Fetch and spawn/execFile child processes now have
partial native implementations; audit their [contracts](../api/index.md), options
and [platform evidence](../status.md) rather than treating them as absent.
TLS/HTTPS servers, Agents, worker_threads, general util, Node's built-in test runner
and V8/native-addon integration remain unavailable.
Unsupported options on an otherwise implemented API can still block migration.

## Keep the ordinary module structure

Use .mjs for ESM, .cjs for CommonJS and package.json type for .js selection.
Supported node: imports retain their names; there is no mite: replacement namespace.

```js
import {readFile} from 'node:fs/promises';
import {createHash} from 'node:crypto';
const bytes = await readFile(process.argv[2]);
console.log(createHash('sha256').update(bytes).digest('hex'));
```

Run the same file with Node and your source-built Mite. Compare the output, exit
status and failure paths. [The CLI tutorial](../guides/mite-cli.md) includes argument
validation and error handling for this example.

CommonJS require works for supported CJS files/built-ins. Do not rely on synchronous
require(ESM), synthetic named exports from CommonJS, custom loader hooks or
require.resolve options. See [module behavior](../concepts/modules-and-packages.md).

## Install and build with existing tools

Mite is not a package manager or TypeScript compiler. Use the project's existing
Node/npm tooling to install locked dependencies and emit ordinary JavaScript.
Review installation/build scripts before running them; scripts can depend on
missing runtime APIs or execute native tools. A successful npm install does not
prove that the application runs in Mite.

The [npm matrix](../compatibility/npm-packages-0.1.md) records exact versions and
fixtures for the small measured package set. It does not certify a package's full
upstream test suite or the wider npm ecosystem.

## Adapt launch and tests deliberately

Replace the runtime in an individual launch command first:

```sh
build/dev/bin/mite path/to/application.mjs argument
```

Do not forward Node flags such as --inspect, --loader or --test. Check the
[CLI contract](../api/cli.md). Run external test orchestration with existing tools
until its runtime dependencies are supported. Cover errors, async ordering,
resource closure, file encodings and network behavior rather than checking output
from a happy path alone.

## Handle blockers

Reduce a missing feature to a small reproduction and record the exact required
signature/options. Keep Node for applications that require unsupported facilities;
there is no transparent fallback engine inside Mite. Do not replace cryptography,
TLS or process isolation with an insecure application workaround.

Before adopting a new revision, inspect compatibility changes, rerun your fixtures
on the real target OS and check the [support policy](../reference/support-policy.md).
Native macOS ARM64 is measured; Windows bring-up and Linux remain unverified.
