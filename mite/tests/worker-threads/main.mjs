/** Differential native worker acceptance; no GUI, network, subprocess worker or device use. */
import assert from 'node:assert/strict';
import process from 'node:process';
import { Worker, isMainThread, parentPort, threadId, workerData } from 'node:worker_threads';
assert.equal(isMainThread, true);
assert.equal(parentPort, null);
assert.equal(threadId, 0);
assert.equal(workerData, null);
const timeout = setTimeout(() => {
  throw new Error('Worker acceptance timed out');
}, 15000);

function result(worker) {
  const messages = [];
  let online = false;

  worker.once('online', () => {
    online = true;
  });

  worker.on('message', (value) => {
    assert.equal(online, true);
    messages.push(value);
  });

  return new Promise((resolve, reject) => {
    worker.once('error', reject);

    worker.once('exit', (code) => resolve({ code, messages }));
  });
}

try {
  globalThis.parentSecret = true;
  delete process.env.MITE_WORKER_OWNED_ENV;
  const worker = new Worker(new URL('./child.mjs', import.meta.url), {
    name: 'Owned clone worker'
  });
  const finished = result(worker);
  const backing = new ArrayBuffer(8);
  const bytes = new Uint8Array(backing, 2, 3);
  bytes.set([0, 255, 17]);
  const value = {
    text: 'native-heap',
    bytes,
    backing,
    missing: undefined,
    big: 123n
  };
  value.self = value;
  value.alias = value.bytes;
  const expectedThread = worker.threadId;
  if (process.versions.mite) {
    assert.throws(() => worker.postMessage(new Uint8Array(new SharedArrayBuffer(8))), {
      name: 'DataCloneError'
    });

    assert.throws(() => worker.postMessage([new Uint8Array(700000), new Uint8Array(700000)]), {
      name: 'DataCloneError'
    });
  }
  worker.postMessage({ op: 'echo', value });
  value.text = 'mutated';
  value.bytes[0] = 12;
  const { code, messages } = await finished;
  assert.equal(code, 0);
  assert.equal(messages.length, 1);
  const reply = messages[0];
  assert.equal(reply.pid, process.pid);
  assert.ok(reply.threadId > 0);
  assert.equal(reply.threadId, expectedThread);
  assert.equal(reply.absent, true);
  assert.equal(reply.chdirCode, 'ERR_WORKER_UNSUPPORTED_OPERATION');
  assert.equal(reply.value.text, 'native-heap');
  assert.deepEqual(Array.from(reply.value.bytes), [0, 255, 17]);
  assert.equal(reply.value.bytes.byteOffset, 2);
  assert.equal(reply.value.bytes.buffer, reply.value.backing);
  assert.equal(reply.value.self, reply.value);
  assert.equal(reply.value.alias, reply.value.bytes);
  assert.equal(reply.value.big, 123n);
  assert.equal(process.env.MITE_WORKER_OWNED_ENV, undefined);
  assert.equal(globalThis.workerSecret, undefined);
  assert.equal(worker.threadId, -1);
  console.log(
    'PASS worker same PID, independent VM/global/environment, real clone snapshots and natural exit'
  );

  const once = new Worker(new URL('./once.cjs', import.meta.url), { name: 'One shot' });
  const one = await result(once);
  assert.equal(one.code, 0);
  assert.equal(one.messages[0].pid, process.pid);
  assert.equal(one.messages[0].done, true);
  console.log('PASS CommonJS parentPort one-shot message precedes exit');

  const loop = new Worker(new URL('./child.mjs', import.meta.url));
  const loopExit = result(loop);
  await new Promise((resolve, reject) => {
    loop.once('error', reject);
    loop.once('message', resolve);
    loop.postMessage({ op: 'loop' });
  });
  const termination = loop.terminate();
  assert.equal(await termination, 1);
  assert.equal((await loopExit).code, 1);
  const bare = new Worker(new URL('./bare-loop.mjs', import.meta.url));
  const bareExit = result(bare);
  await new Promise((resolve, reject) => {
    bare.once('online', resolve);
    bare.once('error', reject);
  });
  assert.equal(await bare.terminate(), 1);
  assert.equal((await bareExit).code, 1);
  console.log('PASS terminate interrupts a real tight loop and joins before exit');

  const exiting = new Worker(new URL('./child.mjs', import.meta.url));
  const exitingResult = result(exiting);
  exiting.postMessage({ op: 'exit' });
  const exit = await exitingResult;
  assert.equal(exit.code, 3);
  assert.deepEqual(exit.messages, ['before-exit']);
  console.log('PASS process.exit remains local to worker');

  const failure = new Worker(new URL('./child.mjs', import.meta.url));
  let caught;
  const failed = new Promise((resolve) => {
    failure.once('error', (error) => {
      caught = error;
    });

    failure.once('exit', resolve);
  });
  failure.postMessage({ op: 'throw' });
  assert.equal(await failed, 1);
  assert.equal(caught.message, 'owned-worker-failure');
  console.log('PASS worker uncaught error and nonzero exit preserve parent lifetime');
} finally {
  clearTimeout(timeout);
  delete globalThis.parentSecret;
}
