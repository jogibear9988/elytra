/** Controlled actual factory policy; no native filesystem polling acceptance is claimed here. */
import create from '../../modules/filesystem/stat-watcher.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
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

function fixture() {
  const state = { starts: [], stops: [], refs: [], next: 1, callbacks: new Map() };
  class Emitter extends EventEmitter {
    on(event, callback) {
      if (event === 'change') {
        state.onListener?.();
      }
      return super.on(event, callback);
    }
  }
  const native = {
    statWatchStart(name, interval, persistent, callback) {
      const id = state.next++;
      state.starts.push({ id, name, interval, persistent });
      state.callbacks.set(id, callback);
      state.onStart?.(callback);
      if (state.failure) {
        throw state.failure;
      }
      return { id, status: state.status ?? 0 };
    },

    statWatchStop(id) {
      state.stops.push(id);
      state.callbacks.delete(id);
    },

    statWatchRef(id, ref) {
      state.refs.push({ id, ref });
    },

    statWatchError(status, name) {
      return Object.assign(Error('native failure'), {
        code: 'EINVAL',
        errno: status,
        syscall: 'watchFile',
        path: name
      });
    }
  };
  const host = { native, EventEmitter: Emitter, process: { nextTick: queueMicrotask }, path };
  const api = create(host, {
    path(value) {
      if (value instanceof URL) {
        return fileURLToPath(value);
      }
      if (typeof value !== 'string' && !(value instanceof Uint8Array)) {
        throw Object.assign(TypeError('path'), { code: 'ERR_INVALID_ARG_TYPE' });
      }
      return value;
    },

    nativePath: (value) => value,

    stats: (value, bigint) => ({ size: bigint ? BigInt(value) : Number(value) })
  });
  return { state, api };
}

{
  const { state, api } = fixture();

  const callback = () => {};

  const first = api.watchFile(
    './controlled-stat',
    { interval: 0, persistent: 0, bigint: 1 },
    callback
  );
  const second = api.watchFile(
    path.resolve('./controlled-stat'),
    { interval: 'ignored', persistent: 'ignored', bigint: true },
    callback
  );
  check(first === second && state.starts.length === 1, 'Resolved paths share first options');
  check(
    state.starts[0].interval === 0 && state.starts[0].persistent === false,
    'Interval0 and persistent truthiness'
  );
  const received = [];

  first.on('change', (current, previous) => received.push([current, previous]));

  state.callbacks.get(1)(-2, 0, 7);
  check(
    received[0][0].size === 0 && received[0][1].size === 7,
    'Negative statuses convert snapshots instead of error'
  );
  check(typeof received[0][0].size === 'number', 'Only exact true enables bigint');
  check(first.ref() === first && first.unref() === first, 'Reference methods chain');
  const stops = [];

  first.on('stop', () => stops.push('stop'));

  const late = state.callbacks.get(1);
  api.unwatchFile('./controlled-stat');
  check(!stops.length && state.stops.length === 1, 'Native stop now, event deferred');
  late(0, 8, 7);
  await Promise.resolve();
  check(
    stops.length === 1 && received.length === 1,
    'Late callbacks ignored and stop emitted once'
  );
  first.stop();
  await Promise.resolve();
  check(stops.length === 1, 'Repeated stop no-op');
  console.log(
    'PASS shared watcher first options references deferred stop and stale callback guard'
  );
}

{
  const { state, api } = fixture();
  const calls = [];

  state.onStart = (callback) => {
    callback(-2, 0, 0);
    callback(0, 8, 0);
  };

  api.watchFile('early-stat', { bigint: true }, (current) => calls.push(current.size));

  check(!calls.length, 'Early callbacks wait for listener admission');
  await Promise.resolve();
  check(
    calls.length === 2 && calls[0] === 0n && calls[1] === 8n,
    'Early FIFO retains bigint choice'
  );
  api.unwatchFile('early-stat');
  state.failure = Object.assign(Error('native start threw'), { code: 'ERR_NATIVE_START' });
  const failure = fails(
    () => api.watchFile('failed-stat', {}, () => calls.push(99)),
    state.failure.code
  );
  check(failure === state.failure, 'Native thrown identity preserved');
  delete state.failure;
  state.status = -22;
  const system = fails(() => api.watchFile('failed-stat', {}, () => calls.push(99)), 'EINVAL');
  check(
    system.errno === -22 && system.syscall === 'watchFile',
    'Actual startup status mapping preserved'
  );
  await Promise.resolve();
  check(!calls.includes(99), 'Failed admission never delivers queued events');
  delete state.status;
  state.onStart = null;

  api.watchFile('failed-stat', {}, () => {});

  check(state.starts.length === 4, 'Failure does not poison shared path cache');
  api.unwatchFile('failed-stat');
  console.log('PASS early admission FIFO startup failures rollback and fresh retry');
}

{
  for (const phase of ['native', 'listener']) {
    const { state, api } = fixture();

    const remove = () => api.unwatchFile('reentrant-stat');

    if (phase === 'native') {
      state.onStart = remove;
    } else {
      state.onListener = remove;
    }

    fails(
      () =>
        api.watchFile('reentrant-stat', {}, () => {
          throw Error('Revoked callback');
        }),
      'ERR_MITE_STAT_WATCH_CLOSED'
    );

    check(
      state.stops.length === 1 && state.callbacks.size === 0,
      'Reentrant removal retires admitted native handle'
    );
    state.onStart = null;
    state.onListener = null;
    const watcher = api.watchFile('reentrant-stat', {}, () => {});
    check(watcher.constructor.name === 'StatWatcher', 'Reentry cache removed before retry');
    api.unwatchFile('reentrant-stat');
  }
  const { state, api } = fixture();

  const fn = () => {};

  const watcher = api.watchFile('specific-stat', {}, fn);
  api.watchFile('specific-stat', {}, fn);

  api.unwatchFile('specific-stat', () => {});

  check(
    watcher.listenerCount('change') === 2 && !state.stops.length,
    'Unknown listener does not remove others'
  );
  api.unwatchFile('specific-stat', fn);
  check(
    watcher.listenerCount('change') === 1 && !state.stops.length,
    'Specific removal removes one duplicate'
  );
  api.unwatchFile('specific-stat', fn);
  check(state.stops.length === 1, 'Final listener removal retires handle');
  console.log('PASS native and listener reentry cleanup duplicate listeners and owner retirement');
}

{
  const { state, api } = fixture();
  for (const interval of [null, undefined, '1', {}, 1n]) {
    fails(() => api.watchFile('invalid-stat', { interval }, () => {}), 'ERR_INVALID_ARG_TYPE');
  }
  for (const interval of [-1, 0.5, Infinity, NaN, 4294967296]) {
    fails(() => api.watchFile('invalid-stat', { interval }, () => {}), 'ERR_OUT_OF_RANGE');
  }
  check(state.starts.length === 0, 'Invalid intervals never admit native polling');
  const watcher = api.watchFile('default-stat', () => {});
  check(
    state.starts[0].interval === 5007 && state.starts[0].persistent === true,
    'Historical Node defaults'
  );
  api.unwatchFile('default-stat', null);
  check(watcher.listenerCount('change') === 0, 'Nonfunction unwatch removes all listeners');
  api.unwatchFile('unknown-stat');
  console.log('PASS interval validation default policy and unknown path removal');
}

console.log('DONE controlled stat watcher policy');
