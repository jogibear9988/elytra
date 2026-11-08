/** Actual shortcut facade with a controlled OS peer; no real global registrations or key events. */
import create from '../../core/global-shortcut/index.mjs';

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

const EXITED = 'ERR_FIREFLY_APP_EXITED';
const UNSUPPORTED = 'ERR_FIREFLY_UNSUPPORTED_API';

function fixture(platform = 'darwin') {
  const state = {
    ready: true,
    exited: false,
    starts: [],
    stops: [],
    shutdowns: 0,
    owned: new Set(),
    conflicts: new Set()
  };
  const native = {
    globalShortcutRegister(token, key, modifiers) {
      state.starts.push({ token, key, modifiers });
      state.onStart?.(token, key, modifiers);
      if (state.failure) {
        throw state.failure;
      }
      if (state.conflicts.has(key)) {
        return false;
      }
      state.owned.add(token);
      return Object.hasOwn(state, 'result') ? state.result : true;
    },

    globalShortcutUnregister(token) {
      state.stops.push(token);
      state.onStop?.(token);
      if (state.stopFailure) {
        throw state.stopFailure;
      }
      state.owned.delete(token);
    },

    globalShortcutShutdown() {
      state.shutdowns++;
      state.owned.clear();
      state.onShutdown?.();
    }
  };

  const error = (code, message) => Object.assign(Error(message), { code });

  const api = create(
    native,
    () => state.ready,
    () => state.exited,
    error,
    (name) => {
      throw error(UNSUPPORTED, name);
    },
    platform
  );
  return { state, native, api, shortcuts: api.globalShortcut, error };
}

{
  const { state, shortcuts, api } = fixture();
  state.ready = false;
  check(
    shortcuts.isRegistered('CommandOrControl+A') === false,
    'Own registration query does not require readiness'
  );
  for (const action of [
    () => shortcuts.register('Cmd+A', () => {}),
    () => shortcuts.registerAll([], () => {}),
    () => shortcuts.unregister('Cmd+A'),
    () => shortcuts.unregisterAll()
  ]) {
    fails(action, 'ERR_FIREFLY_NOT_READY');
  }
  state.ready = true;
  let calls = 0;
  check(
    shortcuts.register('CommandOrControl+Shift+a', function () {
      check(
        this === undefined && arguments.length === 0,
        'Native callback carries no receiver/arguments'
      );
      calls++;
    }),
    'Admission succeeds'
  );
  check(
    state.starts[0].key === 'A' && state.starts[0].modifiers === 9,
    'Mac aliases normalize to native chord'
  );
  check(
    shortcuts.isRegistered('shift+cmd+A') &&
      !shortcuts.register('Shift+Meta+A', () => {
        throw Error('Duplicate replaced owner');
      }),
    'Equivalent aliases preserve original callback'
  );
  api.dispatch({ token: 1 });
  check(calls === 1, 'Accepted callback dispatch');
  check(
    shortcuts.unregister('Super+Shift+A') === undefined && !shortcuts.isRegistered('Cmd+Shift+A'),
    'Alias unregistration returns void'
  );
  api.dispatch({ token: 1 });
  check(calls === 1, 'Stale event ignored');
  shortcuts.unregister('Cmd+Q');
  check(
    shortcuts.register('Option+Return', () => {}),
    'Named key alias accepted'
  );
  check(
    state.starts[1].key === 'Enter' && state.starts[1].modifiers === 4,
    'Native key names normalized'
  );
  api.stop();
  console.log(
    'PASS readiness own queries canonical aliases callback shape duplicate and stale ownership'
  );
}

{
  const { state, shortcuts, api } = fixture('win32');

  shortcuts.register('CmdOrCtrl+F20', () => {});

  check(
    state.starts[0].modifiers === 2 && state.starts[0].key === 'F20',
    'Non-Mac semantic parser uses Control'
  );
  for (const value of [undefined, null, 1, {}, new String('Ctrl+A')]) {
    fails(() => shortcuts.register(value, () => {}), 'ERR_INVALID_ARG_TYPE');
  }
  for (const value of ['', 'Ctrl', 'A+B', 'Ctrl+unknown', 'Ctrl+é', 'Ctrl+A\0']) {
    fails(() => shortcuts.register(value, () => {}), 'ERR_INVALID_ARG_VALUE');
  }

  fails(() => shortcuts.register('x'.repeat(4097), () => {}), 'ERR_OUT_OF_RANGE');

  for (const value of ['AltGr+A', 'MediaPlayPause', 'VolumeUp', 'Num0', 'F21', 'CapsLock']) {
    fails(() => shortcuts.register(value, () => {}), UNSUPPORTED);
  }

  fails(() => shortcuts.register('Ctrl+A', {}), 'ERR_INVALID_ARG_TYPE');

  fails(() => shortcuts.registerAll('Ctrl+A', () => {}), 'ERR_INVALID_ARG_TYPE');

  fails(() => shortcuts.register.call({}, 'Ctrl+A', () => {}), 'ERR_INVALID_ARG_TYPE');

  fails(() => shortcuts.setSuspended(true), UNSUPPORTED);

  fails(() => shortcuts.isSuspended(), UNSUPPORTED);

  for (const key of [';', '=', '-', '[', ']', '\\', "'", '`', ',', '.', '/', '!', '?']) {
    check(
      shortcuts.register('Ctrl+' + key, () => {}),
      'Punctuation mapping ' + key
    );
  }
  check(
    shortcuts.register('Ctrl+Plus', () => {}),
    'Plus alias accepted'
  );
  check(
    state.starts.at(-1).key === '+' && state.starts.at(-1).modifiers === 2,
    'No invented US keyboard shift mapping'
  );
  api.stop();
  console.log(
    'PASS controlled parser platform aliases native punctuation and explicit unsupported key families'
  );
}

{
  const { state, shortcuts, api, error } = fixture();

  shortcuts.register('Ctrl+A', () => {});

  state.conflicts.add('C');
  check(
    shortcuts.registerAll(['Ctrl+B', 'Ctrl+C'], () => {}) === false,
    'OS conflict aborts transaction'
  );
  check(
    shortcuts.isRegistered('Ctrl+A') && !shortcuts.isRegistered('Ctrl+B'),
    'Rollback preserves preexisting owner'
  );
  check(state.owned.size === 1, 'Native newly admitted transaction handles released');
  check(
    shortcuts.registerAll(['Ctrl+D', 'Control+D'], () => {}) === false &&
      !shortcuts.isRegistered('Ctrl+D'),
    'Duplicate within batch rolls back'
  );
  check(
    shortcuts.registerAll(['Ctrl+A'], () => {}) === false && shortcuts.isRegistered('Ctrl+A'),
    'Preexisting duplicate does not unregister owner'
  );
  state.failure = error('ERR_NATIVE_REGISTER', 'Native failed');
  check(
    fails(() => shortcuts.register('Ctrl+E', () => {}), state.failure.code) === state.failure,
    'Native registration error identity'
  );
  delete state.failure;

  state.onStart = (token, key) => {
    if (key === 'G') {
      throw error('ERR_NATIVE_BATCH', 'Second admission failed');
    }
  };

  fails(() => shortcuts.registerAll(['Ctrl+F', 'Ctrl+G'], () => {}), 'ERR_NATIVE_BATCH');

  check(
    !shortcuts.isRegistered('Ctrl+F') && state.owned.size === 1,
    'Thrown batch failure also rolls back'
  );
  state.onStart = null;
  check(
    shortcuts.registerAll(['Ctrl+H', 'Ctrl+I'], () => {}) === true,
    'Successful batch returns boolean'
  );
  state.stopFailure = error('ERR_NATIVE_RELEASE', 'Native removal failed');

  fails(() => shortcuts.unregisterAll(), 'ERR_NATIVE_RELEASE');

  check(
    state.owned.size === 3 && !shortcuts.isRegistered('Ctrl+A'),
    'Failed native removals retain ownership but revoke callbacks'
  );
  const retiredTokens = [...state.owned];
  const beforeRetry = state.stops.length;
  const beforeAdmission = state.starts.length;

  fails(() => shortcuts.register('Ctrl+J', () => {}), 'ERR_NATIVE_RELEASE');

  check(
    state.stops.length === beforeRetry + retiredTokens.length &&
      state.starts.length === beforeAdmission,
    'Permanent release failure retries each token once and prevents admission'
  );
  delete state.stopFailure;
  let reentries = 0;

  state.onStop = (token) => {
    api.dispatch({ token });

    fails(() => shortcuts.register('Ctrl+K', () => {}), 'ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT');

    reentries++;
  };

  check(
    shortcuts.register('Ctrl+J', () => {}),
    'Later admission retries transient failed releases'
  );
  check(reentries === retiredTokens.length, 'Release reentry cannot recursively retry or admit');
  state.onStop = null;
  check(
    state.owned.size === 1 && retiredTokens.every((token) => !state.owned.has(token)),
    'Retry releases old native owners before new admission'
  );
  state.stopFailure = error('ERR_NATIVE_RELEASE', 'Native removal failed');

  fails(() => shortcuts.unregister('Ctrl+J'), 'ERR_NATIVE_RELEASE');

  delete state.stopFailure;
  shortcuts.unregisterAll();
  check(state.owned.size === 0, 'unregisterAll retries already retired tokens');

  shortcuts.register('Ctrl+L', () => {});

  state.stopFailure = error('ERR_NATIVE_RELEASE', 'Native removal failed');

  fails(() => shortcuts.unregister('Ctrl+L'), 'ERR_NATIVE_RELEASE');

  api.stop();
  const afterStop = state.stops.length;

  fails(() => shortcuts.register('Ctrl+M', () => {}), EXITED);

  check(
    state.owned.size === 0 && state.stops.length === afterStop && state.shutdowns === 1,
    'Shutdown clears retired ownership permanently without further release retries'
  );
  console.log(
    'PASS transactional batches conflicts duplicate isolation native failures and exhaustive removal'
  );
}

{
  const { state, shortcuts, api, error } = fixture();
  let calls = 0;

  state.onStart = (token) => {
    api.dispatch({ token });
    api.dispatch({ token });
  };

  shortcuts.register('Ctrl+A', () => {
    calls++;
  });

  check(calls === 0, 'Synchronous native events defer until admission');
  await Promise.resolve();
  check(calls === 2, 'Admitted early events delivered in order');
  state.failure = error('ERR_NATIVE_FAILED', 'Admission failed after event');

  fails(
    () =>
      shortcuts.register('Ctrl+B', () => {
        calls += 100;
      }),
    state.failure.code
  );

  await Promise.resolve();
  check(calls === 2, 'Failed start discards events/callback');
  delete state.failure;

  shortcuts.register('Ctrl+C', () => {
    shortcuts.unregister('Ctrl+C');
    calls++;
  });

  await Promise.resolve();
  check(calls === 3, 'Callback removal revokes remaining early events');
  state.onStart = null;
  api.stop();
  console.log('PASS early event admission rollback and callback reentry revocation');
}

{
  for (const method of [
    'globalShortcutRegister',
    'globalShortcutUnregister',
    'globalShortcutShutdown'
  ]) {
    const peer = fixture();
    delete peer.native[method];

    fails(() => peer.shortcuts.register('Ctrl+A', () => {}), UNSUPPORTED);

    check(
      peer.state.starts.length === 0 && !peer.state.owned.size,
      'Incomplete native ownership cannot admit a hotkey'
    );
    peer.api.stop();
  }
  for (const result of [undefined, null, 0, 1, {}, 'true']) {
    const { state, shortcuts, api } = fixture();
    state.result = result;

    fails(() => shortcuts.register('Ctrl+A', () => {}), 'ERR_FIREFLY_GLOBAL_SHORTCUT_DATA');

    check(
      !state.owned.size && !shortcuts.isRegistered('Ctrl+A'),
      'Malformed native admission cannot own callback'
    );
    api.stop();
  }
  const { state, shortcuts, api } = fixture();

  state.onStart = (token) => {
    for (let index = 0; index < 65; index++) {
      api.dispatch({ token });
    }
  };

  fails(() => shortcuts.register('Ctrl+A', () => {}), 'ERR_FIREFLY_GLOBAL_SHORTCUT_DATA');

  check(!state.owned.size, 'Early event overflow revokes admission');
  api.stop();
  console.log('PASS malformed native admission and bounded early event ownership');
}

{
  const { state, shortcuts, api } = fixture();
  for (let index = 0; index < 64; index++) {
    const modifier = index < 32 ? 'Ctrl' : 'Alt';
    const value = index % 32;

    shortcuts.register(
      modifier + '+' + (value < 26 ? String.fromCharCode(65 + value) : String(value - 26)),
      () => {}
    );
  }

  fails(() => shortcuts.register('Shift+X', () => {}), 'ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT');

  shortcuts.unregister('Ctrl+A');

  shortcuts.register('Shift+X', () => {});

  check(state.starts.at(-1).token === 65, 'Retired tokens are not reused');

  state.onShutdown = () => {
    api.dispatch({ token: 65 });
    throw Error('Shutdown failed');
  };

  api.stop();
  api.stop();
  check(
    state.shutdowns === 1 && !state.owned.size,
    'Shutdown permanently revokes before native cleanup'
  );

  fails(() => shortcuts.isRegistered('Ctrl+A'), EXITED);

  for (const phase of ['exit', 'unregister']) {
    const peer = fixture();

    peer.state.onStart = () => {
      if (phase === 'exit') {
        peer.state.exited = true;
        peer.api.stop();
      } else {
        peer.shortcuts.unregister('Ctrl+A');
      }
    };

    fails(
      () =>
        peer.shortcuts.register('Ctrl+A', () => {
          throw Error('Late callback');
        }),
      phase === 'exit' ? EXITED : 'ERR_FIREFLY_GLOBAL_SHORTCUT_DATA'
    );

    check(peer.state.owned.size === 0, 'Late native admission cleaned after reentrant revocation');
    peer.api.stop();
  }
  console.log(
    'PASS active quota monotonic tokens permanent shutdown and late native admission cleanup'
  );
}

console.log('DONE global shortcut policy');
