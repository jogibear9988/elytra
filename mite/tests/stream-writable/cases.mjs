/** Writable admission reflects bytes retained after synchronous or delayed writes. */
import assert from 'node:assert/strict';

export default async function checkWritable(Writable) {
  for (const highWaterMark of [0, 1, 4]) {
    const order = [];
    const stream = new Writable({
      highWaterMark,

      write(bytes, encoding, callback) {
        order.push('write');
        callback();
      }
    });
    let drains = 0;

    stream.on('drain', () => drains++);

    const completed = new Promise((resolve, reject) => {
      assert.equal(
        stream.write(Buffer.alloc(8), (error) => {
          if (error) {
            return reject(error);
          }
          order.push('callback');
          resolve();
        }),
        true
      );
      order.push('returned');
    });
    assert.equal(stream.writableLength, 0);
    await completed;
    assert.deepEqual(order, ['write', 'returned', 'callback']);
    assert.equal(drains, 0);
    stream.destroy();
  }

  const callbacks = [];
  const pending = [];
  let drains = 0;
  const delayed = new Writable({
    highWaterMark: 4,

    write(bytes, encoding, callback) {
      pending.push(callback);
    }
  });

  delayed.on('drain', () => drains++);

  assert.equal(
    delayed.write(Buffer.alloc(3), () => callbacks.push(1)),
    true
  );
  assert.equal(
    delayed.write(Buffer.alloc(1), () => callbacks.push(2)),
    false
  );
  assert.equal(delayed.writableLength, 4);
  pending.shift()();
  assert.equal(delayed.writableLength, 1);
  assert.equal(drains, 0);
  pending.shift()();
  await new Promise((resolve) => setImmediate(resolve));
  assert.deepEqual(callbacks, [1, 2]);
  assert.equal(delayed.writableLength, 0);
  assert.equal(drains, 1);
  delayed.destroy();

  const zeroPending = [];
  const zero = new Writable({
    highWaterMark: 0,

    write(bytes, encoding, callback) {
      zeroPending.push(callback);
    }
  });
  let zeroDrains = 0;

  zero.on('drain', () => zeroDrains++);

  assert.equal(zero.write(Buffer.alloc(1)), false);
  zeroPending.shift()();
  await new Promise((resolve) => setImmediate(resolve));
  assert.equal(zeroDrains, 1);
  zero.destroy();

  const failure = new Error('owned writable failure');
  const failed = new Writable({
    write(bytes, encoding, callback) {
      callback(failure);
    }
  });
  const events = [];
  const closed = new Promise((resolve) => failed.once('close', resolve));

  failed.on('error', (error) => {
    assert.equal(error, failure);
    events.push('error');
  });

  assert.equal(
    failed.write('x', (error) => {
      assert.equal(error, failure);
      events.push('callback');
    }),
    false
  );
  await closed;
  assert.deepEqual(events, ['callback', 'error']);
  console.log('PASS Writable sync/async admission, zero HWM, drain/FIFO and failure ordering');
}
