# FileHandle.createReadStream

Status: **partial**, tested locally on macOS with real files under Node 24.15.0,
Mite and the non-GUI native GC host. Fresh Linux/Windows execution remains open.
This extends the existing native FileHandle read/close backend; it does not collect
the file with readFile or replace native filesystem operations with JavaScript.

```js
import { open } from 'node:fs/promises';

const file = await open('data.bin', 'r');
const input = file.createReadStream({ start: 10, end: 99 });
try {
  for await (const bytes of input) {
    console.log(bytes.length);
  }
} finally {
  input.destroy();
  await file.close();
}
```

The method returns a Node-compatible Readable subset supporting async iteration,
pipe and the existing data/error/end/close/destroy contract. bytesRead reports the
number of delivered file bytes; path is undefined for a descriptor-backed stream.
Full fs.ReadStream constructor/instance/field/method identity and path-based
fs.createReadStream/fs.createWriteStream are not implemented by this slice.

Options are encoding, start, end, highWaterMark, autoClose, emitClose and signal.
A string option selects encoding. start/end are nonnegative safe integers; end is
inclusive and may be Infinity. Without start the existing descriptor position is
used. highWaterMark defaults to 65,536 and accepts 1–1,048,576 bytes. Only one native
read per stream is in flight, with allocation no larger than that configured bound.
Read demand controls subsequent operations. Unknown options reject explicitly;
custom fs backends, complete Node argument/coercion rules and concurrent position
sharing are not accepted. Regular files are tested; FIFO/device and arbitrary
cancellation-provider parity are not established.

With autoClose:true (default), EOF/error destruction closes the FileHandle.
autoClose:false keeps it open at ordinary EOF. Explicit stream.destroy closes the
handle even when autoClose is false, matching the observed Node reference. Manual
FileHandle.close destroys attached read streams. Both paths wait for any outstanding
native read before completing descriptor retirement. A private close callback lets
the stream await an existing native close even after the descriptor number has retired. The public FileHandle.close Promise-identity
behavior remains the tested Node contract. Errors retain the existing filesystem/Readable contracts within
this subset.

The stream strongly retains its FileHandle while owned work is outstanding. GC
fallback does not replace explicit cleanup. Read errors, end/close timing, abort,
nonregular sources and full ReadStream-specific event/field parity remain partial;
no throughput, zero-copy or frontend transfer performance is claimed.

The mite-readline CTest covers native range reads and inclusive end, counts,
auto-close/retained ownership, explicit destruction, manual handle close during
an in-flight read, native-host GC, pre-aborted/active signals, encoded output and complete Unicode/CRLF file iteration through
[node:readline](readline.md). The 30,000-line fixture exceeds one native chunk by
many reads. No physical desktop test is involved. See
[ADR 0153](../architecture/0153-readline-file-streams.md).
