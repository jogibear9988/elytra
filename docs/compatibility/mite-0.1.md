# Mite 0.1 development compatibility

This is a versioned inventory, not a claim of general Node.js compatibility.
Statuses describe this repository, not upstream LibJS's language conformance.
The current behavioral oracle is Node v24.15.0 on macOS ARM64.

| Surface | Status | Scope / known difference |
| --- | --- | --- |
| JavaScript file entry | partial | LibJS engine with CommonJS wrapper or explicit ESM format; no automatic ESM syntax detection |
| CLI script arguments / `-e` | partial | File execution, eval, help/version; other flags fail explicitly |
| `console.log`, `console.error` | partial | String conversion, space joining and immediate line flush (including pipes); no Node object inspection or format substitutions |
| `process` / `node:process` | partial | Native cwd/chdir, environment reads/writes/deletes/descriptors, pid/ppid, platform/arch, executable path, argv/argv0, exitCode and immediate exit tested; lifecycle events, stdio streams, signals and hrtime absent |
| Runtime identity | intentional difference | Reports Mite version/release, libuv and pinned LibJS versions; versions.node is absent |
| `global` | partial | Alias to `globalThis` |
| Promise jobs / `queueMicrotask` | partial | Shared FIFO queue, nested jobs, checkpoint after each native callback; nextTick runs in a separate priority FIFO; no rejection events |
| Unhandled rejections | partial | Nonzero exit after microtask drain; no process event API yet |
| ES modules | partial | `.mjs`, `package.json` type=module, relative/absolute file URLs, static/dynamic imports, live bindings, cycles, URL cache identity, import.meta url/filename/dirname/main and top-level await tested; no syntax auto-detection or import.meta.resolve yet |
| JSON modules | partial | Default exports with type=json import attributes, cache identity, mutations and syntax errors tested; unknown attribute names are rejected by LibJS before the host and their error codes differ from Node |
| CommonJS file modules | partial | Wrapper, require/resolve/cache/main, cycles, exports replacement, metadata, module.paths values, cache invalidation/retry, relative and directory main/index loading tested; eval globals, require.extensions, resolve options and synchronous require(ESM) unsupported |
| ESM/CJS interoperation | partial | Lazy default and module.exports namespace exports, shared JSON cache tested; static named CJS export detection unsupported |
| npm package resolution | partial | Bare/scoped node_modules lookup, main/index legacy loading, self references, exports/imports, conditions, exact/pattern mappings and array fallback tested; no NODE_PATH/global folders, custom conditions or resolve options; measured real-package scenarios are tracked in npm-packages-0.1.md |
| Package module-sync condition / recursion limits | intentional difference | module-sync inactive until synchronous require(ESM) is supported; recursive package resolution is capped at 64 depth steps |
| `setTimeout`, `setInterval`, clearing timers | partial | Native libuv timers; arguments, callback receiver, numeric/string cancellation, ref/unref/hasRef/close/dispose tested; refresh and timeout warnings unsupported |
| `process.nextTick` | partial | Separate rooted FIFO; CJS/ESM start ordering, argument/receiver semantics, nested queues, timer/fs checkpoints and rejection handling tested; no process rejection/lifecycle events |
| `setImmediate` | unsupported | Additional Node event-loop phases pending |
| `Buffer` / node:buffer | partial | Global/shared builtin constructor on LibJS byte storage; from/alloc, encodings/string writes, view sharing, copy/fill/concat/compare/search/swaps/JSON and integer/Float/Double/BigInt access in both byte orders tested; Blob/File/transcode/object URLs absent |
| Buffer allocation | intentional difference | Fresh zeroed allocUnsafe storage without pooling; allocation API limit 2 GiB minus one; legacy constructor deprecation warnings absent |
| URL / URLSearchParams / node:url | partial | Unmodified LibURL parsing, component setters, origin/toJSON, canParse/parse and live search params with form encoding/iteration tested; shared global/CJS/ESM constructors; file/path/byte-path, domain IDNA and HTTP option helpers tested; legacy URL API and Blob object URLs absent |
| TextEncoder/TextDecoder / node:util | partial | Shared global/CJS/ESM classes; UTF-8 encoding/encodeInto, label aliases, stateful decoding, BOM/fatal/stream flags, byte-view boundaries and detached/shared/resizable inputs tested; five legacy codecs use ICU to match Node; remaining util APIs and encoding stream classes absent |
| node:stream / node:stream/promises | partial | Pinned readable-stream 4.7.0; Readable/Writable/Duplex/Transform/PassThrough identity, push/read, setEncoding, backpressure/drain, cork/writev, callback/Promise pipeline, async iteration and isolated errors tested; helpers exported by the package are not all verified; WHATWG adapters, global AbortController and filesystem/stdio streams absent |
| Concurrent stream errors | intentional difference | Independent failing streams can report errors in a different relative order from Node 24; the differential suite retains both observed outputs explicitly. See ADR 0016 |
| node:string_decoder | partial | Pinned string_decoder 1.3.0 with own adapter for Node labels, base64url, byte views and end-state reset; 449 incremental/input comparisons pass; internal fields/descriptors and all invalid receivers unverified |
| node:dns / node:dns/promises | partial | OS-backed asynchronous lookup, numeric fast paths, all/family/hints, IDNA normalization, shared result ordering and builtin identity; 119 validation comparisons and local resolver/TCP workflows; Resolver/resolve*/lookupService/reverse/server configuration absent |
| DNS edge cases | intentional difference | Embedded NUL rejects with ENOTFOUND instead of resolving a prefix; empty-host deprecation warning omitted; no public request-wrapper cancellation. See ADR 0018 |
| node:net | partial | Native IPv4/IPv6 TCP Socket/Duplex and Server; numeric/hostname/default-localhost connections, family/hints, custom lookup, bounded family retries/events/aggregate failure; accept/read/write/half-close/backpressure/cancellation, address metadata, timeouts/ref/unref and server close waiting tested; IPC transports/local binding/AbortSignal/BlockList/reusePort/reset/reconnect and connection/drop policies remain absent |
| TCP limits and lifecycle | intentional difference | Final connection attempt relies on OS timeout; complete Happy Eyeballs parity unverified; default listen lacks IPv4 fallback; native writes copy bytes, bytesWritten excludes queued Duplex chunks; handles rooted until close or teardown; native Windows/Linux unverified. See ADRs 0017/0018 |
| node:http server | partial | llhttp 9.3.1 over native TCP; Server, IncomingMessage and response/header APIs; GET/POST, fixed/chunked bodies, trailers, HEAD/204, continue, keep-alive, pipelined response order, malformed framing, request read backpressure and server close tested; client/Agent/TLS/upgrade APIs absent |
| HTTP response/timeout policies | intentional difference | Reject mutated CR/LF header arrays and outgoing CL+TE; per-connection request deadlines and keep-alive timeout without Node's sweep/buffer; upgrades/CONNECT return 501. Pipeline aggregate pressure, packet-internal callback interleaving, chunk-extension budgets and full header/lifecycle parity remain open. See ADR 0019 |
| fetch | unsupported | Not installed as a global |
| Builtin module loading | partial | Shared CommonJS objects and node: ESM identities/named exports; require.resolve identifiers and node: cache bypass tested; syncBuiltinESMExports unsupported |
| node:events | partial | EventEmitter ordering/once/prepend/removal, mutation snapshots, hooks, symbols, errorMonitor/errors, counts/limits and Promise once tested; captureRejections=true and AbortSignal options reject explicitly; EventTarget/async iterators/async hooks absent |
| MaxListeners warnings | intentional difference | Emitted via a microtask with simplified stderr formatting; no process warning event or nextTick phase yet |
| node:path, node:path/posix, node:path/win32 | partial | Lexical resolve/normalize/join/relative, parse/format, dirname/basename/extname, isAbsolute and namespace conversion tested for both styles; shared CJS/ESM identities; matchesGlob absent; native Windows drive/cwd behavior unverified |
| node:fs / node:fs/promises | partial | Native whole-file read/write/append, access/exists, stat/lstat, mkdir/rmdir, unlink/rename/readdir; sync/callback/promises, scalar open/close/read/write/fstat/ftruncate/fsync/fdatasync, byte views, borrowed numeric descriptors for callback/sync whole-file APIs, returned Stats/BigIntStats/Dirent and CJS/ESM identities tested; file URL validation/conversion tested; vector I/O, streams/watch and other fs services absent |
| FileHandle | partial | Promise open; read/write, stat/truncate, sync/datasync, whole-file I/O, EventEmitter close event and async disposal tested; close waits active references; streams/vector/permission/time APIs absent; forced-GC fallback unverified and GC-close warnings absent |
| Filesystem limits/options | intentional difference | Async writes snapshot data; whole-file read cap 2 GiB minus one with EFBIG; AbortSignal/flush/recursive readdir/rmdir explicitly unsupported; existsSync deprecation warnings absent; numeric scalar write positions above the safe-integer range explicitly unsupported |
| Fatal errors with pending native work | intentional difference | Flush standard streams and terminate through _Exit; prevents blocked workers hanging fatal shutdown; pending writes may be abandoned. Normal exitCode drains referenced work |
| Other Node built-in modules | unsupported | Except events/process/path/buffer/fs/url/stream/string_decoder/net/dns, HTTP server and the util encoding classes, not implemented; known Node v24.15 names fail explicitly even when shadowed by local npm packages |
| Native addons, N-API, V8 internals, `process.binding()` | unsupported | Explicitly outside initial scope |

Missing APIs are absent and therefore raise JavaScript errors when used. There
are no success-returning placeholder implementations. The first fixture suite
compares stdout and exit status with Node, while requiring stderr for failures;
stack-trace formatting does not yet match Node. Native platform validation and
actual test results are recorded in `docs/PROGRESS.md`.

Behavior references: [CLI](https://nodejs.org/api/cli.html),
[globals](https://nodejs.org/api/globals.html),
[process](https://nodejs.org/api/process.html). Node is only a test reference and
is never linked into or invoked by Mite.
