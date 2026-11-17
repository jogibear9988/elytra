/** A real utility entrypoint: identity, stdio and parentPort snapshots are not simulated. */
const parent = process.parentPort;
const mode = process.argv[2];
const watchdog = setTimeout(() => process.exit(23), 8000);
let collected = false;
if (mode === 'gc') {
  if (typeof globalThis.gc !== 'function') {
    throw new Error('Utility gc opt-in missing');
  }
  globalThis.gc();
  collected = true;
}
parent.postMessage({
  kind: 'boot',
  pid: process.pid,
  ppid: process.ppid,
  argv: process.argv,
  execPath: process.execPath,
  execArgv: process.execArgv,
  gcType: typeof globalThis.gc,
  collected,
  cwd: process.cwd(),
  type: process.type,
  versions: process.versions,
  env: {
    sentinel: process.env.ELYTRA_UTILITY_SENTINEL,
    hasPath: Object.hasOwn(process.env, 'PATH'),
    hasHome: Object.hasOwn(process.env, 'HOME')
  },
  parentPort: { on: typeof parent.on, postMessage: typeof parent.postMessage }
});
if (mode === 'kill') {
  setInterval(() => {}, 1000);
} else {
  // Parent sends before spawn/handler installation. Native parent delivery must queue it.
  setTimeout(
    () =>
      parent.on('message', (event) => {
        if (event.data?.kind === 'echo') {
          process.stdout.write('utility-out:ä\0tail\n');
          process.stderr.write('utility-err:😀\n');
          parent.postMessage({
            kind: 'echo',
            data: event.data,
            ports: event.ports,
            eventKeys: Object.keys(event)
          });
        } else if (event.data?.kind === 'port-handoff') {
          if (event.ports.length !== 1) {
            throw new Error('Expected one actual transferred port');
          }
          const port = event.ports[0];
          let next = 0;

          port.on('message', ({ data }) => {
            if (
              data.kind !== 'port-ping' ||
              data.sequence !== next ||
              !(data.bytes instanceof Uint8Array)
            ) {
              throw new Error('Invalid transferred port message or detached owner delivery');
            }
            port.postMessage({ kind: 'port-echo', sequence: next, bytes: data.bytes });
            data.bytes.fill(211);
            ++next;
            if (next === 2) {
              port.postMessage({ kind: 'port-final' });
              port.close();
            }
          });

          port.postMessage({ kind: 'port-ready' });
          port.start();
        } else if (event.data?.kind === 'quit') {
          parent.postMessage({ kind: 'last-before-exit' });
          clearTimeout(watchdog);
          process.exit(7);
        }
      }),
    20
  );
}
