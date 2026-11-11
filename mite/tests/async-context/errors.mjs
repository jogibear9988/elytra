import assert from 'node:assert/strict';
import { AsyncLocalStorage } from 'node:async_hooks';
const local = new AsyncLocalStorage();
for (const [kind, schedule] of [
  ['timer', (fn) => setTimeout(fn, 0)],
  ['immediate', (fn) => setImmediate(fn)],
  ['tick', (fn) => process.nextTick(fn)],
  ['microtask', (fn) => queueMicrotask(fn)]
]) {
  await new Promise((resolve) => {
    process.once('uncaughtException', (error) => {
      assert.equal(error.message, kind);
      assert.equal(local.getStore(), kind);
      resolve();
    });

    local.run(kind, () =>
      schedule(() => {
        throw Error(kind);
      })
    );
  });
}
console.log('PASS uncaught exception async contexts');
