/** A real utility entrypoint: identity, stdio and parentPort snapshots are not simulated. */
const parent = process.parentPort;
const mode = process.argv[2];
const watchdog = setTimeout(() => process.exit(23), 8000);
parent.postMessage({
  kind: 'boot',
  pid: process.pid,
  ppid: process.ppid,
  argv: process.argv,
  execPath: process.execPath,
  execArgv: process.execArgv,
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
        } else if (event.data?.kind === 'quit') {
          parent.postMessage({ kind: 'last-before-exit' });
          clearTimeout(watchdog);
          process.exit(7);
        }
      }),
    20
  );
}
