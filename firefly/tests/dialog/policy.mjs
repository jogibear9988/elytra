/** Controlled real factory policy, never an OS modal dialog or permission request. */
import create from '../../core/dialog/index.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let caught;
  try {
    action();
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected ' + code + ', got ' + caught?.code);
  return caught;
}

async function rejects(promise, code) {
  let caught;
  try {
    await promise;
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected rejection ' + code + ', got ' + caught?.code);
}

const EXITED = 'ERR_FIREFLY_APP_EXITED';
const DATA = 'ERR_FIREFLY_DIALOG_DATA';
const UNSUPPORTED = 'ERR_FIREFLY_UNSUPPORTED_API';

function fixture() {
  const error = (code, message) => Object.assign(Error(message), { code });

  const state = { ready: true, exited: false, starts: [], cancels: [], shutdowns: 0, syncs: [] };
  const parent = { id: 17, live: true };
  const native = {
    dialogStart(id, kind, parentId, json) {
      state.starts.push({ id, kind, parentId, options: JSON.parse(json) });
      state.onStart?.(id);
      if (state.failure) {
        throw state.failure;
      }
    },

    dialogCancel(id) {
      state.cancels.push(id);
      state.onCancel?.(id);
    },

    dialogShutdown() {
      state.shutdowns++;
      state.onShutdown?.();
    },

    dialogMessageSync(parentId, json) {
      state.syncs.push({ parentId, options: JSON.parse(json) });
      state.onSync?.();
      return state.syncResult ?? '{"response":0,"checkboxChecked":false}';
    }
  };
  const api = create(
    native,
    () => state.ready,
    () => state.exited,
    (value) => {
      if (value !== parent) {
        return undefined;
      }
      if (!parent.live) {
        throw error('ERR_PARENT_DESTROYED', 'Destroyed parent');
      }
      return parent.id;
    },
    error,
    (name) => {
      throw error(UNSUPPORTED, name);
    }
  );
  return { api, dialog: api.dialog, state, native, parent, error };
}

function complete(f, value) {
  f.api.dispatch({ id: f.state.starts.at(-1).id, value });
}

{
  const f = fixture();
  f.state.ready = false;

  fails(() => f.dialog.showMessageBox({}), 'ERR_FIREFLY_NOT_READY');

  fails(() => f.dialog.showMessageBoxSync({}), 'ERR_FIREFLY_NOT_READY');

  f.dialog.showErrorBox('Startup', 'Actual native sync boundary');
  check(f.state.syncs[0].options.type === 'error', 'Error box bypasses readiness only');
  f.state.ready = true;
  const p = f.dialog.showMessageBox(f.parent, { buttons: ['Yes', 'No'], defaultId: 0 });
  const request = f.state.starts.at(-1);
  check(
    request.parentId === 17 && request.options.cancelId === 1,
    'Parent and cancellation default'
  );
  complete(f, { response: 1, checkboxChecked: false });
  check((await p).response === 1, 'Message result');
  check(f.dialog.showMessageBoxSync({}) === 0, 'Sync returns response index');

  fails(() => f.dialog.showMessageBox.call({}, {}), 'ERR_INVALID_ARG_TYPE');

  fails(() => f.dialog.showMessageBox({}, {}), 'ERR_INVALID_ARG_TYPE');

  f.parent.live = false;

  fails(() => f.dialog.showMessageBox(f.parent), 'ERR_PARENT_DESTROYED');

  f.api.stop();
  console.log('PASS readiness overloads strict parents sync response and startup error box');
}

{
  const f = fixture();
  const buttons = ['&Accept', 'Cancel'];
  const p = f.dialog.showMessageBox({
    buttons,
    normalizeAccessKeys: true,
    checkboxLabel: 'Remember',
    checkboxChecked: true
  });
  buttons[0] = 'Mutated';
  check(f.state.starts[0].options.buttons[0] === '&Accept', 'Owned button snapshot');
  complete(f, { response: 0, checkboxChecked: true });
  check((await p).checkboxChecked, 'Checkbox result');
  for (const options of [{ signal: {} }, { icon: {} }, { unknown: false }]) {
    fails(() => f.dialog.showMessageBox(options), UNSUPPORTED);
  }
  for (const options of [
    { message: '\0' },
    { title: '\ud800' },
    { checkboxChecked: true },
    { defaultId: 5 }
  ]) {
    fails(
      () => f.dialog.showMessageBox(options),
      options.defaultId ? 'ERR_OUT_OF_RANGE' : 'ERR_INVALID_ARG_VALUE'
    );
  }

  fails(() => f.dialog.showMessageBox({ title: 'x'.repeat(32769) }), 'ERR_OUT_OF_RANGE');

  fails(() => f.dialog.showMessageBox({ noLink: 1 }), 'ERR_INVALID_ARG_TYPE');

  fails(() => f.dialog.showMessageBox({ buttons: ['x'.repeat(1025)] }), 'ERR_OUT_OF_RANGE');

  fails(() => f.dialog.showMessageBox({ buttons: Array(33).fill('OK') }), 'ERR_INVALID_ARG_TYPE');

  fails(
    () => f.dialog.showOpenDialog({ properties: Array(17).fill('openFile') }),
    'ERR_INVALID_ARG_TYPE'
  );

  fails(
    () => f.dialog.showOpenDialog({ filters: [{ name: 'Long', extensions: ['x'.repeat(257)] }] }),
    'ERR_OUT_OF_RANGE'
  );

  const beforeInvalidFilters = f.state.starts.length;
  for (const extension of ['', '.txt', 'tar.gz', 't*xt', 'folder/txt', 'folder\\txt']) {
    fails(
      () => f.dialog.showOpenDialog({ filters: [{ name: 'Invalid', extensions: [extension] }] }),
      'ERR_INVALID_ARG_VALUE'
    );
  }
  for (const properties of [[], ['showHiddenFiles']]) {
    fails(() => f.dialog.showOpenDialog({ properties }), 'ERR_INVALID_ARG_VALUE');
  }
  check(
    f.state.starts.length === beforeInvalidFilters,
    'Invalid filters and missing selectors never reach native admission'
  );

  fails(() => f.dialog.showOpenDialogSync(), UNSUPPORTED);

  fails(() => f.dialog.showSaveDialogSync(), UNSUPPORTED);

  f.api.stop();
  console.log(
    'PASS immutable option snapshots budgets strict values and explicit unsupported APIs'
  );
}

{
  const f = fixture();
  const extensions = ['txt', '*'];
  const p = f.dialog.showOpenDialog({
    filters: [{ name: 'Text', extensions }],
    properties: ['openFile', 'multiSelections']
  });
  extensions.push('changed');
  check(f.state.starts[0].options.filters[0].extensions.length === 2, 'Owned filters');
  complete(f, { canceled: false, filePaths: ['/tmp/a', '/tmp/b'] });
  check((await p).filePaths.length === 2, 'Multiple selection');
  const save = f.dialog.showSaveDialog();
  check(
    f.state.starts.at(-1).options.title === 'Save' && f.state.starts.at(-1).options.showsTagField,
    'Save defaults'
  );
  complete(f, { canceled: true, filePath: '' });
  check((await save).canceled, 'Canceled save');
  const open = f.dialog.showOpenDialog();
  check(f.state.starts.at(-1).options.properties[0] === 'openFile', 'Open default');
  complete(f, { canceled: true, filePaths: [] });
  await open;

  fails(() => f.dialog.showOpenDialog({ properties: ['promptToCreate'] }), UNSUPPORTED);

  fails(
    () => f.dialog.showSaveDialog({ filters: [{ name: 'Empty', extensions: [] }] }),
    'ERR_INVALID_ARG_TYPE'
  );

  f.api.stop();
  console.log(
    'PASS file dialogs filters default properties owned selection and cancellation results'
  );
}

{
  const f = fixture();

  f.state.onStart = (id) => f.api.dispatch({ id, value: { response: 0, checkboxChecked: false } });

  const p = f.dialog.showMessageBox({});
  let settled = false;

  p.then(() => {
    settled = true;
  });

  check(!settled, 'Synchronous native completion defers settlement');
  await p;
  f.state.failure = f.error('ERR_NATIVE_START', 'Rejected admission');
  await rejects(f.dialog.showMessageBox({}), 'ERR_NATIVE_START');
  delete f.state.failure;

  f.state.onStart = (id) => {
    f.api.dispatch({ id, value: { response: 0, checkboxChecked: false } });
    f.api.dispatch({ id, value: { response: 0, checkboxChecked: false } });
  };

  await rejects(f.dialog.showMessageBox({}), DATA);
  f.state.onStart = undefined;
  const q = f.dialog.showMessageBox({});
  f.api.dispatch({
    id: f.state.starts.at(-1).id,
    error: { code: 'ERR_NATIVE_DIALOG', message: 'Native failure' }
  });
  await rejects(q, 'ERR_NATIVE_DIALOG');
  f.api.stop();
  console.log(
    'PASS early completion deferred admission rollback duplicate and native failure ownership'
  );
}

{
  const f = fixture();
  for (const value of [
    { response: 99, checkboxChecked: false },
    { response: 0, checkboxChecked: 1 },
    null
  ]) {
    const p = f.dialog.showMessageBox({});
    complete(f, value);
    await rejects(p, DATA);
  }
  for (const value of [
    { canceled: true, filePaths: ['/tmp/a'] },
    { canceled: false, filePaths: [] },
    { canceled: false, filePaths: ['/a', '/b'] }
  ]) {
    const p = f.dialog.showOpenDialog({});
    complete(f, value);
    await rejects(p, DATA);
  }
  for (const filePaths of [['relative.txt'], [''], ['C:relative.txt'], ['\\\\server']]) {
    const malformed = f.dialog.showOpenDialog({});
    complete(f, { canceled: false, filePaths });
    await rejects(malformed, DATA);
  }
  const oversized = f.dialog.showOpenDialog({ properties: ['openFile', 'multiSelections'] });
  complete(f, { canceled: false, filePaths: Array(256).fill('/' + 'x'.repeat(1024)) });
  await rejects(oversized, DATA);
  for (const filePath of ['relative.txt', '']) {
    const malformed = f.dialog.showSaveDialog({});
    complete(f, { canceled: false, filePath });
    await rejects(malformed, DATA);
  }
  for (const filePath of [
    '/absolute/../preserved',
    'C:\\folder\\file.txt',
    '\\\\server\\share\\file.txt'
  ]) {
    const valid = f.dialog.showSaveDialog({});
    complete(f, { canceled: false, filePath });
    check((await valid).filePath === filePath, 'Absolute native path spelling preserved');
  }
  const p = f.dialog.showSaveDialog({});
  f.api.dispatch({
    id: f.state.starts.at(-1).id,
    value: { canceled: true, filePath: '' },
    error: { code: 'bad', message: 'bad' }
  });
  await rejects(p, DATA);
  f.state.syncResult = '{"response":900,"checkboxChecked":false}';

  fails(() => f.dialog.showMessageBoxSync({}), DATA);

  f.api.dispatch({ id: 99999, value: null });
  f.api.stop();
  console.log('PASS malformed inconsistent and stale results reject without fabricated selections');
}

{
  const f = fixture();
  const waiting = Array.from({ length: 16 }, () => f.dialog.showMessageBox({}));
  const checked = waiting.map((p) => rejects(p, EXITED));

  fails(() => f.dialog.showMessageBox({}), 'ERR_FIREFLY_DIALOG_LIMIT');

  f.state.onCancel = () => {
    throw Error('Native cancel failure');
  };

  f.state.onShutdown = () => {
    throw Error('Native stop failure');
  };

  f.api.stop();
  f.api.stop();
  await Promise.all(checked);
  check(
    f.state.cancels.length === 16 && f.state.shutdowns === 1,
    'All ownership revoked despite native cleanup errors'
  );

  fails(() => f.dialog.showMessageBox({}), EXITED);

  const g = fixture();

  g.state.onStart = () => {
    g.state.exited = true;
    g.api.stop();
  };

  await rejects(g.dialog.showMessageBox({}), EXITED);
  check(g.state.cancels.length >= 2, 'Late native admission canceled after exit');
  const h = fixture();

  fails(
    () =>
      h.dialog.showMessageBox(h.parent, {
        get message() {
          h.parent.live = false;
          return '';
        }
      }),
    'ERR_PARENT_DESTROYED'
  );

  check(!h.state.starts.length, 'Parent revalidated after option getters');
  h.api.stop();
  console.log(
    'PASS bounded ownership cleanup failures permanent shutdown and reentrant exit parents'
  );
}
console.log('DONE dialog policy');
