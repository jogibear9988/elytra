/** Controlled-native policy unit only: this fixture never creates an OS power assertion. */
import createPowerSaveBlocker from '../../core/power-save-blocker/index.mjs';

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
  check(
    failure instanceof Error && failure.code === code,
    'Expected ' + code + ', got ' + failure?.code
  );
  return failure;
}

function fixture() {
  const state = {
    exited: false,
    starts: [],
    stops: [],
    queries: [],
    shutdowns: 0,
    ids: new Set(),
    next: 0
  };
  const native = {
    powerSaveBlockerStart(type) {
      state.starts.push(type);
      state.onStart?.();
      if (state.failure) {
        throw state.failure;
      }
      const id = state.next++;
      state.ids.add(id);
      return Object.hasOwn(state, 'startResult') ? state.startResult : id;
    },

    powerSaveBlockerStop(id) {
      state.stops.push(id);
      state.onStop?.();
      if (state.failure) {
        throw state.failure;
      }
      return Object.hasOwn(state, 'booleanResult') ? state.booleanResult : state.ids.delete(id);
    },

    powerSaveBlockerIsStarted(id) {
      state.queries.push(id);
      state.onQuery?.();
      if (state.failure) {
        throw state.failure;
      }
      return Object.hasOwn(state, 'booleanResult') ? state.booleanResult : state.ids.has(id);
    },

    powerSaveBlockerShutdown() {
      state.shutdowns++;
      state.ids.clear();
      state.onShutdown?.();
    }
  };

  const error = (code, message) => Object.assign(Error(message), { code });

  const api = createPowerSaveBlocker(
    native,
    () => state.exited,
    error,
    (name) => {
      throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
    }
  );
  return { state, native, api, blocker: api.powerSaveBlocker };
}

{
  const { state, blocker } = fixture();
  // There is deliberately no app-ready input: Electron permits these calls before readiness.
  const first = blocker.start('prevent-app-suspension');
  const second = blocker.start('prevent-display-sleep');
  check(first === 0 && second === 1, 'Zero is a valid first ID');
  check(blocker.isStarted(first) && blocker.isStarted(second), 'Native requests are visible');
  check(blocker.stop(second) && !blocker.stop(second), 'Stop reports native ownership');
  check(blocker.isStarted(first), 'Stopping one request preserves the other');
  check(!blocker.isStarted(-1) && !blocker.stop(-2147483648), 'Signed unknown IDs return false');
  check(!blocker.isStarted(2147483647), 'Maximum signed ID accepted');
  state.ids.add(42);
  check(
    blocker.isStarted(42) && blocker.stop(42),
    'Facade consults native registry rather than shadow state'
  );
  console.log('PASS pre-ready zero IDs native ownership and signed unknown IDs');
}

{
  const { state, blocker } = fixture();
  for (const method of ['start', 'stop', 'isStarted']) {
    fails(() => blocker[method].call({}, 0), 'ERR_INVALID_ARG_TYPE');
  }
  for (const type of [undefined, null, 1, {}, new String('prevent-app-suspension')]) {
    fails(() => blocker.start(type), 'ERR_INVALID_ARG_TYPE');
  }
  for (const type of ['', 'screen', 'prevent-display-sleep\0']) {
    fails(() => blocker.start(type), 'ERR_INVALID_ARG_VALUE');
  }
  for (const method of ['stop', 'isStarted']) {
    for (const value of [undefined, null, '0', {}, 0.5, NaN, Infinity, -Infinity, -0]) {
      fails(() => blocker[method](value), 'ERR_INVALID_ARG_TYPE');
    }
    for (const value of [-2147483649, 2147483648]) {
      fails(() => blocker[method](value), 'ERR_OUT_OF_RANGE');
    }
  }
  check(
    !state.starts.length && !state.queries.length && !state.stops.length,
    'Invalid input never reaches native'
  );
  console.log('PASS receivers types int32 bounds and negative-zero validation');
}

{
  for (const [method, nativeName, value] of [
    ['start', 'powerSaveBlockerStart', 'prevent-app-suspension'],
    ['stop', 'powerSaveBlockerStop', 0],
    ['isStarted', 'powerSaveBlockerIsStarted', 0]
  ]) {
    const { native, blocker, state } = fixture();
    delete native[nativeName];

    fails(() => blocker[method](value), 'ERR_FIREFLY_UNSUPPORTED_API');

    check(state.ids.size === 0, 'Missing native methods do not admit requests');
  }
  for (const method of ['start', 'stop', 'isStarted']) {
    const { state, blocker } = fixture();
    state.failure = Object.assign(Error('OS operation failed'), {
      code: 'ERR_FIREFLY_POWER_SAVE_CREATE'
    });
    const failure = fails(
      () => blocker[method](method === 'start' ? 'prevent-display-sleep' : 0),
      state.failure.code
    );
    check(
      failure === state.failure && state.ids.size === 0,
      'Native error preserved without fake admission'
    );
  }
  console.log('PASS unavailable native APIs and native failure identity');
}

{
  for (const result of [undefined, null, -1, -0, 2147483648, 0.5, NaN, Infinity, '0', {}, true]) {
    const { state, blocker } = fixture();
    state.startResult = result;

    fails(() => blocker.start('prevent-app-suspension'), 'ERR_FIREFLY_POWER_SAVE_DATA');

    check(
      state.shutdowns === 1 && !state.ids.size,
      'Malformed admission revokes and cleans native'
    );

    fails(() => blocker.isStarted(0), 'ERR_FIREFLY_APP_EXITED');
  }
  for (const method of ['stop', 'isStarted']) {
    for (const result of [undefined, null, 0, 1, 'false', {}, []]) {
      const { state, blocker } = fixture();
      blocker.start('prevent-app-suspension');
      state.booleanResult = result;

      fails(() => blocker[method](0), 'ERR_FIREFLY_POWER_SAVE_DATA');

      check(state.shutdowns === 1 && !state.ids.size, 'Malformed bool permanently revokes facade');

      fails(() => blocker.start('prevent-app-suspension'), 'ERR_FIREFLY_APP_EXITED');
    }
  }
  console.log('PASS malformed native snapshots permanently revoke and release');
}

{
  {
    const { state, blocker } = fixture();
    state.exited = true;

    fails(() => blocker.start('prevent-app-suspension'), 'ERR_FIREFLY_APP_EXITED');

    state.exited = false;

    fails(() => blocker.isStarted(0), 'ERR_FIREFLY_APP_EXITED');

    check(state.shutdowns === 1 && state.starts.length === 0, 'Observed exit permanently revokes');
  }
  {
    const { state, native, blocker } = fixture();
    let reads = 0;
    Object.defineProperty(native, 'powerSaveBlockerStart', {
      get() {
        reads++;
        state.exited = true;
        return () => {
          throw Error('Native getter exit must prevent invocation');
        };
      }
    });

    fails(() => blocker.start('prevent-display-sleep'), 'ERR_FIREFLY_APP_EXITED');

    check(
      reads === 1 && state.shutdowns === 1,
      'Native method snapshot checked once before invocation'
    );
  }
  for (const method of ['start', 'stop', 'isStarted']) {
    const { state, blocker, api } = fixture();

    state[method === 'start' ? 'onStart' : method === 'stop' ? 'onStop' : 'onQuery'] = () => {
      state.exited = true;
      api.stop();
    };

    fails(
      () => blocker[method](method === 'start' ? 'prevent-app-suspension' : 0),
      'ERR_FIREFLY_APP_EXITED'
    );

    check(!state.ids.size, 'Reentrant late start has no surviving request');
    check(state.shutdowns === (method === 'start' ? 2 : 1), 'Late start receives a second cleanup');
  }
  const { state, blocker, api } = fixture();
  blocker.start('prevent-app-suspension');

  state.onShutdown = () => {
    fails(() => blocker.start('prevent-display-sleep'), 'ERR_FIREFLY_APP_EXITED');

    api.stop();
    throw Error('OS release failed');
  };

  api.stop();
  api.stop();
  check(
    state.shutdowns === 1 && !state.ids.size,
    'Shutdown is revoked first idempotent and best effort'
  );
  for (const method of ['start', 'stop', 'isStarted']) {
    fails(
      () => blocker[method](method === 'start' ? 'prevent-app-suspension' : 0),
      'ERR_FIREFLY_APP_EXITED'
    );
  }
  console.log('PASS reentrant exit late admission shutdown and cleanup failures');
}

console.log('DONE power save blocker policy');
