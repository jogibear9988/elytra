/** Errors in rejection listeners are ordinary uncaught exceptions, not recursive events. */
import assert from 'node:assert/strict';
import collect from './collect.mjs';

const mode = process.argv[2];
const marker = new Error(`controlled-${mode}-callback`);
let original;
let notices = 0;
let exceptions = 0;
const order = [];

process.on('uncaughtExceptionMonitor', (error, origin) => {
  collect?.();
  assert.equal(error, marker);
  assert.equal(origin, 'uncaughtException');
  order.push('monitor');
});

process.once('uncaughtException', (error, origin) => {
  collect?.();
  assert.equal(error, marker);
  assert.equal(origin, 'uncaughtException');
  assert.deepEqual(order, ['monitor']);
  ++exceptions;

  setTimeout(() => {
    assert.equal(notices, 1);
    assert.equal(exceptions, 1);
    console.log(`PASS process ${mode} callback throw reaches monitor and uncaught recovery once`);
  }, 10);
});

process.once('unhandledRejection', (reason, promise) => {
  collect?.();
  assert.equal(promise, original);
  assert.equal(reason, 'original-reason');
  ++notices;
  if (mode === 'unhandledRejection') {
    throw marker;
  }

  setTimeout(() => original.catch(() => {}), 0);
});

if (mode === 'rejectionHandled') {
  process.once('rejectionHandled', (promise) => {
    collect?.();
    assert.equal(promise, original);
    throw marker;
  });
}

setTimeout(() => {
  original = Promise.reject('original-reason');
}, 0);
