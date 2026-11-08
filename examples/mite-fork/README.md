# Forked JSON jobs

Run from the repository root:

```sh
build/release-mite/bin/mite examples/mite-fork/main.mjs
```

The parent starts a genuine Mite worker, snapshots three JSON jobs, receives their
results and disconnects. Output is `[2,4,6]`. The worker exits naturally and the
parent waits for its actual exit event. `node examples/mite-fork/main.mjs` runs
the same source under the Node reference.

This is JSON process IPC, not a worker thread, shared memory, renderer bridge or
Firefly utility process. See [the API](../../docs/api/child-process.md#forkmodulepath-args-options)
for supported options, bounds and platform acceptance.
