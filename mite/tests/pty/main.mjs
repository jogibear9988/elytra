/** Real slave PTY behavior shared with the unchanged pinned node-pty package. */
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { fileURLToPath } from 'node:url';
import { resolve } from 'node:path';
import { spawn as spawnVerifier } from 'node:child_process';

const [backend, python, directory, nonce, record] = process.argv.slice(2);
const pty =
  backend === 'mite:pty' ? await import('mite:pty') : createRequire(import.meta.url)(backend);
const peer = fileURLToPath(new URL('./peer.py', import.meta.url));

const delay = (milliseconds = 5) => new Promise((done) => setTimeout(done, milliseconds));

async function until(check) {
  for (let attempt = 0; attempt < 1600; attempt++) {
    if (check()) {
      return;
    }
    await delay();
  }
  throw Error('Owned PTY condition deadline');
}

assert.equal(typeof globalThis.gc, 'function', 'Run real native GC with --expose-gc');

async function verifyReaped(pid) {
  // A separate OS process observes ESRCH while the owning VM is still alive.
  // An exit callback alone does not establish that waitpid has reaped the child.
  const source =
    'import os,sys\ntry: os.kill(int(sys.argv[1]),0)\nexcept ProcessLookupError: sys.exit(0)\nexcept OSError: sys.exit(2)\nelse: sys.exit(1)';
  const verifier = spawnVerifier(python, ['-c', source, String(pid)], { stdio: 'ignore' });
  await new Promise((done, reject) => {
    verifier.once('error', reject);

    verifier.once('exit', (code, signal) => {
      try {
        assert.equal(signal, null);
        assert.equal(code, 0, 'Independent OS verifier must observe ESRCH after PTY exit');
        done();
      } catch (error) {
        reject(error);
      }
    });
  });
}

const children = [];
const deadline = setTimeout(() => {
  throw Error('Owned PTY fixture deadline');
}, 30000);

function spawn(suffix) {
  const token = nonce + '-' + suffix;
  const terminal = pty.spawn(python, ['-u', peer, token, record], {
    cwd: directory,
    env: { ...process.env, ELYTRA_OWNED_PTY: token },
    cols: 80,
    rows: 24,
    name: 'xterm-256color'
  });
  const state = {
    terminal,
    token,
    events: [],
    raw: '',
    exit: undefined,
    exits: 0,
    lastAtExit: false
  };
  children.push(state);
  let pending = '';

  terminal.onData((data) => {
    assert.equal(typeof data, 'string');
    state.raw += data;
    assert.ok(state.raw.length < 2 * 1024 * 1024);
    pending += data;
    let newline;
    while ((newline = pending.indexOf('\n')) >= 0) {
      const line = pending.slice(0, newline).replace(/\r$/, '');
      pending = pending.slice(newline + 1);
      const marker = line.indexOf('@@PTY ');
      if (marker >= 0) {
        const value = JSON.parse(line.slice(marker + 6));
        assert.equal(value.nonce, token);
        state.events.push(value);
      }
    }
  });

  terminal.onExit((event) => {
    state.exits++;
    state.exit = event;
    state.lastAtExit = state.events.some((value) => value.kind === 'last');
  });

  return state;
}

function write(state, command) {
  state.terminal.write(JSON.stringify(command) + '\n');
}

try {
  const main = spawn('main');
  await until(() => main.events.some((event) => event.kind === 'ready'));
  const ready = main.events.find((event) => event.kind === 'ready');
  assert.ok(Number.isInteger(main.terminal.pid) && main.terminal.pid > 0);
  assert.equal(main.terminal.pid, ready.pid);
  assert.deepEqual(ready.tty, [true, true, true]);
  assert.equal(ready.foreground, ready.group);
  assert.equal(ready.session, ready.pid);
  assert.equal(ready.group, ready.pid);
  assert.equal(ready.cwd, resolve(directory));
  assert.equal(ready.term, 'xterm-256color');
  assert.equal(ready.owned, main.token);
  assert.equal(ready.cols, 80);
  assert.equal(ready.rows, 24);
  assert.equal(main.terminal.cols, 80);
  assert.equal(main.terminal.rows, 24);
  assert.equal(typeof main.terminal.process, 'string');
  assert.ok(main.terminal.process.length > 0);
  console.log('PASS real slave PTY controlling terminal/session/foreground/metadata');

  let disposedData = 0;
  const subscription = main.terminal.onData(() => disposedData++);
  subscription.dispose();
  subscription.dispose();
  for (let sequence = 0; sequence < 16; sequence++) {
    write(main, { op: 'echo', sequence, text: 'owned Grüße 🌍 ' + sequence });
  }
  await until(() => main.events.filter((event) => event.kind === 'echo').length === 16);
  const echoes = main.events.filter((event) => event.kind === 'echo');
  for (let sequence = 0; sequence < 16; sequence++) {
    assert.equal(echoes[sequence].sequence, sequence);
    assert.equal(echoes[sequence].text, 'owned Grüße 🌍 ' + sequence);
  }
  assert.ok(
    main.raw.includes(JSON.stringify({ op: 'echo', sequence: 0, text: 'owned Grüße 🌍 0' })),
    'Actual terminal line-discipline echo'
  );
  assert.equal(disposedData, 0);
  console.log('PASS PTY terminal echo/UTF8/input-output FIFO/listener disposal');

  main.terminal.resize(101, 37);
  await until(() =>
    main.events.some((event) => event.kind === 'winch' && event.cols === 101 && event.rows === 37)
  );
  write(main, { op: 'size' });
  await until(() => main.events.some((event) => event.kind === 'size'));
  const dimensions = main.events.find((event) => event.kind === 'size');
  assert.equal(dimensions.cols, 101);
  assert.equal(dimensions.rows, 37);
  assert.ok(dimensions.winches > 0);
  assert.equal(main.terminal.cols, 101);
  assert.equal(main.terminal.rows, 37);
  console.log('PASS real PTY resize delivers kernel dimensions and SIGWINCH');

  main.terminal.pause();
  write(main, { op: 'burst' });
  await delay(75);
  assert.equal(main.events.filter((event) => event.kind === 'burst').length, 0);
  main.terminal.resume();
  await until(() => main.events.filter((event) => event.kind === 'burst').length === 128);
  for (const [sequence, event] of main.events.filter((event) => event.kind === 'burst').entries()) {
    assert.equal(event.sequence, sequence);
    assert.equal(event.text, 'Grüße 🌍' + 'x'.repeat(4096));
  }
  if (typeof globalThis.gc === 'function') {
    globalThis.gc();
  }
  write(main, { op: 'exit', code: 23 });
  await until(() => main.exit);
  assert.equal(main.exit.exitCode, 23);
  assert.equal(main.exits, 1);
  assert.equal(main.lastAtExit, true);

  await verifyReaped(ready.pid);

  console.log('PASS PTY pause/resume preserves full FIFO and natural exit drains/reaps');

  const interrupted = spawn('interrupt');
  await until(() => interrupted.events.some((event) => event.kind === 'ready'));
  interrupted.terminal.write('\x03');
  await until(() => interrupted.exit);
  assert.equal(interrupted.exit.exitCode, 42);
  assert.equal(interrupted.events.find((event) => event.kind === 'signal').signal, 2);
  assert.equal(interrupted.exits, 1);
  console.log('PASS PTY control-C uses actual terminal SIGINT delivery');

  const killed = spawn('kill');
  await until(() => killed.events.some((event) => event.kind === 'ready'));
  let disposedExit = 0;
  const exitSubscription = killed.terminal.onExit(() => disposedExit++);
  exitSubscription.dispose();
  killed.terminal.kill('SIGTERM');
  await until(() => killed.exit);
  assert.equal(killed.exit.signal, 15);
  assert.equal(killed.exits, 1);
  assert.equal(disposedExit, 0);

  await verifyReaped(killed.terminal.pid);

  console.log('PASS PTY kill/exit disposal and owned child reap');
} finally {
  clearTimeout(deadline);
  for (const child of children) {
    if (!child.exit) {
      try {
        child.terminal.kill('SIGKILL');
      } catch {}
    }
  }
}
