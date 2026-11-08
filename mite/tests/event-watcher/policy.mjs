/** Actual event-watcher factory with a controlled peer; no native filesystem events are claimed. */
import create from '../../modules/filesystem/event-watcher.mjs';
import { EventEmitter } from 'node:events';
import { fileURLToPath } from 'node:url';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let failure;
  try {
    action();
  } catch (error) {
    failure = error;
  }
  check(failure?.code === code, 'Expected ' + code + ', got ' + failure?.code);
  return failure;
}

function fixture(platform = 'darwin') {
  const state = {
    starts: [],
    stops: [],
    refs: [],
    next: 1,
    callbacks: new Map(),
    signals: new Map(),
    removed: 0
  };
  class Emitter extends EventEmitter {
    on(event, callback) {
      if (event === 'change') {
        state.onListener?.(this);
      }
      return super.on(event, callback);
    }
  }
  const native = {
    fsWatchStart(path, persistent, recursive, callback) {
      const id = state.next++;
      state.starts.push({ id, path, persistent, recursive });
      state.callbacks.set(id, callback);
      state.onStart?.(callback);
      if (state.failure) {
        throw state.failure;
      }
      return { id, status: state.status ?? 0 };
    },

    fsWatchStop(id) {
      state.stops.push(id);
      state.callbacks.delete(id);
    },

    fsWatchRef(id, ref) {
      state.refs.push({ id, ref });
    },

    fsWatchError(status, path) {
      return Object.assign(Error('Native watcher error'), {
        code: 'ENOENT',
        errno: status,
        syscall: 'watch',
        path,
        filename: path
      });
    }
  };
  const abort = {
    isSignal: (signal) => state.signals.has(signal),

    subscribe(signal, callback) {
      const entry = state.signals.get(signal);
      if (entry.aborted) {
        callback();
      } else {
        entry.callback = callback;
      }
      return () => {
        entry.callback = null;
        state.removed++;
      };
    }
  };
  const api = create(
    {
      native,
      Buffer,
      EventEmitter: Emitter,
      abort,
      platform,
      process: { nextTick: queueMicrotask }
    },
    {
      path(value) {
        if (value instanceof URL) {
          return fileURLToPath(value);
        }
        if (typeof value !== 'string' && !(value instanceof Uint8Array)) {
          throw Object.assign(TypeError('path'), { code: 'ERR_INVALID_ARG_TYPE' });
        }
        if (typeof value === 'string' ? value.includes('\0') : value.includes(0)) {
          throw Object.assign(TypeError('NUL'), { code: 'ERR_INVALID_ARG_VALUE' });
        }
        return value;
      },

      nativePath: (value) => value
    }
  );
  return { state, api };
}

{
  const { state, api } = fixture();
  const rows = [];
  const path = Buffer.from('/file');
  const a = api.watch(path, { persistent: 0, recursive: true, encoding: 'buffer' }, (...args) =>
    rows.push(args)
  );
  const b = api.watch('/file');
  path.fill(120);
  check(state.starts.length === 2 && a !== b, 'Each watch owns a separate native handle');
  check(
    state.starts[0].path.toString() === '/file' &&
      !state.starts[0].persistent &&
      state.starts[0].recursive,
    'Captured byte path and native flags'
  );
  const filename = Uint8Array.from([97, 255]);
  state.callbacks.get(1)(0, 3, filename);
  filename.fill(120);
  state.callbacks.get(1)(0, 2, null);
  check(
    rows[0][0] === 'rename' && Buffer.isBuffer(rows[0][1]) && rows[0][1][1] === 255,
    'Rename precedence and filename ownership'
  );
  check(rows[1][0] === 'change' && rows[1][1] === null, 'Null native filename preserved');
  check(a.ref() === a && a.unref() === a, 'Reference methods chain');
  let closes = 0;

  a.on('close', () => closes++);

  const late = state.callbacks.get(1);
  check(a.close() === undefined && closes === 0, 'Close queues event after native retirement');
  a.close();
  late(0, 2, Uint8Array.from([98]));
  await Promise.resolve();
  check(
    closes === 1 && rows.length === 2 && state.stops[0] === 1,
    'Close once with stale callbacks ignored'
  );
  b.close();
  console.log(
    'PASS independent ownership paths rename precedence null buffers references and deferred close'
  );
}

{
  const { state, api } = fixture();
  const rows = [];

  state.onStart = (callback) => callback(0, 2, Uint8Array.from([0xc3, 0xa9]));

  const watcher = api.watch('/early', 'hex', (event, filename) => rows.push([event, filename]));
  check(!rows.length, 'Native synchronous callback held until listener admission');
  await Promise.resolve();
  check(rows[0][1] === 'c3a9', 'Selected filename encoding');
  watcher.close();
  state.failure = Object.assign(Error('Native failed'), { code: 'ERR_NATIVE_START' });
  check(
    fails(() => api.watch('/failed', {}, () => rows.push('bad')), state.failure.code) ===
      state.failure,
    'Thrown native failure identity'
  );
  delete state.failure;
  state.status = -2;
  const error = fails(() => api.watch('/failed', {}, () => rows.push('bad')), 'ENOENT');
  check(
    error.errno === -2 && error.syscall === 'watch',
    'Native startup status maps genuine error'
  );
  await Promise.resolve();
  check(rows.length === 1, 'Failed admissions discard early callbacks');
  delete state.status;
  state.onStart = null;
  const failing = api.watch('/error');
  let errors = 0;
  let closes = 0;

  failing.on('error', (error) => {
    check(error.code === 'ENOENT', 'Event error retained');
    errors++;
  });

  failing.on('close', () => closes++);

  state.callbacks.get(state.starts.at(-1).id)(-2, 0, null);
  failing.close();
  await Promise.resolve();
  check(errors === 1 && closes === 0, 'Error retirement suppresses ordinary close event');
  console.log('PASS early encoding snapshots startup rollback and error-only retirement');
}

{
  const { state, api } = fixture();
  const pre = {};
  state.signals.set(pre, { aborted: true });
  let closed = 0;
  const watcher = api.watch('/pre', { signal: pre });

  watcher.on('close', () => closed++);

  check(state.stops.length === 0, 'Pre-aborted signal allows startup then defers close');
  await Promise.resolve();
  await Promise.resolve();
  check(
    closed === 1 && state.stops.length === 1 && state.removed === 1,
    'Pre-abort cleanup closes once and removes subscription'
  );
  const active = {};
  state.signals.set(active, { aborted: false });
  const live = api.watch('/live', { signal: active });
  const callback = state.signals.get(active).callback;
  callback();
  check(
    state.stops.length === 2 && state.signals.get(active).callback === null,
    'Later abort revokes handle immediately'
  );
  live.close();
  state.status = -2;

  fails(() => api.watch('/missing', { signal: pre }), 'ENOENT');

  delete state.status;

  state.onListener = (value) => value.close();

  fails(() => api.watch('/reentrant', () => {}), 'ERR_MITE_FS_WATCH_CLOSED');

  check(
    state.stops.at(-1) === state.starts.at(-1).id,
    'Listener-installation reentry releases actual native ID'
  );
  console.log('PASS private abort ownership pre-abort error priority and listener reentry cleanup');
}

{
  const { state, api } = fixture('linux');
  for (const encoding of ['unknown', 1, true, {}]) {
    fails(() => api.watch('/bad', { encoding }), 'ERR_INVALID_ARG_VALUE');
  }

  fails(() => api.watch('/bad', 1), 'ERR_INVALID_ARG_TYPE');

  fails(() => api.watch('/bad', {}, true), 'ERR_INVALID_ARG_TYPE');

  fails(() => api.watch('/bad', { signal: {} }), 'ERR_INVALID_ARG_TYPE');

  fails(() => api.watch('/bad', { signal: null }), 'ERR_INVALID_ARG_TYPE');

  fails(() => api.watch('/bad', { ignore: [] }), 'ERR_MITE_UNSUPPORTED_API');

  fails(() => api.watch('/bad', { recursive: true }), 'ERR_MITE_UNSUPPORTED_API');

  fails(() => api.watch('/bad\0'), 'ERR_INVALID_ARG_VALUE');

  check(!state.starts.length, 'Invalid/unsupported arguments never admit native handles');
  for (const encoding of [undefined, null, '', false, 0]) {
    const watcher = api.watch('/valid', { encoding, anything: true }, false);
    watcher.close();
  }
  console.log(
    'PASS strict validation native admission boundaries Linux recursion and Node option defaults'
  );
}

console.log('DONE controlled event watcher policy');
