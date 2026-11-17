/** Default iterator cleanup and genuine half-open native TCP ownership. */
import assert from 'node:assert/strict';
import { Readable } from 'node:stream';
import { once } from 'node:events';
import { connect, createServer } from 'node:net';

const ordinary = Readable.from([1, 2, 3]);
const values = [];
for await (const value of ordinary) {
  values.push(value);
}
assert.deepEqual(values, [1, 2, 3]);
assert.equal(ordinary.readableEnded, true);
assert.equal(ordinary.destroyed, true);
assert.equal(ordinary.errored, null);
console.log('PASS ordinary Readable EOF cleanup');

for (const mode of ['break', 'throw']) {
  const stream = Readable.from([1, 2]);
  const sentinel = new Error('iterator consumer failure');
  let emitted;

  stream.on('error', (error) => {
    emitted = error;
  });

  const closed = new Promise((resolve) => stream.once('close', resolve));

  async function consume() {
    for await (const value of stream) {
      assert.equal(value, 1);
      if (mode === 'throw') {
        throw sentinel;
      }
      break;
    }
  }

  if (mode === 'throw') {
    await assert.rejects(consume(), (error) => error === sentinel);
  } else {
    await consume();
  }
  await closed;
  assert.equal(stream.destroyed, true);
  assert.equal(emitted?.code, 'ABORT_ERR');
  assert.equal(stream.errored, emitted);
  console.log(`PASS iterator ${mode} preserves cleanup/error identity`);
}

const retained = Readable.from([1, 2, 3]);
for await (const value of retained.iterator({ destroyOnReturn: false })) {
  assert.equal(value, 1);
  break;
}
assert.equal(retained.destroyed, false);
const remainder = [];
for await (const value of retained) {
  remainder.push(value);
}
assert.deepEqual(remainder, [2, 3]);
assert.equal(retained.destroyed, true);
console.log('PASS explicit iterator retention and later cleanup');

// Node 24.15 destroyed half-open Duplexes after ordinary iterator EOF.
// Require the modern behavior in Mite; the exact old oracle remains explicit.
const legacyNode = process.versions.node === '24.15.0';
let serverDone;
let serverFailed;
const handled = new Promise((resolve, reject) => {
  serverDone = resolve;
  serverFailed = reject;
});
const server = createServer({ allowHalfOpen: true }, (socket) => {
  socket.on('error', serverFailed);

  setTimeout(async () => {
    try {
      let request = '';
      for await (const bytes of socket) {
        request += bytes;
      }
      assert.equal(socket.bytesRead, 9);
      assert.equal(request, 'buffer me');
      assert.equal(socket.readableEnded, true);
      assert.equal(socket.destroyed, legacyNode);
      assert.equal(socket.writableEnded, false);
      if (!legacyNode) {
        assert.equal(socket.writable, true);
        socket.end(request.toUpperCase());
      }
      serverDone();
    } catch (error) {
      serverFailed(error);
    }
  }, 10);
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
const client = connect(server.address().port, '127.0.0.1');
client.end('buffer me');
let response = '';
for await (const bytes of client) {
  response += bytes;
}
await handled;
assert.equal(response, legacyNode ? '' : 'BUFFER ME');
assert.equal(client.destroyed, true);
await new Promise((resolve) => server.close(resolve));
console.log('PASS default iterator native half-open EOF ownership');
