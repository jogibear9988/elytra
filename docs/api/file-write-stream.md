# fs.createWriteStream and FileHandle.createWriteStream

Status: **partial, locally tested on macOS**. All seven focused Node/Mite
native-file differential groups and the owned policy checks pass. The deliberate
pre-aborted file-side-effect difference remains documented below. The actual
compiled Code-OSS ZIP writer now passes with genuine yazl, source-analyzed
CommonJS named exports and independent archive CRC/extracted-byte verification.
util.inherits itself passes local differential acceptance. Fresh native Linux/Windows
writer execution is unverified.

```js
import { createWriteStream } from 'node:fs';

const output = createWriteStream('./result.txt', { encoding: 'utf8' });
output.on('error', (error) => console.error(error));
output.write('Grüße 🦋\n');
output.end('Finished\n');
await new Promise((resolve, reject) => {
  output.once('close', resolve);
  output.once('error', reject);
});
```

fs.createWriteStream(path[, options]) returns a real Writable/WriteStream backed by
native asynchronous open/write/close operations. fs.WriteStream and named imports
refer to this maintained implementation; CommonJS/default/ESM use the same API.
FileHandle.createWriteStream(options) uses that actual handle instead of reopening
a pathname. This does not implement path-based fs.createReadStream or the complete
Node filesystem stream API.

## Options

| Option | Current source contract |
| --- | --- |
| encoding | Writable string encoding; default utf8. A string options argument selects encoding. |
| flags | Existing native filesystem flags policy; default w. Append and positional behavior depend on the selected flags/OS. |
| mode | Existing mode policy; default 0666, subject to OS permissions/umask. Applies when creating a file. |
| fd | Nonnegative numeric descriptor or owned Mite FileHandle. Supplied descriptors bypass path open. |
| start | Optional nonnegative safe integer; advances after actual written bytes. Without it the descriptor position is used. |
| highWaterMark | Writable pressure threshold, default 16384. Respect write(false)/drain; it is not a hard memory cap. |
| autoClose | Default true; completion/error destruction retires the descriptor. False keeps the ordinary finished stream open until explicit cleanup. |
| emitClose | Writable close-event policy; default true. Suppressing close also suppresses listeners waiting on that event. |
| signal | Existing Writable AbortSignal integration, with the pre-aborted path difference described below. |

Unknown options fail explicitly, including custom fs backends and flush. Full Node
argument coercion, subclass/function-call wrapping such as graceful-fs, vector I/O,
permission/time APIs and arbitrary device/FIFO behavior are not accepted by this
slice. Calling similarly named APIs in another package is not package acceptance.

## Writes, events and ownership

The actual file opens asynchronously. Path-backed streams emit open(fd), then ready;
a supplied descriptor bypasses those path-open notifications. Writes issued before
open queue through Writable. Native requests contain at most 65,536 bytes; short
writes continue until the chunk is fully written. Zero-progress/EAGAIN retries are
bounded and eventually fail. bytesWritten counts completed bytes, excluding queued
bytes. path is undefined for descriptor-backed streams; fd becomes null after
stream destruction closes the descriptor. pending reflects absence of that fd.

Writable supplies ordering, callbacks, pipe, finish, pressure/drain and destruction.
finish means accepted writes completed; close follows descriptor retirement. A
request already submitted may finish after cancellation. For required output, await
completion; process.exit remains immediate. OS errors remain errors, never successful
zero-byte writes. Native worker/request limits remain in force; ignoring Writable
backpressure can still retain arbitrary application data in its JavaScript queue.

A FileHandle stream acquires a private lease spanning its entire queued write loop.
Closing/destruction waits for active work before releasing the lease and awaiting
native descriptor closure. Manual FileHandle.close asks the attached writer to end;
already queued writes drain before its descriptor is retired. Explicit stream.destroy
closes the descriptor even with autoClose:false in this subset. Callers supplying
numeric descriptors retain responsibility for coordinating concurrent external use;
this is not protection against another caller explicitly closing/reusing that fd.

A pre-aborted path stream deliberately does not open/create/truncate the target.
Node may leave an empty file from an already submitted open; Mite chooses absence
of that side effect. This is an intentional compatibility difference. Later abort
can leave partial output and does not roll back filesystem changes. The focused gate accepts the documented pre-aborted difference; exact arbitrary
late-abort timing/bytes are not a general Node compatibility guarantee.

The runner compares actual Node/Mite event observations and resulting file bytes,
including UTF-8, flags/start, numeric/handle ownership, manual-handle closure,
errors/abort and close behavior. No desktop test is needed:

```sh
python3 tooling/tests/test_file_write_stream.py --mite build/release-mite/bin/mite
node tooling/code_oss/probe-write-stream.mjs build/code-oss-nls-final build/release-mite/bin/mite
```

The second probe uses the unchanged compiled Code-OSS ZIP consumer and genuine
packages, with 34 verified compiled dependencies and independent archive
CRC/extracted-byte verification. It passes under actual Node and Mite. No extension
installation or Code-OSS workbench startup acceptance is claimed here. See [read streams](file-read-stream.md), [Writable](stream.md),
[filesystem APIs](../compatibility/mite-0.1.md) and the
[Node 24.15 contract](https://nodejs.org/download/release/v24.15.0/docs/api/fs.html#fscreatewritestreampath-options).
