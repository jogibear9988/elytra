/** Exact production dialog Host and real factory, hidden parent and controlled native completions. */
import native from 'dialog-test';
import createDialogs from '../../core/dialog/index.mjs';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const failure = (code, message) => Object.assign(Error(message), { code });

const unsupported = (name) => {
  throw failure('ERR_FIREFLY_UNSUPPORTED_API', name);
};

const tick = () => new Promise((resolve) => setTimeout(resolve, 10));

const parent = Object.freeze({ id: 7 });
let parentExists = true;
let ready = false;

const resolveParent = (value) => {
  if (value !== parent) {
    return undefined;
  }
  if (!parentExists) {
    throw failure('ERR_FIREFLY_OBJECT_DESTROYED', 'Parent destroyed');
  }
  return 7;
};

const component = createDialogs(
  native,
  () => ready,
  () => false,
  resolveParent,
  failure,
  unsupported
);

native.setHandler((frame) => component.dispatch(JSON.parse(frame)));

const dialog = component.dialog;
const options = {
  message: 'Grüße 日本語',
  buttons: ['First', 'Second'],
  checkboxLabel: 'Remember',
  checkboxChecked: true
};

async function rejectCode(fn, code) {
  let caught;
  try {
    await fn();
  } catch (error) {
    caught = error;
  }
  check(caught?.code === code, `Expected ${code}, got ${caught?.code}: ${caught?.message}`);
  return caught;
}

if (process.argv.includes('--leave-active')) {
  ready = true;

  dialog.showOpenDialog({ properties: ['openFile'] }).catch(() => {});

  check(native.count() === 1, 'Native request outstanding at actual VM shutdown');
  console.log('PASS dialog outstanding controlled native request reaches real VM shutdown');
} else {
  await rejectCode(() => dialog.showOpenDialog({}), 'ERR_FIREFLY_NOT_READY');
  check(native.created() === 0, 'Readiness rejection creates no native provider');
  ready = true;
  for (const args of [
    [-1, 'message', 0, '{}'],
    [1.5, 'message', 0, '{}'],
    [1, 'unknown', 0, '{}'],
    [1, 'message', -1, '{}'],
    [1, 'message', 0, '{'],
    [1, 'message', 0, '[]']
  ]) {
    let caught = false;
    try {
      native.dialogStart(...args);
    } catch {
      caught = true;
    }
    check(caught && native.created() === 0, 'Native invalid admission precedes provider creation');
  }
  await rejectCode(() => dialog.showOpenDialog({ id: 999 }, {}), 'ERR_INVALID_ARG_TYPE');
  check(native.created() === 0, 'Unknown parent creates no native provider');
  native.control(1);
  const admission = await rejectCode(() => dialog.showMessageBox(options), 'ERR_CONTROLLED_DIALOG');
  check(admission.message.includes('Grüße 日本語'), 'Native Unicode admission error preserved');
  native.control(2);
  let earlyDelivered = false;
  await rejectCode(
    () =>
      dialog.showMessageBox(options).then(() => {
        earlyDelivered = true;
      }),
    'ERR_CONTROLLED_DIALOG'
  );
  await tick();
  check(!earlyDelivered, 'Failed admission revokes synchronous early completion');
  let callbacks = 0;

  function retainCompletion(promise) {
    const marker = { message: 'retained actual native promise reaction' };
    return promise.then((value) => {
      native.collect();
      check(
        marker.message === 'retained actual native promise reaction',
        'Collector preserves active result callback'
      );
      callbacks++;
      return value;
    });
  }

  const messagePromise = retainCompletion(dialog.showMessageBox(parent, options));
  const messageIndex = native.count() - 1;
  native.collect();
  native.complete(messageIndex, 0);
  check(callbacks === 0, 'Worker completion cannot synchronously enter VM');
  const result = await messagePromise;
  check(
    result.response === 1 && result.checkboxChecked === true && callbacks === 1,
    'Actual owner dispatch preserves result and GC closure'
  );
  native.complete(messageIndex, 0);
  await tick();
  check(callbacks === 1, 'Duplicate terminal native result ignored');
  for (const [method, key] of [
    ['showOpenDialog', 'filePaths'],
    ['showSaveDialog', 'filePath']
  ]) {
    const promise = dialog[method]({
      defaultPath: '/tmp',
      filters: [{ name: 'Text', extensions: ['txt'] }]
    });
    native.complete(native.count() - 1, 0);
    const value = await promise;
    check(
      !value.canceled &&
        (Array.isArray(value[key]) ? value[key][0] : value[key]).includes('Grüße 日本語'),
      'Native absolute Unicode file selection survives exact Host'
    );
  }
  native.control(3);
  let synchronous = true;
  const inline = dialog.showMessageBox(options).then((value) => {
    check(
      !synchronous && value.response === 1,
      'Synchronous provider completion still asynchronous public Promise'
    );
  });
  synchronous = false;
  await inline;
  for (const choice of [1, 2, 3, 4]) {
    const promise = choice === 3 ? dialog.showSaveDialog({}) : dialog.showMessageBox(options);
    native.complete(native.count() - 1, choice);
    await rejectCode(
      () => promise,
      choice === 1 ? 'ERR_CONTROLLED_DIALOG' : 'ERR_FIREFLY_DIALOG_DATA'
    );
  }
  check(dialog.showMessageBoxSync(options) === 1, 'Exact native synchronous message result');
  const deferredDuringModal = dialog.showMessageBox(options);
  native.control(3);
  check(
    dialog.showMessageBoxSync(options) === 1,
    'Native synchronous result survives nested Qt processing'
  );
  check(
    (await deferredDuringModal).response === 1,
    'Native completion deferred until synchronous modal operation returns'
  );
  native.control(1);
  await rejectCode(() => dialog.showMessageBoxSync(options), 'ERR_CONTROLLED_DIALOG');
  native.control(2);
  await rejectCode(() => dialog.showMessageBoxSync(options), 'ERR_FIREFLY_DIALOG_DATA');
  const first = native.count();
  const pending = Array.from({ length: 16 }, () => dialog.showMessageBox(options));
  await rejectCode(() => dialog.showMessageBox(options), 'ERR_FIREFLY_DIALOG_LIMIT');
  for (let index = first; index < first + 16; index++) {
    native.complete(index, 0);
  }
  check(
    (await Promise.all(pending)).length === 16,
    'Bounded pending requests settle and release slots'
  );
  const beforeCanceled = native.canceled();
  const parentClosed = dialog.showMessageBox(parent, options);
  native.complete(native.count() - 1, 0); // Queued success must revalidate its parent before VM delivery.
  native.destroyParent();
  parentExists = false;
  await rejectCode(() => parentClosed, 'ERR_FIREFLY_OBJECT_DESTROYED');
  check(native.canceled() > beforeCanceled, 'Parent destruction revokes native request');
  const last = dialog.showMessageBox(options);
  const rejected = rejectCode(() => last, 'ERR_FIREFLY_APP_EXITED');
  const lastIndex = native.count() - 1;
  native.destroyReceiver(); // Revoke native lifetime before the managed API retires its promises.
  component.stop();
  native.complete(lastIndex, 0);
  native.collect();
  await rejected;
  await rejectCode(() => native.dialogStart(10000, 'message', 7, '{}'), 'ERR_FIREFLY_APP_EXITED');
  await rejectCode(() => native.dialogMessageSync(7, '{}'), 'ERR_FIREFLY_APP_EXITED');
  await tick();
  console.log(
    'PASS exact dialog Host strict admission, native results, queued worker delivery, callback GC, capacity and parent/shutdown races'
  );
}
