# contentTracing

Status: partial main-process native tracing. Firefly records actual measured native
work in its own categories. It does not collect Chromium/V8, WebContent/helper,
JavaScript stack samples, heap dumps or system-wide traces. The current output is
legacy trace-event JSON, not Electron 45+'s Perfetto protobuf output.

```js
import {app, contentTracing} from 'firefly';
await app.whenReady();
await contentTracing.startRecording({
  included_categories: ['firefly.main', 'firefly.window'],
  recording_mode: 'record-continuously'
});
// Perform the operation you want to diagnose.
const {percentage} = await contentTracing.getTraceBufferUsage();
const file = await contentTracing.stopRecording();
console.log(file, percentage);
```

The Firefly main module, explicit Electron alias and CommonJS expose the same
singleton. Renderer/preload and standalone Mite do not receive tracing controls.
All methods return promises, and all require app readiness. Validation errors reject
the promise. Native event collection has no dependency on LibWeb or a modified
Ladybird tree; Firefly's own bridge supplies measured instrumentation.

## Methods

| Method | Return / current behavior |
| --- | --- |
| getCategories() | Promise<string[]>; fresh array of the three available categories |
| startRecording(options) | Promise<void>; start one recording, or preserve an existing active recording |
| getTraceBufferUsage() | Promise<{value, percentage}>; retained event count / 8,192 and that ratio times 100 |
| stopRecording([resultFilePath]) | Promise<string>; stop collection and save the snapshot, resolving with an absolute output path |

A repeated valid start does not replace active options or discard recorded data.
Starting while output is pending/saving rejects ERR_FIREFLY_TRACE_BUSY. Calling stop
without a recording rejects ERR_FIREFLY_TRACE_NOT_RECORDING. Concurrent stops reject
ERR_FIREFLY_TRACE_BUSY. Invalid output arguments leave the active recording intact.

Saving uses Mite's asynchronous filesystem. Native snapshot/JSON serialization is
bounded and happens on the main thread before disk I/O. A failed write retains the
same bounded snapshot: retry stopRecording(newPath), rather than starting another
recording. No capture continues while a failed snapshot awaits output.

An explicit nonempty string path is resolved against the current working directory
and written with ordinary writeFile overwrite semantics. It may therefore replace
an existing user-selected file. New files use mode 0600 on POSIX; existing-file modes
are unchanged. Empty/omitted paths create an exclusive trace.json in a native private
temporary directory (0700). The caller owns the returned file and its directory;
remove both when finished. Failed default writes may leave their private partial
file/directory for inspection; retry with an explicit new path. No file is created
at start, and active unsaved data is discarded on process shutdown.

## Options

Use one of two shapes; mixing them or passing unknown keys is explicitly unsupported.

| Shape | Supported fields |
| --- | --- |
| TraceConfig subset | included_categories, excluded_categories, recording_mode |
| Legacy categories/options | categoryFilter, traceOptions |

The default included list is ['*'], excluded list empty and mode record-until-full.
Each category list has at most 32 string entries. Values must be '*' or exact known
category names; partial wildcards and Chromium/V8 category names are rejected.
Exclusions win, including '*'. An empty modern included list records no durations.
Legacy categoryFilter is a comma-separated string: prefix an exclusion with '-'.
A legacy filter containing only exclusions includes '*' first.

recording_mode and legacy traceOptions support record-until-full or
record-continuously. The former retains the first 8,192 completed matching scopes;
the latter retains the most recent 8,192. Later discarded/overwritten events are
counted as droppedEvents in output metadata. Empty legacy flags use the default.
Other flags, including enable-sampling, fail with ERR_FIREFLY_UNSUPPORTED_API.

Legacy strings are bounded to 1,024 UTF-16 code units each; native configuration
is independently validated as an exact schema of at most 4 KiB. Output paths must
be strings without NUL, at most 32,768 UTF-16 code units; filesystem path errors
remain native filesystem errors. General argument errors use ERR_INVALID_ARG_TYPE,
ERR_INVALID_ARG_VALUE or ERR_OUT_OF_RANGE; native schema errors use TypeError.

## What events mean

| Category | Current measured native scopes |
| --- | --- |
| firefly.main | Each main event-loop pump and delivered bridge callback |
| firefly.window | Native BrowserWindow creation and destruction requests |
| firefly.ipc | Main bridge delivery of IPC events |

Each complete event has category/name, phase X, monotonic microsecond timestamp,
duration in microseconds, actual process PID and logical owner-thread ID 1. Metadata
names that thread Mite/Qt owner. Window creation duration covers its synchronous
native creation work, not time until painting. Destruction covers the deletion
request, not eventual OS destruction. Nested events may be emitted in completion
order; timestamps retain their actual start times.

The public gate currently accepts actual main-loop measurements. Window/IPC scope
instrumentation is implemented but still requires dedicated real-operation capture
acceptance. No helper/renderer acceptance is implied.

No URL, document contents, IPC payload, filesystem path, environment or command line
is recorded by these scopes. Tracing can add measurement overhead. A scope that
begins before recording or outlives stop/restart is excluded, preventing durations
from crossing recording generations. Recording/filters do not change renderer
privileges or the main callback error path.

## Remaining support and validation

Renderer/helper tracing, cross-process clock coordination, CPU/heap profiling,
memory_dump_config, other recording modes/options, included/excluded wildcard
patterns, configurable buffer sizes, streaming and Perfetto protobuf encoding remain
unfinished. enableHeapProfiling and chrome://tracing are unavailable. Code-OSS's
sampling/memory tracing commands still need compatible engine-specific work; an
export resolving during bootstrap does not prove those commands work.

```sh
cmake --build build/renderer --target check-firefly-content-tracing --parallel 3
```

The gate includes actual native clock/filter/buffer/retirement checks, Node/Mite
policy and real public main-host tracing/output without a BrowserWindow. Platform
acceptance is recorded in [PROGRESS](../PROGRESS.md); fresh native Linux/Windows
claims require their own execution. See [ADR 0129](../architecture/0129-main-tracing.md).

The API baseline is [Electron contentTracing](https://www.electronjs.org/docs/latest/api/content-tracing).
Firefly deliberately uses its own bounded categories and JSON format. Perfetto
[documents importing trace-event JSON](https://perfetto.dev/docs/getting-started/other-formats);
an actual external viewer import is not part of the current automated gate.
