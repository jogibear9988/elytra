import { createServer, connect } from 'node:net';
import { once } from 'node:events';
const server = createServer((socket) => socket.resume());
server.listen(0, '127.0.0.1');
await once(server, 'listening');
const socket = connect(server.address().port, '127.0.0.1');
await once(socket, 'connect');

socket.setTimeout(10, () => {
  console.log('timeout', socket.timeout, socket.destroyed);
  socket.end();
});

await once(socket, 'close');
console.log('closed');
await new Promise((resolve) => server.close(resolve));
