# Mite runtime API

Status: partial Node-style runtime; tested on macOS ARM64 against Node 24.15.0.
See the [compatibility matrix](../compatibility/mite-0.1.md) for differences and
[getting started](../getting-started.md) for builds. This is the supported subset,
not a promise that every Node option or overload works.

See the [CLI contract](cli.md), [module concepts](../concepts/modules-and-packages.md)
and [Node migration guide](../migration/from-node.md) for application workflows.

## CLI and modules

```sh
mite app.mjs argument
mite app.cjs argument
mite -e "console.log('hello')"
mite --help
mite --version
```

Script arguments are available in `process.argv`. `.mjs` is ESM, `.cjs` is CommonJS;
`.js` follows package.json type. ESM supports static/dynamic imports, top-level await,
live bindings and JSON imports with `with {type: 'json'}`. CommonJS supports require,
module.exports, require.resolve/cache/main and cycles. ESM imports of CommonJS expose
the default/module.exports value; named-export detection and synchronous require(ESM)
are not implemented. Unsettled top-level await exits with status 13.

Bare packages resolve through node_modules, package exports/imports, self references
and legacy main/index. Global package folders, NODE_PATH and custom conditions are
unsupported. For implemented built-ins, `require('fs')` and `require('node:fs')` share
exports; ESM has fixed named exports. `syncBuiltinESMExports` is absent.

## Globals and process

Available globals include global/globalThis, console, process, Buffer, URL,
URLSearchParams, TextEncoder, TextDecoder, queueMicrotask, setTimeout/setInterval and
clearTimeout/clearInterval. `fetch` and global Web Crypto are absent.

- `console.log(...values)` / `console.error(...values)` convert values to strings
  and join with spaces; Node inspection and format substitutions are not implemented.
- `process.argv`, `argv0`, `execPath`, `pid`, `ppid`, `arch`, `platform`, `version`,
  `versions`, `release`, `env`; `cwd()`, `chdir(path)`, `nextTick(callback, ...args)`,
  `exit(code)` and `exitCode` provide basic process services.
- Identity reports Mite and the pinned LibJS/libuv versions; `versions.node` is absent.
- Process events, stdio streams, signals and child_process are not implemented.
- `queueMicrotask(callback)` and Promise jobs share a FIFO; nextTick uses its own
  priority queue. Complete Node event-loop phases are not claimed.
- Timer handles support `ref()`, `unref()`, `hasRef()`, `close()`, numeric conversion
  and disposal. `refresh()` and immediate timers are unsupported.

Unhandled rejections and fatal callbacks exit nonzero. Normal exitCode waits for
referenced work; fatal shutdown may abandon pending native writes.

## node:fs and node:fs/promises

Supported operation families include whole-file read/write/append, access, stat/lstat,
mkdir/rmdir, unlink, rename, readdir, scalar file-descriptor I/O and synchronization.

| API | Return / use |
| --- | --- |
| `readFile(path[, options], callback)` / `readFileSync(path[, options])` | Buffer by default; string with encoding |
| `writeFile(path, data[, options], callback)` / sync variant | Write data; flags such as `wx` support exclusive creation |
| `appendFile(path, data[, options], callback)` / sync variant | Append file data |
| `stat`, `lstat`, `fstat` and sync variants | Stats; bigint mode where supported |
| `readdir(path[, options], callback)` / sync variant | Names or Dirent records with withFileTypes |
| `access`, `exists`, `mkdir`, `rmdir`, `unlink`, `rename` and sync variants | Basic filesystem operations; exists callback takes one boolean |
| `open` / `openSync`, `close` / `closeSync` | Numeric descriptors |
| `read`, `write` and sync variants | Scalar byte-view/string I/O |
| `ftruncate`, `fsync`, `fdatasync` and sync variants | Descriptor truncation/synchronization |

The promises module supports open, readFile, writeFile, appendFile, access, stat,
lstat, mkdir, rmdir, unlink, rename, readdir and constants. `open()` returns FileHandle
with read/write, stat/truncate, sync/datasync, whole-file I/O, close and async disposal.
Closing waits for active operations. Callback/sync whole-file APIs also accept numeric
descriptors. File URL paths are supported and validated.

```js
import {readFile, writeFile} from 'node:fs/promises';
const bytes = await readFile('input.txt');
await writeFile('copy.txt', bytes, {flag: 'wx'});
```

Async writes snapshot input bytes. Whole-file reads are capped at 2 GiB-minus-one.
AbortSignal, flush, vector I/O, filesystem streams/watch, permission/time services
and recursive readdir/rmdir remain unsupported. Check the matrix before using options.

## node:buffer and encodings

`Buffer` is also global. Core construction, allocation, byte views, common string
encodings, copying/filling/searching/comparison, slicing and numeric access are
implemented in part. `SlowBuffer` is exported. Allocation policy uses the same
2 GiB-minus-one limit; uninitialized-memory semantics are intentionally restricted.
Use `Buffer.from(value[, encoding])`, `Buffer.alloc(size)` and `buffer.toString(encoding)`
for ordinary code. See the matrix and Buffer differential tests for full measured scope.

`TextEncoder` / `TextDecoder` are globals and util exports. `node:string_decoder`
exports `StringDecoder` for incremental text decoding. The encoding implementations
use existing native codecs plus owned adapters; exact Node error formatting differs.

## Paths, URLs and events

- `node:path`, `node:path/posix`, `node:path/win32`: resolve, normalize, isAbsolute,
  join, relative, dirname, basename, extname, format, parse, toNamespacedPath, sep,
  delimiter, posix and win32. Lexical Windows paths are tested on macOS; native
  Windows drive/current-directory behavior is unverified. matchesGlob is absent.
- `node:url`: URL, URLSearchParams, fileURLToPath, fileURLToPathBuffer, pathToFileURL,
  domainToASCII, domainToUnicode and urlToHttpOptions. Legacy URL parser APIs are absent.
- `node:events`: EventEmitter, synchronous on/once/prepend/removal, event names/counts,
  listener hooks/limits, errorMonitor and `once(emitter, eventName)` Promise helper.
  `captureRejections: true`, AbortSignal modes and async event iterators are unsupported.
- `node:util` currently provides only TextEncoder/TextDecoder, not the general util API.

## Streams

`node:stream` uses a pinned readable-stream implementation with native-host Buffer,
process and events adapters. Exports include Stream, Readable, Writable, Duplex,
Transform, PassThrough, pipeline, finished, compose and selected state helpers.
`node:stream/promises` exposes pipeline and finished. Export presence does not imply
all option modes are supported; AbortSignal integration is limited by the host.

```js
import {Readable, Transform, Writable} from 'node:stream';
import {pipeline} from 'node:stream/promises';
await pipeline(
  Readable.from(['a', 'b']),
  new Transform({transform(chunk, encoding, callback) {
    callback(null, chunk.toString().toUpperCase());
  }}),
  new Writable({write(chunk, encoding, callback) {
    console.log(chunk.toString()); callback();
  }})
);
```

## TCP, DNS and HTTP

- `node:net`: Socket, Server, createServer, connect/createConnection and isIP/isIPv4/
  isIPv6. TCP streams support listening, connecting, data, end/half-close, timeout,
  ref/unref, addresses and selected options. OS hostname lookup/custom lookup and
  partial family retry are available. Unix sockets, complete retry parity and TLS
  remain open.
- `node:dns` / `node:dns/promises`: lookup, getDefaultResultOrder and
  setDefaultResultOrder, plus ADDRCONFIG/V4MAPPED/ALL flags. Record query APIs are absent.
- `node:http`: createServer, Server, IncomingMessage, ServerResponse, OutgoingMessage,
  METHODS, STATUS_CODES, validateHeaderName, validateHeaderValue and maxHeaderSize.
  Request/response streams, headers, keep-alive and selected timeouts are implemented
  in part. Client APIs, upgrades, HTTPS and fetch are absent.

```js
import {createServer} from 'node:http';
createServer((request, response) => {
  response.setHeader('Content-Type', 'text/plain');
  response.end('Hello from Mite');
}).listen(3000, '127.0.0.1');
```

## node:os

Functions: arch, availableParallelism, endianness, freemem, homedir, hostname, loadavg,
platform, release, tmpdir, totalmem, type, uptime, version, machine, cpus, userInfo,
networkInterfaces, getPriority and setPriority. Constants: EOL, devNull, constants.

CPU/account/interface data are native snapshots. userInfo supports encoding options;
getPriority defaults to the current process. `setPriority(priority)` targets the
current process; `setPriority(pid, priority)` uses the explicit PID. Privileges and
platform rules apply. Constants derive from native SDK/libuv headers. Native
Windows/Linux values and behavior still need validation.

## node:crypto

See the [detailed crypto reference](crypto.md) for parameter/error tables,
resource lifetime, executable example and precise compatibility limits.

### randomBytes(size[, callback])

Without callback, return a Buffer of OS-generated random bytes. With callback, return
undefined and call `(error, buffer)`. Zero-size callback runs synchronously; nonempty
work runs off the JS thread and completes after the current Promise checkpoint.
Size is numeric, truncated toward an integer and bounded by 2 GiB-minus-one. Errors
never publish partial output. Nonempty callbacks have undefined as strict receiver,
rather than Node's internal job receiver.

### randomUUID([options])

Return a lowercase UUIDv4 from fresh OS entropy. `disableEntropyCache`, if supplied,
must be boolean; both values obtain fresh entropy because Mite has no UUID cache.

### createHash(algorithm[, options]) and Hash

Create an incremental Transform hash. `Hash(algorithm, options)` is also callable.
The existing native OpenSSL provider is used; unavailable algorithms reject explicitly.
Locally tested families are MD5, SHA1, SHA2, SHA3, BLAKE2 and SHAKE. This list is test
coverage, not a complete inventory or a recommendation of cryptographic algorithms.

| Method | Behavior |
| --- | --- |
| `hash.update(data[, inputEncoding])` | Return this; string or Buffer/typed-array/DataView raw bytes |
| `hash.digest([outputEncoding])` | Buffer by default, string for a recognized encoding; finalize |
| `hash.copy([options])` | Independent snapshot before finalization |
| Stream write/end/pipeline | Emit one digest chunk |

Raw ArrayBuffer input to update rejects; use a view. Ordinary update/digest/copy after
manual digest throws ERR_CRYPTO_HASH_FINALIZED. Unknown digest output labels fall
back to Buffer, matching the tested Node version. Symbol output coercion throws before
finalization. `options.outputLength` controls SHAKE bytes; fixed digests accept only
their natural length. Above Mite's Buffer limit it explicitly rejects. Native contexts
are freed on finalization/collection; secure-heap zeroization/FIPS/audit parity is not
claimed. HMAC, ciphers, signatures, keys, KDFs, getHashes and Web Crypto remain absent.

```js
import {createHash} from 'node:crypto';
const original = createHash('sha256').update('prefix');
const first = original.copy().update('one').digest('hex');
const second = original.update('two').digest('hex');
console.log(first, second);
```
