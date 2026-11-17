/** Explicit Mite bounds and native active-handle GC ownership, separate from Node policy. */
import assert from 'node:assert/strict';
import { fileURLToPath } from 'node:url';
import { spawn } from 'mite:pty';

const [, python, cwd, nonce, record] = process.argv.slice(2);
const peer = fileURLToPath(new URL('./peer.py', import.meta.url));
const args = ['-u', peer, nonce + '-policy', record];
const options = {
  name: 'xterm-256color',
  cwd,
  env: { ...process.env, ELYTRA_OWNED_PTY: nonce + '-policy' },
  cols: 80,
  rows: 24
};
for (const patch of [
  { unknown: true },
  { uid: 0 },
  { gid: 0 },
  { useConpty: true },
  { encoding: 'hex' }
]) {
  assert.throws(() => spawn(python, args, { ...options, ...patch }), {
    code: 'ERR_MITE_UNSUPPORTED_API'
  });
}
for (const value of [0, -1, 65536, NaN, 1.5]) {
  assert.throws(() => spawn(python, args, { ...options, cols: value }), {
    code: 'ERR_OUT_OF_RANGE'
  });
}

assert.throws(() => spawn(python + '\0', args, options), { code: 'ERR_INVALID_ARG_VALUE' });

assert.throws(() => spawn(python, Array(1025).fill('x'), options), {
  code: 'ERR_INVALID_ARG_VALUE'
});

assert.throws(() => spawn(python, args, { ...options, env: { BAD: false } }), {
  code: 'ERR_INVALID_ARG_TYPE'
});

assert.throws(() => spawn(cwd + '/missing-executable', args, options), { code: 'ENOENT' });

assert.throws(() => spawn(python, args, { ...options, cwd: cwd + '/missing-directory' }), {
  code: 'ENOENT'
});

console.log('PASS PTY unsupported/options bounds fail before spawning');

const state = { raw: '', exit: undefined, exits: 0 };

function create() {
  const terminal = spawn(python, args, {
    ...options,
    encoding: null,
    useConpty: false,
    useConptyDll: false,
    conptyInheritCursor: false
  });

  terminal.onData((data) => {
    assert.equal(Buffer.isBuffer(data), true);
    state.raw += data.toString('utf8');
  });

  terminal.onExit((value) => {
    state.exit = value;
    state.exits++;
  });

  assert.throws(() => terminal.write({}), { code: 'ERR_INVALID_ARG_TYPE' });

  assert.throws(() => terminal.resize(0, 24), { code: 'ERR_OUT_OF_RANGE' });

  assert.throws(() => terminal.write(Buffer.alloc(1048577)), { code: 'ENOBUFS' });

  return new WeakRef(terminal);
}

state.weak = create();

const delay = () => new Promise((done) => setTimeout(done, 5));

async function until(check) {
  for (let index = 0; index < 1600; index++) {
    if (check()) {
      return;
    }
    await delay();
  }
  throw Error('PTY GC ownership deadline');
}

try {
  await until(() => state.raw.includes('@@PTY '));
  await delay();
  globalThis.gc();
  assert.ok(
    state.weak.deref(),
    'Native callback roots active PTY without an external strong wrapper'
  );
  state.weak.deref().destroy();

  assert.throws(() => state.weak.deref().write('after destroy'), { code: 'ERR_MITE_PTY_CLOSED' });

  await until(() => state.exit);
  assert.equal(state.exits, 1);
  assert.equal(state.weak.deref().process, '');
  await until(() => {
    globalThis.gc();
    return state.weak.deref() === undefined;
  });
  console.log('PASS PTY active native rooting and post-exit callback reclamation');
} finally {
  if (!state.exit) {
    try {
      state.weak.deref()?.kill('SIGKILL');
    } catch {}
  }
}
