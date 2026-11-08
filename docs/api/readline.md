# node:readline

Status: **partial**, tested locally on macOS against Node 24.15.0 and the non-GUI
native GC host. Fresh Linux/Windows execution remains unverified. The implemented
interface reads real streams; terminal editing and readline/promises are unavailable.

## Read file lines

```js
import { open } from 'node:fs/promises';
import { createInterface } from 'node:readline';

const file = await open('data.txt', 'r');
const input = file.createReadStream();
const lines = createInterface({ input, crlfDelay: Infinity });
try {
  for await (const line of lines) {
    console.log(line);
  }
} finally {
  lines.close();
  input.destroy();
  await file.close();
}
```

Bare readline, node:readline and CommonJS require share the same realm-local module.
createInterface(options) and new Interface(options) create EventEmitter instances.
The positional input/output/completer/terminal form is accepted within the same
nonterminal subset. Calling Interface without new and subclass/foreign-realm parity
are not implemented. Input must provide real readable-stream events and pause/resume.
The caller retains ownership of input/output: closing readline removes its listeners
and pauses input; it does not destroy the input or close the caller's file descriptor.
See [FileHandle streams](file-read-stream.md) for native descriptor ownership.

## Framing and lifecycle

The line event supplies text without LF, CR or CRLF separators. UTF-8 sequences
split between byte chunks are decoded incrementally. EOF emits a final nonempty
unterminated line. Like the tested Node reference, an incomplete final UTF-8 byte
sequence is discarded rather than creating an extra replacement-character line.
crlfDelay defaults to 100 ms; finite values are clamped to at least 100 ms and
Infinity pairs split CRLF regardless of delay. Actual elapsed time between separate
CR/LF chunks is tested; a pair within one input chunk stays paired through pressure.

pause()/resume() affect the actual input stream, emit their transition events and
return the interface. Reentrant handlers can pause again during resume. close() is
idempotent, detaches listeners, pauses input and emits close once. Mite immediately
stops pending text delivery after explicit close; complete Node late-batch ordering
is not claimed. Input errors forward to error listeners and reject pending iterator
reads, then retire the Mite interface. It does not silently turn a failure into EOF.
The signal option accepts the actual AbortSignal implementation; abort, including
an already-aborted signal, closes the interface and removes listeners.

The async iterator returns individual lines. A queue reaching 1,024 lines or
1,048,576 UTF-16 code units pauses input and defers remaining text parsing; it resumes
below its low-water marks. One completed line may exceed the text threshold before
pressure is applied. Iterator return/break closes the Mite interface and retires
pending reads. The observed Node 24.15.0 fixture retains its input listener after
return; tests keep this intentional cleanup difference explicit. Input errors and
multiple simultaneous consumer/event ordering outside tested cases remain partial.

## Prompts and explicit limits

getPrompt()/setPrompt(string), prompt() and question(query, callback) support basic
nonterminal output. question writes the query, consumes the next complete line for
its callback and does not emit that answer as an ordinary line event. A closed
interface rejects question with ERR_USE_AFTER_CLOSE. EOF without an answer does not
invent a response. Question option objects, completion/history/key handling and the
Promise question API are unavailable; multi-consumer/question combinations and full
Node overload/coercion parity are not claimed.

Supported options are input, output, terminal:false, crlfDelay, prompt and signal.
A requested terminal or output reporting isTTY rejects with ERR_MITE_UNSUPPORTED_API.
Completion/history and other options also reject explicitly. emitKeypressEvents,
cursorTo, moveCursor, clearLine, clearScreenDown, interface.write and getCursorPos
are explicit unsupported methods. Raw terminal modes, TTY signals, readline/promises,
full field/disposal/constructor parity and interactive shell acceptance remain open.

An individual input chunk is limited to 1 MiB of bytes or 1,048,576 string code units;
a line is limited to 1,048,576 UTF-16 code units. Excess input/line length rejects
with ERR_OUT_OF_RANGE and closes the interface. A producer that emits additional
chunks despite paused input while pending text remains is refused explicitly.
These are bounded-stream contracts, not complete Node memory/pressure parity or a
zero-copy/performance claim. Native file streams provide bounded chunks by default.

The mite-readline CTest compares real Node/Mite framing, delays, events, callback
questions and native AbortSignal behavior, plus read pressure, exception identity,
GC and 30,000-line real-file consumption. The actual pinned Code-OSS OS-release
module runs on the real platform; the macOS branch returns without Linux reads.
Its Linux file branch requires native Linux acceptance. The actual Firefly ready
path now passes readline and reaches missing MessageChannelMain; no workbench is
accepted. See [ADR 0153](../architecture/0153-readline-file-streams.md).

Reference: [Node readline API](https://nodejs.org/docs/latest-v24.x/api/readline.html).
