/** Controlled preferences peer only: no real defaults writes, native posts or permission requests. */
import createSystemPreferences from '../../core/system-preferences/index.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let failure;
  try {
    action();
  } catch (caught) {
    failure = caught;
  }
  check(
    failure instanceof Error && failure.code === code,
    'Expected ' + code + ', got ' + failure?.code
  );
  return failure;
}

const DATA = 'ERR_FIREFLY_SYSTEM_PREFERENCES_DATA';
const UNSUPPORTED = 'ERR_FIREFLY_UNSUPPORTED_API';
const EXITED = 'ERR_FIREFLY_APP_EXITED';

function fixture() {
  const state = {
    exited: false,
    reads: [],
    writes: [],
    starts: [],
    stops: [],
    media: [],
    shutdowns: 0,
    defaults: new Map(),
    statuses: { camera: 'not-determined', microphone: 'denied', screen: 'unknown' }
  };
  const native = {
    preferencesGetDefault(name, type) {
      state.reads.push([name, type]);
      state.onRead?.();
      if (state.failure) {
        throw state.failure;
      }
      return (
        state.raw ??
        JSON.stringify({ value: state.defaults.get(name) ?? (type === 'string' ? '' : false) })
      );
    },

    preferencesSetDefault(name, type, json) {
      state.writes.push([name, type, json]);
      state.onWrite?.();
      if (state.failure) {
        throw state.failure;
      }
      state.defaults.set(name, JSON.parse(json));
    },

    preferencesMediaStatus(type) {
      state.media.push(type);
      state.onMedia?.();
      if (state.failure) {
        throw state.failure;
      }
      return state.raw ?? JSON.stringify({ value: state.statuses[type] });
    },

    preferencesSubscribe(id, kind, name) {
      state.starts.push({ id, kind, name });
      state.onStart?.(id, kind, name);
      if (state.failure) {
        throw state.failure;
      }
    },

    preferencesUnsubscribe(id, kind) {
      state.stops.push({ id, kind });
      state.onStop?.();
      if (state.stopFailure) {
        throw state.stopFailure;
      }
    },

    preferencesShutdown() {
      state.shutdowns++;
      state.onShutdown?.();
    }
  };

  const error = (code, message) => Object.assign(Error(message), { code });

  const api = createSystemPreferences(
    native,
    () => state.exited,
    error,
    (name) => {
      throw error(UNSUPPORTED, name);
    }
  );
  return { state, native, api, prefs: api.systemPreferences, error };
}

{
  const { prefs, state, api } = fixture();
  check(
    prefs.getUserDefault('Absent', 'string') === '',
    'Absent string default is native empty string'
  );
  check(
    prefs.getUserDefault('Absent', 'boolean') === false,
    'Absent boolean default is native false'
  );
  check(
    prefs.setUserDefault('NSWindowShouldTab', 'boolean', true) === undefined,
    'Setter returns void'
  );
  check(
    prefs.getUserDefault('NSWindowShouldTab', 'boolean') === true,
    'Boolean write reaches controlled native store'
  );
  prefs.setUserDefault('Name', 'string', '雪😀');
  check(prefs.getUserDefault('Name', 'string') === '雪😀', 'Unicode string roundtrip');
  check(
    state.writes[0][2] === 'true' && state.writes[1][2] === '"雪😀"',
    'JSON scalar rather than wrapper'
  );
  for (const [kind, expected] of Object.entries(state.statuses)) {
    check(
      prefs.getMediaAccessStatus(kind) === expected,
      'Real native status forwarded for ' + kind
    );
  }
  for (const value of ['granted', 'restricted', 'not-determined', 'denied', 'unknown']) {
    state.statuses.camera = value;
    check(prefs.getMediaAccessStatus('camera') === value, 'All five status values admitted');
  }
  api.stop();
  console.log('PASS pre-ready typed defaults scalar writes and media statuses');
}

{
  const { prefs, state, api } = fixture();
  for (const key of [undefined, null, {}, 1]) {
    fails(() => prefs.getUserDefault(key, 'string'), 'ERR_INVALID_ARG_TYPE');
  }
  for (const key of ['', 'x\0', '\ud800', '\udfff']) {
    fails(() => prefs.getUserDefault(key, 'string'), 'ERR_INVALID_ARG_VALUE');
  }

  fails(() => prefs.getUserDefault('😀'.repeat(1025), 'string'), 'ERR_OUT_OF_RANGE');

  fails(() => prefs.setUserDefault('x', 'string', '😀'.repeat(16385)), 'ERR_OUT_OF_RANGE');

  fails(() => prefs.setUserDefault('x', 'boolean', 1), 'ERR_INVALID_ARG_TYPE');

  fails(() => prefs.setUserDefault('x', 'string', true), 'ERR_INVALID_ARG_TYPE');

  fails(() => prefs.getUserDefault('x', null), 'ERR_INVALID_ARG_TYPE');

  for (const type of ['integer', 'float', 'double', 'url', 'array', 'dictionary', 'unknown']) {
    fails(() => prefs.getUserDefault('x', type), UNSUPPORTED);

    fails(() => prefs.setUserDefault('x', type, 1), UNSUPPORTED);
  }

  fails(() => prefs.getMediaAccessStatus('desktop'), 'ERR_INVALID_ARG_VALUE');

  fails(() => prefs.getMediaAccessStatus(null), 'ERR_INVALID_ARG_TYPE');

  fails(() => prefs.getUserDefault.call({}, 'x', 'boolean'), 'ERR_INVALID_ARG_TYPE');

  fails(() => prefs.subscribeNotification(undefined, () => {}), 'ERR_INVALID_ARG_TYPE');

  fails(() => prefs.subscribeNotification('x', null), 'ERR_INVALID_ARG_TYPE');

  for (const id of [null, '0', 0.5, -0, NaN, Infinity]) {
    fails(() => prefs.unsubscribeNotification(id), 'ERR_INVALID_ARG_TYPE');
  }

  fails(() => prefs.unsubscribeNotification(2147483648), 'ERR_OUT_OF_RANGE');

  for (const method of [
    'postNotification',
    'postLocalNotification',
    'postWorkspaceNotification',
    'askForMediaAccess',
    'registerDefaults',
    'removeUserDefault',
    'getAccentColor',
    'on',
    'emit'
  ]) {
    fails(() => prefs[method](), UNSUPPORTED);
  }

  fails(() => prefs.effectiveAppearance, UNSUPPORTED);

  check(
    state.reads.length + state.writes.length + state.starts.length + state.media.length === 0,
    'Invalid/unsupported calls never reach native'
  );
  api.stop();
  console.log('PASS receiver types UTF-8 bounds invalid IDs and explicit unsupported APIs');
}

{
  const { prefs, state, api } = fixture();
  const calls = [];
  const first = prefs.subscribeNotification('Change', (...args) => calls.push(args));
  const local = prefs.subscribeLocalNotification(null, (...args) => calls.push(args));
  const workspace = prefs.subscribeWorkspaceNotification('Activate', (...args) => calls.push(args));
  check(first === 0 && local === 1 && workspace === 2, 'Shared monotonic zero-based IDs');
  check(
    state.starts.map((entry) => entry.kind).join(',') === 'distributed,local,workspace',
    'Native families'
  );
  const input = {
    nested: [true, null, 12, '雪😀'],
    object: JSON.parse('{"__proto__":{"safe":true}}')
  };
  api.dispatch({ id: first, event: 'Change', userInfo: input, object: 'sender' });
  input.nested[0] = false;
  check(
    calls[0][0] === 'Change' && calls[0][1].nested[0] === true && calls[0][2] === 'sender',
    'Notification arguments are detached snapshot'
  );
  check(
    Object.hasOwn(calls[0][1].object, '__proto__') && {}.safe === undefined,
    'Prototype key preserved safely'
  );
  api.dispatch({ id: local, event: 'AnyName', userInfo: {}, object: '' });
  check(calls.length === 2, 'Null wildcard accepts arbitrary event');
  api.dispatch({
    id: local,
    event: 'JsonNul',
    userInfo: { ['key\0tail']: 'value\0tail' },
    object: ''
  });
  check(
    calls[2][1]['key\0tail'] === 'value\0tail',
    'JSON userInfo preserves escaped NUL in keys and values'
  );
  check(
    prefs.unsubscribeLocalNotification(first) === undefined && !state.stops.length,
    'Wrong-family removal is a no-op'
  );
  api.dispatch({ id: first, event: 'Change', userInfo: {}, object: '' });
  check(calls.length === 4, 'Wrong-family call did not lose subscription');
  check(prefs.unsubscribeNotification(first) === undefined, 'Unsubscribe returns void');
  prefs.unsubscribeNotification(first);
  prefs.unsubscribeNotification(-1);
  api.dispatch({ id: first, event: 'Change', userInfo: {}, object: '' });
  check(
    calls.length === 4 && state.stops.length === 1,
    'Removed callbacks and unknown IDs ignored'
  );
  api.stop();
  check(state.stops.length === 3, 'Shutdown releases remaining families');
  console.log('PASS zero IDs notification families snapshots wildcard and unsubscribe ownership');
}

{
  const { state, prefs, api, error } = fixture();
  const calls = [];

  state.onStart = (id) => {
    api.dispatch({ id, event: 'Early', userInfo: { sequence: 1 }, object: '' });
    api.dispatch({ id, event: 'Early', userInfo: { sequence: 2 }, object: '' });
  };

  const id = prefs.subscribeNotification('Early', (event, info) => calls.push(info.sequence));
  check(!calls.length, 'Synchronous provider events wait for completed admission');
  await Promise.resolve();
  check(calls.join(',') === '1,2', 'Early event order');
  state.failure = error('ERR_NATIVE_SUBSCRIBE', 'Native admission failed');
  check(
    fails(() => prefs.subscribeNotification('Early', () => calls.push(99)), state.failure.code) ===
      state.failure,
    'Native error identity'
  );
  await Promise.resolve();
  check(!calls.includes(99), 'Failed admission discards queued callback');
  delete state.failure;
  const removed = prefs.subscribeNotification('Early', () => calls.push(100));
  prefs.unsubscribeNotification(removed);
  await Promise.resolve();
  check(!calls.includes(100), 'Removed before admission microtask releases callback');
  prefs.unsubscribeNotification(id);

  state.onStart = (newId) => {
    for (let index = 0; index < 65; index++) {
      api.dispatch({ id: newId, event: 'Overflow', userInfo: {}, object: '' });
    }
  };

  fails(() => prefs.subscribeNotification('Overflow', () => calls.push(101)), DATA);

  await Promise.resolve();
  check(!calls.includes(101), 'Early overflow retires subscription');
  api.stop();
  console.log('PASS deferred early notifications admission rollback removal and early quota');
}

{
  const invalid = [
    undefined,
    null,
    [],
    { bad: undefined },
    { bad: Infinity },
    { bad: () => {} },
    { bad: new Date() },
    { bad: '\ud800' },
    { bad: 'x'.repeat(65536) }
  ];
  const cyclic = {};
  cyclic.self = cyclic;
  invalid.push(cyclic);
  let deep = {};
  for (let index = 0; index < 10; index++) {
    deep = { child: deep };
  }
  invalid.push(deep);
  invalid.push(
    Object.fromEntries(Array.from({ length: 1025 }, (_, index) => [String(index), index]))
  );
  const sparse = [];
  sparse[3] = true;
  invalid.push({ sparse });
  let reads = 0;
  invalid.push({
    get accessor() {
      reads++;
      return 1;
    }
  });
  for (const info of invalid) {
    const { prefs, api, state } = fixture();

    prefs.subscribeNotification('Name', () => {
      throw Error('Invalid packet delivered');
    });

    fails(() => api.dispatch({ id: 0, event: 'Name', userInfo: info, object: '' }), DATA);

    check(state.stops.length === 1, 'Malformed packet retires observer');
    api.dispatch({ id: 0, event: 'Name', userInfo: {}, object: '' });
    api.stop();
  }
  check(reads === 0, 'Nested notification accessors are not executed');
  for (const value of [
    { event: 'Wrong', userInfo: {}, object: '' },
    { event: 'Name', userInfo: {}, object: '\udfff' },
    { code: 'ERR_NATIVE_DELIVERY', message: 'Overflow' }
  ]) {
    const { prefs, api, state } = fixture();

    prefs.subscribeNotification('Name', () => {
      throw Error('Invalid event delivered');
    });

    fails(() => api.dispatch({ id: 0, ...value }), DATA);

    check(state.stops.length === 1, 'Native error retires active subscription');
    api.stop();
  }
  for (const raw of ['null', '{', '{}', '{"value":1}', '{"value":"bad\\ud800"}']) {
    const { prefs, state, api } = fixture();
    state.raw = raw;

    fails(() => prefs.getUserDefault('x', 'boolean'), DATA);

    fails(() => prefs.getMediaAccessStatus('camera'), DATA);

    api.stop();
  }
  console.log('PASS bounded JSON notifications malformed packets and strict native snapshots');
}

{
  const { state, prefs, api } = fixture();
  const ids = [];
  for (let index = 0; index < 64; index++) {
    ids.push(prefs.subscribeNotification('N', () => {}));
  }

  fails(() => prefs.subscribeNotification('N', () => {}), 'ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT');

  prefs.unsubscribeNotification(ids[0]);
  check(prefs.subscribeNotification('N', () => {}) === 64, 'Quota recovery uses new ID');

  state.onStop = () => {
    api.stop();
    api.dispatch({ id: 1, event: 'N', userInfo: {}, object: '' });
  };

  state.onShutdown = () => {
    throw Error('Native shutdown failure');
  };

  api.stop();
  api.stop();
  check(
    state.stops.length === 65 && state.shutdowns === 1,
    'Shutdown revokes all callbacks before reentrant native cleanup'
  );

  fails(() => prefs.getUserDefault('x', 'string'), EXITED);

  fails(() => prefs.subscribeNotification('x', () => {}), EXITED);

  for (const phase of ['Read', 'Write', 'Media', 'Start']) {
    const peer = fixture();

    peer.state['on' + phase] = () => {
      peer.state.exited = true;
      peer.api.stop();
    };

    const action =
      phase === 'Read'
        ? () => peer.prefs.getUserDefault('x', 'string')
        : phase === 'Write'
          ? () => peer.prefs.setUserDefault('x', 'boolean', true)
          : phase === 'Media'
            ? () => peer.prefs.getMediaAccessStatus('camera')
            : () => peer.prefs.subscribeNotification('x', () => {});
    fails(action, EXITED);
    check(peer.state.shutdowns === 1, 'Native reentry cannot return stale success');
  }
  const missing = fixture();
  {
    const peer = fixture();
    const observers = new Set();

    peer.state.onStart = (id) => {
      peer.state.exited = true;
      peer.api.stop();
      // Deliberately broken peer: admission occurs after the permanent shutdown callback.
      observers.add(id);
    };

    peer.state.onStop = () => {
      observers.delete(peer.state.stops.at(-1).id);
    };

    fails(
      () =>
        peer.prefs.subscribeLocalNotification('Late', () => {
          throw Error('Revoked late callback must not run');
        }),
      EXITED
    );

    check(
      observers.size === 0 && peer.state.stops.length === 2,
      'Late observer admission receives specific second removal'
    );
    check(
      peer.state.stops.every((record) => record.id === 0 && record.kind === 'local'),
      'Final cleanup preserves ID and family'
    );
    check(peer.state.shutdowns === 1, 'Late admission does not repeat whole-host shutdown');
    peer.api.dispatch({ id: 0, event: 'Late', userInfo: {}, object: '' });
  }
  delete missing.native.preferencesGetDefault;

  fails(() => missing.prefs.getUserDefault('x', 'string'), UNSUPPORTED);

  missing.api.stop();
  console.log('PASS subscription quota shutdown reentry stale results and missing native APIs');
}

console.log('DONE system preferences policy');
