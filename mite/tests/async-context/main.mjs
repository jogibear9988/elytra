import assert from 'node:assert/strict';
import { AsyncLocalStorage, AsyncResource } from 'node:async_hooks';
import { readFile } from 'node:fs';
import { lookup } from 'node:dns';
import { createServer, connect } from 'node:net';
const storage = new AsyncLocalStorage();
assert.equal(storage.getStore(), undefined);
assert.equal(
  storage.run('outer', () => {
    assert.equal(storage.getStore(), 'outer');

    assert.throws(() =>
      storage.run('inner', () => {
        throw new Error('test');
      })
    );

    assert.equal(storage.getStore(), 'outer');
    return storage.exit(() => storage.getStore());
  }),
  undefined
);
assert.equal(storage.getStore(), undefined);
console.log('PASS nested scopes and throws');
let resolve;
const pending = new Promise((r) => {
  resolve = r;
});
const registered = storage.run('registration', () => pending.then(() => storage.getStore()));

storage.run('resolution', () => resolve());

assert.equal(await registered, 'registration');
await storage.run('await', async () => {
  await 1;
  assert.equal(storage.getStore(), 'await');
  await Promise.resolve();
  assert.equal(storage.getStore(), 'await');
  await new Promise((r) => setTimeout(r, 1));
  assert.equal(storage.getStore(), 'await');
});
console.log('PASS registration and direct await');
await Promise.all(
  ['one', 'two'].map((value) =>
    storage.run(value, async () => {
      await new Promise((r) =>
        process.nextTick(() => {
          assert.equal(storage.getStore(), value);
          r();
        })
      );
      await new Promise((r) =>
        queueMicrotask(() => {
          assert.equal(storage.getStore(), value);
          r();
        })
      );
      await new Promise((r) =>
        setImmediate(() => {
          assert.equal(storage.getStore(), value);
          r();
        })
      );
      await new Promise((r) =>
        setTimeout(() => {
          assert.equal(storage.getStore(), value);
          r();
        }, 1)
      );
      assert.equal(storage.getStore(), value);
    })
  )
);
console.log('PASS interleaved ticks microtasks immediates timers');
const resource = storage.run('resource', () => new AsyncResource('test'));
const receiver = {};
assert.equal(
  resource.runInAsyncScope(
    function (x) {
      assert.equal(this, receiver);
      assert.equal(storage.getStore(), 'resource');
      return x;
    },
    receiver,
    17
  ),
  17
);
assert.equal(resource.emitDestroy(), resource);
const bound = storage.run('bound', () => AsyncLocalStorage.bind(() => storage.getStore()));
assert.equal(bound(), 'bound');
const snapshot = storage.run('snapshot', () => AsyncLocalStorage.snapshot());
assert.equal(
  snapshot(() => storage.getStore()),
  'snapshot'
);
console.log('PASS AsyncResource bind snapshot');
const old = storage.run('old', () => AsyncLocalStorage.snapshot());
storage.disable();
assert.equal(storage.getStore(), undefined);
assert.equal(
  old(() => storage.getStore()),
  'old'
);

storage.run('new', () => assert.equal(storage.getStore(), 'new'));

assert.equal(
  old(() => storage.getStore()),
  'old'
);
storage.enterWith('entered');
await Promise.resolve();
assert.equal(storage.getStore(), 'entered');
storage.disable();
console.log('PASS disable re-enable and enterWith');
await storage.run(
  'worker',
  () =>
    new Promise((resolve, reject) => {
      readFile(new URL(import.meta.url), (error, bytes) => {
        if (error) {
          return reject(error);
        }
        try {
          assert.equal(storage.getStore(), 'worker');
          assert.ok(bytes.length);
          resolve();
        } catch (error) {
          reject(error);
        }
      });
    })
);
await storage.run(
  'dns',
  () =>
    new Promise((resolve, reject) => {
      lookup('localhost', (error) => {
        if (error) {
          return reject(error);
        }
        try {
          assert.equal(storage.getStore(), 'dns');
          resolve();
        } catch (error) {
          reject(error);
        }
      });
    })
);
console.log('PASS real filesystem worker and DNS');
await new Promise((resolve, reject) => {
  const server = storage.run('server', () =>
    createServer((socket) => {
      try {
        assert.equal(storage.getStore(), 'server');
      } catch (error) {
        reject(error);
      }
      socket.end('hello');
    })
  );
  server.on('error', reject);

  storage.run('server', () =>
    server.listen(0, '127.0.0.1', () => {
      const socket = storage.run('client', () => connect(server.address().port, '127.0.0.1'));
      socket.on('error', reject);

      socket.on('data', () => {
        try {
          assert.equal(storage.getStore(), 'client');
        } catch (error) {
          reject(error);
        }
      });

      socket.on('close', () => server.close(resolve));
    })
  );
});
console.log('PASS real loopback socket contexts');
console.log('DONE async context');
