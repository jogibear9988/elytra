# fs.watch

Mite implements individually owned FSWatcher handles backed by libuv filesystem
events. Local macOS tests cover native file/directory changes, filename encodings,
recursive relative names, abort/close, references, admission limits and actual GC.
Fresh native Linux/Windows execution remains unverified. The unchanged compiled
Code-OSS nodejsWatcherLib component passes with real canonical paths and native
create/update/delete events; this is not acceptance of the complete watcher service
or workbench.

```js
import { watch } from 'node:fs';

const controller = new AbortController();
const watcher = watch('./workspace', { signal: controller.signal }, (type, filename) => {
  console.log(type, filename);
});
watcher.on('error', (error) => console.error(error));
// Release the native watcher when the workspace closes:
controller.abort();
```

## Arguments and results

`watch(filename[, options][, listener])` returns an EventEmitter FSWatcher.
String, file URL and Buffer paths are accepted; byte paths are snapshotted before
native admission. Each call owns its own handle even when paths match. A listener,
when supplied, receives the same arguments as a change event. It may also be
attached later with watcher.on('change', listener).

| Option | Behavior |
| --- | --- |
| encoding | Default utf8. A string options argument selects encoding. Supported Buffer encodings decode the actual filename bytes; buffer returns a fresh Buffer. Falsy values select utf8. |
| persistent | Default true; falsy values unref the handle. |
| recursive | Default false. True uses native libuv recursive events on macOS/Windows. Linux currently throws ERR_MITE_UNSUPPORTED_API; Node's separate recursive directory-tree implementation is not provided. |
| signal | A genuine AbortSignal closes the watcher when aborted. Cancellation uses private subscriptions; overriding public signal listeners cannot revoke native cleanup. |

Non-null ignore filters fail explicitly because Node's filter facility is not yet
implemented. Other unknown option properties have no effect, matching the observed
Node policy; they cannot enable a missing facility. Invalid paths, encodings,
listeners or signals fail synchronously. Native startup errors carry genuine code,
errno, syscall, path and filename fields. A pre-aborted signal still permits native
startup errors to surface before its deferred close.

## Events and lifecycle

change delivers `(eventType, filename)`, with eventType rename or change. The OS
may coalesce events or report a write as rename; neither event counts nor a
one-to-one mapping to filesystem operations is guaranteed. When libuv reports both
flags, rename wins, matching Node's native wrapper. Directory names are relative
to the watched path. Filenames can be null when the OS cannot identify the entry;
Mite preserves that absence instead of inventing a name. Applications must stat or
rescan when necessary and handle files disappearing before a later read.

close() returns undefined and retires native admission/callback roots immediately;
it emits one deferred close event. Repeated close, ref and unref after closure are
harmless. ref()/unref() return the watcher and control loop liveness independently
of GC retention. Symbol.dispose delegates to close. An asynchronous native failure
retires the handle and emits error, without the ordinary close event, matching
Node's native watcher error path. Unhandled errors retain normal EventEmitter behavior.

Native callback payloads are copied before libuv reuses their storage. One filename
is bounded to 1 MiB. Admission permits at most 256 active **or closing** event handles
per filesystem host; stat-poll watchers have their own limit. Native memory remains
owned through actual uv_close completion, and shutdown stops every active watcher
before the JS realm is retired. Unreferenced watchers continue receiving events
while other work keeps the runtime alive, but do not prevent natural process exit.

Filesystem events are OS/filesystem dependent: network shares, inode replacement
and platform-specific event loss remain limitations of this API. This does not
implement fs.promises.watch or promise-iterator queue/overflow policy.

```sh
python3 tooling/tests/test_event_watcher.py --mite build/release-mite/bin/mite
python3 tooling/tests/test_event_watcher_lifetime.py --mite build/release-mite/bin/mite --host build/release-mite/bin/mite-embedded-host
node tooling/code_oss/probe-event-watcher.mjs build/code-oss-nls-final build/release-mite/bin/mite
```

See [stat polling](stat-watcher.md), [canonical paths](realpath.md),
[Node fs.watch](https://nodejs.org/download/release/v24.15.0/docs/api/fs.html#fswatchfilename-options-listener)
and [libuv event guarantees](https://docs.libuv.org/en/v1.x/fs_event.html).
