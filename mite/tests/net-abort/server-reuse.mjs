/** Disposing a listen subscription must prevent the old signal from closing a later listener. */
import { createServer, createConnection } from 'node:net';
const old = new AbortController();
const current = new AbortController();
const server = createServer((peer) => peer.end('new generation'));
let closes = 0;

server.on('close', () => closes++);

server.listen({ port: 0, host: '127.0.0.1', signal: old.signal }, () => {
  server.close(() => {
    server.listen({ port: 0, host: '127.0.0.1', signal: current.signal }, () => {
      old.abort('stale');
      const socket = createConnection({ port: server.address().port, host: '127.0.0.1' });
      let payload = '';

      socket.on('data', (data) => (payload += data));

      socket.once('end', () => {
        server.once('close', () => console.log('reuse', payload === 'new generation', closes));

        current.abort('current');
      });
    });
  });
});
