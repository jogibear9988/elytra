/** Genuine Mite/factory/Host lifecycle with controlled native completions; no OS launches/trash. */
import native from 'shell-test';
import createShell from '../../core/shell/index.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function rejectsSync(callback) {
  let caught;
  try {
    callback();
  } catch (error) {
    caught = error;
  }
  check(typeof caught?.code === 'string', 'Native boundary rejects invalid request');
}

async function failure(promise, code) {
  let caught;
  try {
    await promise;
  } catch (error) {
    caught = error;
  }
  check(caught?.code === code, 'Expected ' + code + ', got ' + caught);
  return caught;
}

const error = (code, message) => Object.assign(Error(message), { code });

const component = createShell(
  native,
  () => false,
  error,
  (name) => {
    throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
  }
);
const shell = component.shell;
const frames = [];

native.setHandler((json) => {
  const value = JSON.parse(json);
  frames.push(value);
  component.dispatch(value);
});

const watchdog = setTimeout(() => {
  throw Error('Shell component probe timed out');
}, 5000);
check(native.created() === 0, 'Import leaves provider uncreated');
for (const id of [0, -1, -0, 0.5, NaN, Infinity, '1', 2147483648]) {
  rejectsSync(() => native.shellRequest(id, 'openPath', '{"path":"a"}'));

  rejectsSync(() => native.shellCancel(id));
}
for (const payload of [
  'null',
  '[]',
  '{}',
  '{"path":null}',
  '{"path":""}',
  '{"path":"a","extra":true}',
  '{"path":"\\u0000"}',
  '{"path":"\\ud800"}',
  ' '.repeat(512 * 1024 + 1)
]) {
  rejectsSync(() => native.shellRequest(1, 'openPath', payload));
}
check(
  native.created() === 0 && native.count() === 0,
  'Invalid inputs never create native provider'
);

let index = native.count();
let operation = shell.openPath('./relative/../test-file');
check(
  native.value(index) === process.cwd() + '/./relative/../test-file',
  'Path captured absolute at owner admission'
);
native.collect();
native.complete(index, 0);
check(frames.length === 0, 'Worker completion cannot enter JS synchronously');
check((await operation) === '', 'openPath success returns empty string');
const delivered = frames.length;
native.complete(index, 0);
await new Promise((resolve) => setTimeout(resolve, 1));
check(frames.length === delivered, 'Duplicate native callback ignored');

index = native.count();
operation = shell.openPath('missing-file');
native.complete(index, 1);
check(
  (await operation) === 'Native failure: Grüße 日本語',
  'openPath failure resolves UTF-8 message'
);
index = native.count();
operation = shell.openExternal('https://example.invalid/test', { activate: false });
check(native.activate(index) === false, 'Activation option reaches actual native boundary');
native.complete(index, 1);
check(
  (await failure(operation, 'ERR_CONTROLLED_SHELL')).message === 'Native failure: Grüße 日本語',
  'External failure retains Unicode'
);
index = native.count();
operation = shell.trashItem('controlled-only-file');
native.complete(index, 0);
check((await operation) === undefined, 'Trash success resolves void');

for (const mode of [2, 3]) {
  index = native.count();
  operation = shell.openExternal('https://example.invalid/data');
  native.complete(index, mode);
  await failure(operation, 'ERR_FIREFLY_SHELL_DATA');
}

const beforeSynchronous = frames.length;
operation = shell.openExternal('https://example.invalid/synchronous');
check(
  frames.length === beforeSynchronous,
  'Synchronous native completion still queues owner delivery'
);
check((await operation) === undefined, 'Immediate provider completion survives admission');

for (const name of ['invalid-reveal-utf8', 'oversize-reveal', 'invalid-reveal-code']) {
  let caught;
  try {
    shell.showItemInFolder(name);
  } catch (error) {
    caught = error;
  }
  check(caught?.code === 'ERR_FIREFLY_SHELL_DATA', 'Corrupt synchronous native reveal result');
}
shell.showItemInFolder('controlled-reveal-only');

// Leave one real public promise pending while independently filling the native quota.
index = native.count();
operation = shell.openExternal('https://example.invalid/shutdown');
const stoppedOperation = failure(operation, 'ERR_FIREFLY_APP_EXITED');
for (let id = 100; id < 163; id++) {
  native.shellRequest(id, 'openPath', '{"path":"quota-only"}');
}

rejectsSync(() => native.shellRequest(163, 'openPath', '{"path":"quota-only"}'));

const quotaEnd = native.count();
for (let id = 100; id < 163; id++) {
  native.shellCancel(id);
}
const frameCount = frames.length;
for (let offset = quotaEnd - 63; offset < quotaEnd; offset++) {
  native.complete(offset, 0);
}
await new Promise((resolve) => setTimeout(resolve, 1));
check(frames.length === frameCount, 'Canceled native quota batch cannot dispatch');

rejectsSync(() => native.shellRequest(100, 'openPath', '{"path":"reused-id"}'));

// Queue a completion, revoke its public promise, then submit the same callback late.
native.complete(index, 0);
const beforeStop = frames.length;
component.stop();
native.collect();
native.complete(index, 0);
await stoppedOperation;
await new Promise((resolve) => setTimeout(resolve, 1));
check(frames.length === beforeStop, 'Shutdown rejects queued and late callbacks');
await failure(shell.openPath('after-exit'), 'ERR_FIREFLY_APP_EXITED');
clearTimeout(watchdog);
console.log(
  'PASS real shell factory/exact Host strict admission, worker delivery, Unicode/errors, quotas, GC and shutdown'
);
