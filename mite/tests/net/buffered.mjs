import { createServer, connect } from 'node:net';
import { once } from 'node:events';
import assert from 'node:assert/strict';
const server = createServer({ allowHalfOpen: true }, (socket) => {
  // No data listener until later: starting native reads must retain stream data.
  setTimeout(async () => {
    let text = '';
    // This fixture tests delayed native buffering, not the default iterator's
    // version-dependent destruction of a half-open Duplex on normal EOF.
    for await (const bytes of socket.iterator({ destroyOnReturn: false })) {
      text += bytes;
    }
    assert.equal(socket.bytesRead, 9);
    assert.equal(text, 'buffer me');
    assert.equal(socket.destroyed, false);
    socket.end(text.toUpperCase());
  }, 10);
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
const socket = connect(server.address().port, '127.0.0.1');
socket.end('buffer me');
let result = '';
for await (const bytes of socket) {
  result += bytes;
}
assert.equal(result, 'BUFFER ME');
console.log(result);
await new Promise((resolve) => server.close(resolve));
