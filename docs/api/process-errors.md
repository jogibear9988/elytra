# Process exceptions and Promise rejections

Mite delivers process error events from its native LibJS/libuv event loop.
These are VM-local JavaScript events, separate from OS signal handling and
native crash reports. Import process from node:process or use the global object.
Firefly's main and utility processes use this runtime; document JavaScript does
not gain process or privileged event access.

```js
import process from 'node:process';

process.on('uncaughtExceptionMonitor', (error, origin) => {
  console.error(origin, error);
});

process.on('unhandledRejection', (reason, promise) => {
  console.error('Unhandled Promise rejection', reason);
});
```

## uncaughtException and uncaughtExceptionMonitor

Native timer, immediate, nextTick, queueMicrotask and I/O callback failures enter
the uncaught-exception path. The original thrown value and origin string are
passed to listeners. The origin is uncaughtException for ordinary callbacks and
unhandledRejection when a rejected Promise is promoted to a fatal exception.

Monitor listeners run before uncaughtException listeners, with the same error
and origin. A monitor alone does not suppress the default failure. If there is
no uncaughtException listener, the runtime reports the error and exits with status
1. When a listener handles the event, later owned jobs and native work can run;
the interrupted JavaScript callback does not resume after its throw.

An exception thrown by a monitor or uncaughtException listener is terminal and
exits with status 7. It is not recursively delivered to that handler. These
rules also apply to once/prepend listeners; ordinary EventEmitter receiver,
ordering and removal semantics are preserved.

## unhandledRejection

At the end of the Promise/nextTick checkpoint, a rejection without a handler
emits `(reason, promise)`. Both values retain their original identities. A catch
attached synchronously or during that checkpoint suppresses the notification.
Internal LibJS await/module handling is checked against the actual Promise state.

If an unhandledRejection listener exists, notification does not itself terminate
the process. Otherwise, the rejection is promoted to uncaughtException with
origin unhandledRejection. Native Error objects and other values with an own stack property are
preserved. Other reasons are wrapped in an Error named UnhandledPromiseRejection
with code ERR_UNHANDLED_REJECTION. Formatting of arbitrary object reasons is not
a complete Node inspector implementation.

Throwing from an unhandledRejection listener creates an ordinary uncaught exception
with origin uncaughtException. An uncaughtException listener can handle this
new failure; it is different from throwing while already delivering a fatal
exception event.

## rejectionHandled

When a previously notified Promise gains a handler later, this event receives
that same Promise once. Attaching a catch inside unhandledRejection is supported.
The callback runs after queued Promise reactions, rather than inside Promise.catch.
An exception in the notification callback follows ordinary uncaughtException
handling.

Pending notification batches root their values through listener execution and GC.
Reported Promises are tracked weakly after notification; the runtime does not keep
every unhandled Promise alive indefinitely. Late-handled tracking is removed when
its notification is delivered.

## Entry points and limits

A CommonJS or eval entry throw uses origin uncaughtException. A failed ESM entry,
including rejected top-level await, goes directly to uncaughtException with origin
unhandledRejection, even if an ordinary unhandledRejection listener is installed.
The runtime owns this entry completion once and does not report it again after
recovery. An unresolved top-level await retains the existing exit-status-13 policy.

Portable EventTarget error reporting retains its separate behavior.
Bootstrap/host initialization, exit-listener failures, native crashes and OS fault
signals retain their separate terminal policies. beforeExit, warning, capture
callbacks and configurable --unhandled-rejections modes remain unsupported.
Default PromiseRejectionHandledWarning output is not implemented; use an explicit
rejectionHandled listener. This is a partial process API, not full Node compatibility.

Reference behavior is defined by the [Node process events documentation](https://nodejs.org/docs/latest-v24.x/api/process.html#process-events)
and the actual pinned Node comparison tests. Fresh platform and real Code-OSS
workbench acceptance must be recorded separately from individual event tests.
