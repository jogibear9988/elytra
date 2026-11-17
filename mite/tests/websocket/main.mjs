/** Real raw-peer acceptance of the standalone WebSocket API; no browser or canvas globals. */
import assert from 'node:assert/strict';
import process from 'node:process';
const [mode, url] = process.argv.slice(2);
const timer = setTimeout(() => {
  throw Error('WebSocket fixture deadline');
}, 12000);

const delay = () => new Promise((resolve) => setTimeout(resolve, 5));

async function until(test) {
  for (let n = 0; n < 1600; n++) {
    if (test()) {
      return;
    }
    await delay();
  }
  throw Error('WebSocket condition timed out');
}

let socket;
try {
  assert.equal(typeof WebSocket, 'function');
  for (const [key, value] of [
    ['CONNECTING', 0],
    ['OPEN', 1],
    ['CLOSING', 2],
    ['CLOSED', 3]
  ]) {
    assert.equal(WebSocket[key], value);
    assert.equal(WebSocket.prototype[key], value);
  }
  if (process.versions.mite) {
    assert.throws(
      () =>
        new WebSocket(url, {
          dispatcher: {
            dispatch() {
              throw Error('must not call');
            }
          }
        }),
      (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
    );
  }

  assert.throws(
    () => new WebSocket('ftp://owned.invalid/'),
    (error) => error.name === 'SyntaxError'
  );

  assert.throws(
    () => new WebSocket(url + '#fragment'),
    (error) => error.name === 'SyntaxError'
  );

  assert.throws(
    () => new WebSocket(url, ['duplicate', 'duplicate']),
    (error) => error.name === 'SyntaxError'
  );

  socket = new WebSocket(url, mode === 'happy' ? ['owned'] : []);
  assert.equal(socket instanceof EventTarget, true);
  assert.equal(socket.readyState, WebSocket.CONNECTING);

  assert.throws(
    () => socket.send('before-open'),
    (error) => error.name === 'InvalidStateError'
  );

  const messages = [];
  let errors = 0;
  let opened = 0;
  let attributeOpened = 0;
  let closeEvent;
  let attributeClose;

  socket.onclose = (event) => {
    attributeClose = event;
  };

  const removed = () => {
    throw Error('removed event listener ran');
  };

  socket.addEventListener('open', removed);
  socket.removeEventListener('open', removed);

  socket.onopen = () => attributeOpened++;

  socket.addEventListener('open', () => opened++, { once: true });

  socket.addEventListener('message', (event) => messages.push(event.data));

  socket.onerror = () => errors++;

  const closed = new Promise((resolve) =>
    socket.addEventListener(
      'close',
      (event) => {
        closeEvent = event;
        resolve();
      },
      { once: true }
    )
  );
  if (mode === 'connecting-close') {
    socket.close();
    assert.ok(socket.readyState === WebSocket.CLOSING || socket.readyState === WebSocket.CLOSED);
    let pending;
    try {
      await Promise.race([
        closed,
        new Promise((resolve, reject) => {
          pending = setTimeout(
            () => reject(Error('Connecting close did not cancel transport promptly')),
            2000
          );
        })
      ]);
    } finally {
      clearTimeout(pending);
    }
    if (process.versions.mite) {
      assert.equal(socket.readyState, WebSocket.CLOSED);
    } else {
      assert.ok([WebSocket.CLOSING, WebSocket.CLOSED].includes(socket.readyState));
    } // Node 24.15 retains CLOSING after this close event.
    assert.equal(opened, 0);
    assert.equal(attributeOpened, 0);
    assert.equal(messages.length, 0);
    assert.ok(errors >= 1);
    assert.equal(closeEvent.code, 1006);
    assert.equal(closeEvent.wasClean, false);
  } else if (['happy', 'blob-close', 'tls-trusted', 'send-bound'].includes(mode)) {
    await until(() => opened === 1 || closeEvent);
    assert.equal(opened, 1);
    assert.equal(attributeOpened, 1);
    assert.equal(socket.readyState, WebSocket.OPEN);
    assert.equal(socket.url, url);
    assert.equal(socket.binaryType, 'blob');
    assert.equal(socket.extensions, '');
    assert.equal(typeof socket.bufferedAmount, 'number');
    assert.ok(socket.bufferedAmount >= 0);

    assert.throws(
      () => socket.close(1006),
      (error) => error.name === 'InvalidAccessError'
    );

    assert.throws(
      () => socket.close(1000, 'x'.repeat(124)),
      (error) => error.name === 'SyntaxError'
    );

    assert.equal(socket.readyState, WebSocket.OPEN);

    let expectedReason = mode === 'blob-close' ? 'blob-done' : 'done';
    if (mode === 'send-bound') {
      assert.ok(process.versions.mite, 'Private queue bound is an explicit Mite policy');
      let getterCalls = 0;
      Object.defineProperty(socket, 'bufferedAmount', {
        configurable: true,

        get() {
          getterCalls++;
          throw Error('author bufferedAmount getter');
        }
      });
      let admitted = 0;
      let refusal;
      // Blob reads suspend FIFO draining, so this task deterministically retains
      // every admitted payload even when native writes complete synchronously.
      for (let attempt = 0; attempt < 16; attempt++) {
        const bytes = new Uint8Array(512 * 1024);
        for (let offset = 0; offset < bytes.length; offset++) {
          bytes[offset] = (offset * 7 + admitted) & 255;
        }
        try {
          socket.send(new Blob([bytes]));
          admitted++;
          bytes.fill(211);
        } catch (error) {
          refusal = error;
          break;
        }
      }
      assert.ok(refusal instanceof RangeError);
      assert.equal(refusal.code, 'ERR_MITE_WEBSOCKET_LIMIT');
      assert.equal(admitted, 8, 'Eight immutable Blobs fill the private 4 MiB queue');
      assert.equal(getterCalls, 0);
      expectedReason = 'bound-' + admitted;
      socket.close(1000, expectedReason);
    } else if (mode === 'happy') {
      assert.equal(socket.protocol, 'owned');
      socket.send('hello');
      const storage = new Uint8Array([99, 0, 255, 17, 88]);
      socket.send(new Uint8Array(storage.buffer, 1, 3));
      storage.fill(66);
      socket.send(new Blob(['blob']));
      socket.send('after');
      await until(() => messages.length === 3 || closeEvent);
      assert.equal(messages[0], 'early');
      assert.equal(messages[1], 'ab');
      assert.equal(messages[2] instanceof Blob, true);
      assert.deepEqual(Array.from(new Uint8Array(await messages[2].arrayBuffer())), [0, 255, 7]);
      socket.binaryType = 'arraybuffer';
      assert.equal(socket.binaryType, 'arraybuffer');
      socket.send('array');
      await until(() => messages.length === 4 || closeEvent);
      assert.equal(messages[3] instanceof ArrayBuffer, true);
      assert.deepEqual(Array.from(new Uint8Array(messages[3])), [9, 8]);
      if (process.versions.mite) {
        assert.throws(
          () => socket.send(new Uint8Array(1024 * 1024 + 1)),
          (error) => error.code === 'ERR_MITE_WEBSOCKET_LIMIT'
        );
      }
      socket.close(1000, 'done');
    } else if (mode === 'blob-close') {
      socket.send(new Blob(['pending']));
      socket.send('tail');
      socket.close(1000, 'blob-done');
    } else {
      socket.send('secure');
      await until(() => messages.length === 1 || closeEvent);
      assert.equal(messages[0], 'secure-ok');
      socket.close(1000, 'done');
    }
    await closed;
    assert.equal(closeEvent.code, 1000);
    assert.equal(closeEvent.reason, expectedReason);
    assert.equal(closeEvent.wasClean, true);
    assert.equal(errors, 0);
    assert.equal(socket.readyState, WebSocket.CLOSED);
  } else {
    await closed;
    assert.equal(socket.readyState, WebSocket.CLOSED);
    assert.equal(messages.length, 0);
    if (mode === 'abrupt') {
      assert.equal(opened, 1);
      assert.equal(closeEvent.code, 1006);
      assert.equal(closeEvent.wasClean, false);
    } else {
      assert.ok(errors >= 1);
      if (['bad-accept', 'tls-reject'].includes(mode)) {
        assert.equal(opened, 0);
      }
    }
  }
  assert.equal(attributeClose, closeEvent);
  assert.equal(closeEvent instanceof Event, true);
  console.log('PASS standalone WebSocket ' + mode);
} finally {
  clearTimeout(timer);
  if (socket && socket.readyState === WebSocket.OPEN) {
    socket.close();
  }
}
