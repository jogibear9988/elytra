/** Default rejection policy promotes Error reasons to uncaught exception handlers. */
import assert from 'node:assert/strict';
import collect from './collect.mjs';

const error = new Error('rejection-promoted');
const order = [];

process.on('uncaughtExceptionMonitor', (actual, origin) => {
  collect?.();
  assert.equal(actual, error);
  assert.equal(origin, 'unhandledRejection');
  order.push('monitor');
});

process.once('uncaughtException', (actual, origin) => {
  collect?.();
  assert.equal(actual, error);
  assert.equal(origin, 'unhandledRejection');
  assert.deepEqual(order, ['monitor']);

  setTimeout(() => {
    console.log(
      'PASS process default rejection promotes original Error and unhandledRejection origin'
    );
  }, 0);
});

setTimeout(() => Promise.reject(error), 0);
