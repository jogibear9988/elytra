/** Abort closes the listener but preserves an accepted connection until the peer finishes. */
import { createServer, createConnection } from 'node:net';
const controller = new AbortController();
let closed = false;
let errors = 0;
let stillUsable = false;
const server = createServer((peer) => {
  controller.abort('stop accepting');
  peer.pipe(peer);
});

server.on('error', () => errors++);

server.once('close', () => {
  closed = true;
  console.log('server wait', stillUsable, errors, server.listening, server.address() === null);
});

server.listen({ port: 0, host: '127.0.0.1', signal: controller.signal }, () => {
  const socket = createConnection({ port: server.address().port, host: '127.0.0.1' });

  socket.once('connect', () => socket.write('accepted connection survives'));

  socket.once('data', (data) => {
    stillUsable =
      !closed && !server.listening && data.toString() === 'accepted connection survives';
    socket.end();
  });
});
