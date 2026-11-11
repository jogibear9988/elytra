/** Actual utility children and native parent IPC, with no BrowserWindow or UI automation. */
import { app, utilityProcess, MessageChannelMain } from 'firefly';
import path from 'node:path';

const limited = process.versions.electron === undefined;
const children = [];
let stage = 'startup';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const wait = (duration) => new Promise((resolve) => setTimeout(resolve, duration));

const entry = path.join(import.meta.dirname, 'child.mjs');

const capture = (action) => {
  let error;
  try {
    action();
  } catch (value) {
    error = value;
  }
  check(error instanceof Error, 'Expected explicit rejection');
  return error;
};

const own = (child) => {
  children.push(child);
  return child;
};

const stop = () => {
  for (const child of children) {
    child.kill();
  }
};

if (!app.isReady()) {
  const error = capture(() => utilityProcess.fork(entry, ['unused'], { stdio: 'ignore' }));
  if (limited) {
    check(error.code === 'ERR_FIREFLY_APP_NOT_READY', 'fork readiness guard');
  }
}
const watchdog = setTimeout(() => {
  console.error('FAIL utilityProcess acceptance timed out at ' + stage);
  stop();

  setTimeout(() => app.exit(1), 100);
}, 15000);

app
  .whenReady()
  .then(async () => {
    const env = { ELYTRA_UTILITY_SENTINEL: 'owned-env' };
    if (process.platform === 'win32' && process.env.SystemRoot) {
      env.SystemRoot = process.env.SystemRoot;
    }
    const args = ['echo', '--literal-arg', 'Unicode-ä'];
    const child = own(
      utilityProcess.fork(entry, args, {
        cwd: import.meta.dirname,
        env,
        stdio: 'pipe',
        execArgv: [],
        allowLoadingUnsignedLibraries: false,
        respondToAuthRequestsFromMainProcess: false
      })
    );
    check(child.pid === undefined, 'PID unavailable before spawn');
    check(child.stdout !== null && child.stderr !== null, 'pipe streams available immediately');
    check(child.stdin == null, 'main exposes no writable utility stdin');
    const stdout = child.stdout;
    const stderr = child.stderr;
    const out = [];
    const err = [];
    const events = [];
    const messages = [];
    let outEnded = false;
    let errEnded = false;

    stdout.on('data', (chunk) => out.push(Buffer.from(chunk)));

    stderr.on('data', (chunk) => err.push(Buffer.from(chunk)));

    const outputClosed = new Promise((resolve) => stdout.once('close', resolve));
    const errorClosed = new Promise((resolve) => stderr.once('close', resolve));

    stdout.on('end', () => {
      outEnded = true;
    });

    stderr.on('end', () => {
      errEnded = true;
    });

    child.once('spawn', () => {
      check(Number.isInteger(child.pid) && child.pid > 0, 'real PID at spawn');
      events.push('spawn');
    });

    const waiters = new Map();

    child.on('message', (data) => {
      check(
        data && typeof data === 'object' && typeof data.kind === 'string',
        'parent receives raw message data'
      );
      messages.push(data);
      events.push('message:' + data.kind);
      waiters.get(data.kind)?.(data);
    });

    const until = (kind) =>
      messages.find((value) => value.kind === kind)
        ? Promise.resolve(messages.find((value) => value.kind === kind))
        : new Promise((resolve) => waiters.set(kind, resolve));

    const exited = new Promise((resolve) =>
      child.once('exit', (code) => {
        check(child.pid === undefined, 'PID retired in exit event');
        if (!outEnded) {
          check(child.stdout === stdout, 'live stdout retained through exit listener');
        }
        if (!errEnded) {
          check(child.stderr === stderr, 'live stderr retained through exit listener');
        }
        events.push('exit');
        resolve(code);
      })
    );
    const buffer = new ArrayBuffer(8);
    const bytes = new Uint8Array(buffer);
    bytes.set([1, 2, 3, 4]);
    const shared = { value: 'before' };
    const value = {
      kind: 'echo',
      bytes,
      view: new DataView(buffer, 1, 2),
      buffer,
      first: shared,
      second: shared
    };
    value.self = value;
    check(child.postMessage(value) === undefined, 'postMessage return contract');
    bytes[1] = 99;
    shared.value = 'after';
    stage = 'boot';
    const boot = await until('boot');
    check(
      events[0] === 'spawn' && boot.pid === child.pid && boot.ppid === process.pid,
      'spawn before actual child identity'
    );
    check(
      boot.type === 'utility' &&
        boot.parentPort.on === 'function' &&
        boot.parentPort.postMessage === 'function',
      'actual utility process parent endpoint'
    );
    check(
      limited
        ? typeof boot.versions.mite === 'string' &&
            boot.versions.node === undefined &&
            boot.versions.electron === undefined
        : typeof boot.versions.electron === 'string' && typeof boot.versions.node === 'string',
      'utility child runtime identity: native Mite/LibJS or actual Electron reference'
    );
    check(
      boot.argv[0] === boot.execPath &&
        boot.argv[1] === entry &&
        JSON.stringify(boot.argv.slice(2)) === JSON.stringify(args),
      'logical argv excludes private control arguments'
    );
    check(
      boot.cwd === import.meta.dirname && boot.execArgv.length === 0,
      'cwd and empty executable flags'
    );
    check(
      boot.env.sentinel === 'owned-env' && !boot.env.hasPath && !boot.env.hasHome,
      'explicit environment replaces inherited variables'
    );
    stage = 'echo';
    const echoed = await until('echo');
    check(
      Array.isArray(echoed.ports) &&
        echoed.ports.length === 0 &&
        echoed.eventKeys.includes('data') &&
        echoed.eventKeys.includes('ports'),
      'child event contains data/ports'
    );
    const copied = echoed.data;
    check(
      copied.self === copied && copied.first === copied.second && copied.first.value === 'before',
      'cross-process graph snapshot and aliases'
    );
    check(
      copied.bytes instanceof Uint8Array &&
        copied.view instanceof DataView &&
        copied.bytes.buffer === copied.buffer &&
        copied.view.buffer === copied.buffer,
      'cross-process binary types/aliases'
    );
    check(
      copied.bytes[1] === 2 && copied.view.getUint8(0) === 2 && buffer.byteLength === 8,
      'call-time copy without source detachment'
    );

    if (limited) {
      const ports = new MessageChannelMain();
      check(
        capture(() => child.postMessage(null, [ports.port2])).code ===
          'ERR_FIREFLY_UNSUPPORTED_API',
        'unavailable utility port transfer fails explicitly'
      );
      stage = 'local port ownership';
      const alive = new Promise((resolve) => ports.port2.once('message', resolve));
      ports.port2.start();
      ports.port1.postMessage('still-owned');
      check(
        (await alive).data === 'still-owned',
        'rejected utility handoff preserves endpoint ownership'
      );
      ports.port1.close();
      ports.port2.close();
      for (const options of [
        { execArgv: ['--expose-gc'] },
        { allowLoadingUnsignedLibraries: true },
        { respondToAuthRequestsFromMainProcess: true }
      ]) {
        const error = capture(() => utilityProcess.fork(entry, [], options));
        check(
          error.code === 'ERR_FIREFLY_UNSUPPORTED_API',
          'unsafe/unavailable option rejected before spawn'
        );
      }
    }
    stage = 'natural exit';
    child.postMessage({ kind: 'quit' });
    check((await exited) === 7, 'actual natural exit code');
    stage = 'stdio close';
    await Promise.all([outputClosed, errorClosed]);
    check(
      messages.some((message) => message.kind === 'last-before-exit'),
      'accepted final child frame survives process exit'
    );
    check(
      Buffer.concat(out).equals(Buffer.from('utility-out:ä\0tail\n')) &&
        Buffer.concat(err).equals(Buffer.from('utility-err:😀\n')),
      'native binary stdout/stderr'
    );
    await wait(0);
    check(
      child.stdout === null && child.stderr === null && child.pid === undefined,
      'retired public process/stdio properties'
    );
    check(child.kill() === false, 'kill after exit');
    check(
      child.postMessage(() => {}) === undefined,
      'post after exit is a no-op before serialization'
    );

    stage = 'kill';
    const killed = own(utilityProcess.fork(entry, ['kill'], { stdio: 'ignore' }));
    check(killed.stdout === null && killed.stderr === null, 'ignored pipes are null');
    const killedExit = new Promise((resolve) => killed.once('exit', resolve));
    await new Promise((resolve) => killed.once('spawn', resolve));
    check(killed.kill() === true, 'successful kill request');
    check(Number.isInteger(await killedExit) && killed.pid === undefined, 'killed child is reaped');
    check(killed.kill() === false, 'kill remains retired');

    // Fast failures stress spawn/exit ordering without relying on a long-running child.
    for (let attempt = 0; attempt < 8; ++attempt) {
      stage = 'missing module ' + attempt;
      const missing = own(
        utilityProcess.fork(path.join(import.meta.dirname, 'missing.mjs'), [], { stdio: 'pipe' })
      );
      let diagnostic = '';
      let spawnCount = 0;

      missing.stdout.on('data', () => {});

      missing.stderr.on('data', (chunk) => {
        diagnostic += chunk.toString();
      });

      missing.on('spawn', () => {
        spawnCount++;
      });

      const missingOutput = new Promise((resolve) => missing.stderr.once('close', resolve));
      const missingExit = await new Promise((resolve) => missing.once('exit', resolve));
      await missingOutput;
      await wait(0);
      check(
        spawnCount === 1 && missingExit === 1 && diagnostic.length > 0 && missing.pid === undefined,
        'missing module spawns exactly once, then retires permanently after exit'
      );
    }
    clearTimeout(watchdog);
    stop();
    console.log(
      'PASS public utilityProcess: real PID/argv/env/cwd/stdio, queued binary graph IPC, parentPort shape, natural exit/final data, kill and module failure'
    );
    app.exit(0);
  })
  .catch((error) => {
    console.error(error.stack ?? error);
    clearTimeout(watchdog);
    stop();

    setTimeout(() => app.exit(1), 100);
  });
