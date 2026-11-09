/** Promise notifications use the original promise/reason and report late handling once. */
import assert from 'node:assert/strict';
import collect from './collect.mjs';

const reason = { marker: 'object-reason' };
const order = [];
let original;
let second;
let handled = 0;
let notices = 0;

function forbidden() {
  throw new Error('removed rejection listener ran');
}

process.on('unhandledRejection', forbidden);
process.off('unhandledRejection', forbidden);

process.prependOnceListener('unhandledRejection', () => order.push('prepend-once'));

process.once('unhandledRejection', () => order.push('once'));

process.on('unhandledRejection', function (actual, promise) {
  collect?.();
  assert.equal(this, process);
  ++notices;
  if (promise === original) {
    assert.equal(actual, reason);
    assert.deepEqual(order, ['prepend-once', 'once']);
    assert.equal(notices, 1);

    setTimeout(() => {
      collect?.();

      original.catch((value) => {
        collect?.();
        assert.equal(value, reason);
      });
    }, 0);
  } else {
    assert.equal(promise, second);
    assert.equal(actual, undefined);
    assert.equal(notices, 2);
    assert.equal(process.listenerCount('unhandledRejection'), 1);

    second.catch(() => {});
  }
});

process.on('rejectionHandled', function (promise) {
  collect?.();
  assert.equal(this, process);
  assert.equal(promise, handled++ === 0 ? original : second);
  if (handled === 1) {
    second = Promise.reject(undefined);
  } else {
    setTimeout(() => {
      assert.equal(notices, 2);
      assert.equal(handled, 2);
      process.removeAllListeners('unhandledRejection');
      process.removeAllListeners('rejectionHandled');
      console.log(
        'PASS process rejection identity reasons late handling once and listener methods'
      );
      console.log('PASS process same-turn catch suppresses native rejection notifications');
    }, 10);
  }
});

// Neither synchronously nor microtask-attached catches may become unhandled.
Promise.reject(new Error('synchronous-catch')).catch(() => {});

const microtaskCaught = Promise.reject(new Error('microtask-catch'));

queueMicrotask(() => microtaskCaught.catch(() => {}));

setTimeout(() => {
  original = Promise.reject(reason);
}, 0);
