# fs.watchFile and fs.unwatchFile

Status: **partial, locally tested on macOS**. Mite polls real filesystem metadata
through libuv. Node/Mite differential tests cover shared registrations, changes,
delete/recreate history, initially missing paths, Stats/BigIntStats, validation,
listener removal, reference ownership and natural exit. Fresh native Linux and
Windows acceptance remains unverified. This does not implement fs.watch or
fs.promises.watch, recursive directory watching or Code-OSS's complete watcher service.

```js
import { watchFile, unwatchFile } from 'node:fs';

const filename = './settings.json';
const changed = (current, previous) => {
  console.log(previous.size, current.size);
};
const watcher = watchFile(filename, { interval: 1000, persistent: false }, changed);
// When this observer is no longer needed:
unwatchFile(filename, changed);
```

## Registration and options

`watchFile(filename[, options], listener)` returns an EventEmitter StatWatcher.
String paths and file URLs resolve against the current working directory. Relative
and absolute spellings of the same resolved pathname share one watcher. As observed
in Node 24, Buffer paths are rejected during path resolution, despite being accepted
by other filesystem APIs. Embedded NULs and invalid listeners fail synchronously.

The first registration determines these options; later registrations on the same
resolved path add listeners and ignore their options:

| Option | Behavior |
| --- | --- |
| interval | Unsigned 32-bit integer milliseconds, default 5007. Zero is accepted and libuv clamps it to its minimum polling interval. |
| persistent | Default true. Falsy values unref the initial native handle; truthy values retain it. |
| bigint | Exact true selects BigIntStats; other values select numeric Stats, matching the observed Node constructor behavior. |

Polling observes metadata differences, not every write. Short-lived intermediate
states may disappear between samples. Watching a directory is not recursive.
Unknown option properties have no effect, matching Node's watchFile policy; they
never enable an unsupported watcher facility.

## Changes and missing paths

Each change listener receives `(current, previous)` Stats objects. A successful
initial stat establishes the baseline without emitting change. An initially
missing path emits one zero/epoch pair; repeated unchanged errors are suppressed.
Creating the file then supplies actual current metadata and a zero previous value.

Deletion emits zero current fields and epoch dates with the last real previous
snapshot. Recreation retains that same last real snapshot as previous. Native
stat errors therefore produce missing/unavailable metadata, rather than a fabricated
successful file or a terminal error event. Neither notification proves the file
remains accessible when a later operation runs.

## Removal, references and limits

`unwatchFile(filename, listener)` removes one matching registration, preserving
other observers and duplicate registrations. An omitted or nonfunction listener
removes all change listeners. Unknown paths/listeners are harmless. After the last
registration is removed, native polling stops and stop is emitted on nextTick.
The return value is undefined. StatWatcher.ref()/unref() return the watcher and
use Node-style shared-registration reference counts. Unreferenced watchers remain
active while other work keeps the runtime alive, but do not prevent natural exit.

Native admission is bounded to 256 active **or closing** handles per filesystem
host. Closing a watcher does not immediately return capacity while libuv still
owns an in-flight stat request. Admission failure throws a genuine native error,
including code, errno, syscall and path. JavaScript callbacks remain rooted while
active; stop/teardown retires those roots before native close completion. No JavaScript
callback runs after runtime teardown. Direct construction and private handle fields
are not a supported API.

```sh
python3 tooling/tests/test_stat_watcher.py --mite build/release-mite/bin/mite
```

See [filesystem compatibility](../compatibility/mite-0.1.md) and the
[Node watchFile reference](https://nodejs.org/download/release/v24.15.0/docs/api/fs.html#fswatchfilefilename-options-listener).
