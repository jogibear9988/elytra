/** Controlled shell peer only; no desktop application, file-manager or trash requests. */
import createShell from '../../core/shell/index.mjs';

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

async function rejects(promise, code) {
  let failure;
  try {
    await promise;
  } catch (caught) {
    failure = caught;
  }
  check(
    failure instanceof Error && failure.code === code,
    'Expected rejection ' + code + ', got ' + failure?.code
  );
  return failure;
}

function fixture() {
  const state = { exited: false, requests: [], shows: [], cancels: [], shutdowns: 0 };
  const native = {
    shellRequest(id, kind, json) {
      const record = { id, kind, options: JSON.parse(json) };
      state.requests.push(record);
      state.onRequest?.(record);
      if (state.failure) {
        throw state.failure;
      }
    },

    shellShowItemInFolder(path) {
      state.shows.push(path);
      state.onShow?.();
    },

    shellCancel(id) {
      state.cancels.push(id);
      state.onCancel?.(id);
    },

    shellShutdown() {
      state.shutdowns++;
      state.onShutdown?.();
    }
  };

  const error = (code, message) => Object.assign(Error(message), { code });

  const api = createShell(
    native,
    () => state.exited,
    error,
    (name) => {
      throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
    }
  );
  return { state, native, api, shell: api.shell, error };
}

{
  const { state, api, shell } = fixture();
  check(shell.showItemInFolder('/tmp/雪😀') === undefined, 'Show returns void');
  const opened = shell.openPath('/tmp/document.txt');
  const external = shell.openExternal('custom:hello😀', { activate: false });
  const trash = shell.trashItem('/tmp/disposable');
  check(
    JSON.stringify(state.requests.map((record) => record.id)) === '[1,2,3]',
    'Positive monotonic IDs'
  );
  check(state.requests[1].options.activate === false, 'Activate snapshot');
  check(state.requests[1].options.url === 'custom:hello😀', 'URL snapshot');
  api.dispatch({ id: 2, ok: true });
  api.dispatch({ id: 1, ok: true });
  api.dispatch({ id: 3, ok: true });
  check(
    (await opened) === '' && (await external) === undefined && (await trash) === undefined,
    'Public success result shapes'
  );
  check(state.shows[0] === '/tmp/雪😀', 'Unicode path forwarded');
  const failedPath = shell.openPath('/missing');
  api.dispatch({ id: 4, ok: false, message: 'No default handler', code: 'ERR_NO_HANDLER' });
  check((await failedPath) === 'No default handler', 'openPath OS failure resolves message');
  for (const method of ['openExternal', 'trashItem']) {
    const promise = shell[method]('example:value');
    api.dispatch({
      id: state.requests.at(-1).id,
      ok: false,
      message: 'Native operation failed',
      code: 'ERR_NATIVE_OPERATION'
    });
    const failure = await rejects(promise, 'ERR_NATIVE_OPERATION');
    check(failure.message === 'Native operation failed', 'Native failure message preserved');
  }
  api.stop();
  console.log('PASS pre-ready native payloads success and operation failure results');
}

{
  const { state, shell, api } = fixture();
  for (const method of ['openPath', 'openExternal', 'trashItem']) {
    for (const value of [undefined, null, 1, {}, new String('path')]) {
      await rejects(shell[method](value), 'ERR_INVALID_ARG_TYPE');
    }
    for (const value of ['', 'a\0b', '\ud800', '\udfff', 'x\ud800z']) {
      await rejects(shell[method](value), 'ERR_INVALID_ARG_VALUE');
    }
    await rejects(
      shell[method]('😀'.repeat(method === 'openExternal' ? 16385 : 8193)),
      'ERR_OUT_OF_RANGE'
    );
    await rejects(shell[method].call({}, 'path'), 'ERR_INVALID_ARG_TYPE');
  }

  fails(() => shell.showItemInFolder(null), 'ERR_INVALID_ARG_TYPE');

  fails(() => shell.showItemInFolder('x\0'), 'ERR_INVALID_ARG_VALUE');

  fails(() => shell.showItemInFolder.call({}, 'x'), 'ERR_INVALID_ARG_TYPE');

  for (const value of [null, 1, [], 'x']) {
    await rejects(shell.openExternal('custom:x', value), 'ERR_INVALID_ARG_TYPE');
  }
  for (const value of [null, 1, 'false']) {
    await rejects(shell.openExternal('custom:x', { activate: value }), 'ERR_INVALID_ARG_TYPE');
  }
  for (const value of [
    { workingDirectory: '/tmp' },
    { logUsage: false },
    { extra: true },
    { [Symbol('unknown')]: true }
  ]) {
    await rejects(shell.openExternal('custom:x', value), 'ERR_FIREFLY_UNSUPPORTED_API');
  }
  for (const method of ['beep', 'writeShortcutLink', 'readShortcutLink']) {
    fails(() => shell[method](), 'ERR_FIREFLY_UNSUPPORTED_API');
  }
  check(!state.requests.length && !state.shows.length, 'Validation never invokes native provider');
  let reads = 0;
  const options = {
    get activate() {
      reads++;
      return false;
    }
  };
  const promise = shell.openExternal('custom:x', options);
  check(reads === 1 && state.requests[0].options.activate === false, 'Getter read once');
  api.dispatch({ id: 1, ok: true });
  await promise;
  api.stop();
  console.log('PASS strict validation Unicode bounds unsupported options and single snapshots');
}

{
  const { state, shell, api, error } = fixture();

  state.onRequest = ({ id }) => {
    api.dispatch({ id, ok: true });
    api.dispatch({ id, ok: false, message: 'Duplicate callback' });
  };

  let settled = false;
  const early = shell.openPath('/early').then((value) => {
    settled = true;
    return value;
  });
  check(!settled, 'Synchronous native completion cannot synchronously settle user callback');
  check((await early) === '', 'First early callback wins after admission');
  state.failure = error('ERR_NATIVE_ADMISSION', 'Admission failed after early callback');
  const failure = await rejects(shell.openExternal('custom:failure'), 'ERR_NATIVE_ADMISSION');
  check(failure === state.failure, 'Failed admission overrides queued success');
  check(
    (await rejects(shell.openPath('/failed-admission'), 'ERR_NATIVE_ADMISSION')) === state.failure,
    'openPath admission failure rejects'
  );
  delete state.failure;
  state.onRequest = null;
  api.dispatch({ id: 2, ok: true });
  const pending = shell.trashItem('/later');
  api.dispatch({ id: state.requests.at(-1).id, ok: true });
  await pending;
  check(state.cancels.length === 2, 'Failed admission retires native delivery');
  api.stop();
  console.log('PASS early completions failed admission rollback duplicate and late packets');
}

{
  for (const bad of [
    {},
    { ok: 1 },
    { ok: false },
    { ok: false, message: '' },
    { ok: true, message: 1 },
    { ok: true, code: '' },
    { ok: true, code: 'ERR_NATIVE_OPERATION' },
    { ok: true, message: 'Native operation failed' },
    { ok: true, message: '' },
    { ok: true, code: 'ERR_NATIVE_OPERATION', message: 'Native operation failed' },
    { ok: false, message: '\ud800' },
    { ok: false, message: 'prefix\udfff' },
    { ok: false, message: 'a\0b' },
    { ok: false, message: 'x'.repeat(65537) }
  ]) {
    const { state, shell, api } = fixture();
    const first = shell.openPath('/first');
    const other = shell.openPath('/other');
    api.dispatch({ id: 1, ...bad });
    await rejects(first, 'ERR_FIREFLY_SHELL_DATA');
    check(state.cancels[0] === 1, 'Malformed completion retires only its request');
    api.dispatch({ id: 999, ok: false });
    api.dispatch({ id: 2, ok: true });
    check((await other) === '', 'Other request survives malformed/unknown packets');
    api.stop();
  }
  const { shell, api, native, state } = fixture();
  for (const code of [
    'ERR_FIREFLY_SHELL_UNAVAILABLE',
    'ERR_FIREFLY_SHELL_LIMIT',
    'ERR_FIREFLY_SHELL_DATA',
    'ERR_FIREFLY_APP_EXITED',
    'ERR_FIREFLY_UNSUPPORTED_API'
  ]) {
    const promise = shell.openPath('/reserved-failure');
    api.dispatch({
      id: state.requests.at(-1).id,
      ok: false,
      code,
      message: 'Reserved infrastructure failure'
    });
    await rejects(promise, code);
  }
  delete native.shellRequest;
  await rejects(shell.openPath('/unsupported'), 'ERR_FIREFLY_UNSUPPORTED_API');
  delete native.shellShowItemInFolder;

  fails(() => shell.showItemInFolder('/unsupported'), 'ERR_FIREFLY_UNSUPPORTED_API');

  api.stop();
  console.log('PASS malformed completion isolation and missing native methods');
}

{
  const { state, shell, api } = fixture();
  const requests = [];
  for (let index = 0; index < 64; index++) {
    requests.push(shell.openPath('/' + index));
  }
  await rejects(shell.openPath('/overflow'), 'ERR_FIREFLY_SHELL_LIMIT');
  api.dispatch({ id: 1, ok: true });
  check((await requests.shift()) === '', 'Completed request releases JS quota');
  const replacement = shell.openPath('/replacement');
  check(state.requests.at(-1).id === 65, 'Quota recovery does not reuse ID');
  const settlements = [...requests, replacement].map((promise) =>
    rejects(promise, 'ERR_FIREFLY_APP_EXITED')
  );

  state.onCancel = () => {
    api.dispatch({ id: 2, ok: true });
    api.stop();
  };

  state.onShutdown = () => {
    throw Error('Native shutdown failed');
  };

  api.stop();
  await Promise.all(settlements);
  api.stop();
  check(
    state.shutdowns === 1 && state.cancels.length === 64,
    'Cleanup idempotent despite reentry/failure'
  );
  api.dispatch({ id: 65, ok: true });
  await rejects(shell.openPath('/after-exit'), 'ERR_FIREFLY_APP_EXITED');

  fails(() => shell.showItemInFolder('/after-exit'), 'ERR_FIREFLY_APP_EXITED');

  console.log('PASS quotas ID monotonicity shutdown revocation and late delivery');
}

{
  for (const location of ['options', 'request', 'show', 'getter']) {
    const { state, shell, native, api } = fixture();

    const exit = () => {
      state.exited = true;
      api.stop();
    };

    if (location === 'options') {
      await rejects(
        shell.openExternal('custom:exit', {
          get activate() {
            exit();
            return true;
          }
        }),
        'ERR_FIREFLY_APP_EXITED'
      );
      check(state.requests.length === 0, 'Option exit prevents admission');
    } else if (location === 'request') {
      state.onRequest = exit;
      await rejects(shell.openPath('/exit'), 'ERR_FIREFLY_APP_EXITED');
    } else if (location === 'show') {
      state.onShow = exit;

      fails(() => shell.showItemInFolder('/exit'), 'ERR_FIREFLY_APP_EXITED');
    } else {
      Object.defineProperty(native, 'shellRequest', {
        get() {
          exit();
          return () => {
            throw Error('Must not invoke');
          };
        }
      });
      await rejects(shell.openPath('/exit'), 'ERR_FIREFLY_APP_EXITED');
    }
    check(state.shutdowns === 1, 'Reentrant exit shuts down once');
  }
  console.log('PASS reentrant exit during option snapshot native admission show and method access');
}

console.log('DONE shell policy');
