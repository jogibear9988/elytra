/** Failed binding must release its signal before the same Server is reused. */
import { createServer, createConnection } from 'node:net';
const occupied = createServer();
const old = new AbortController();
const current = new AbortController();
const server = createServer((peer) => peer.end('recovered'));

occupied.listen({ port: 0, host: '127.0.0.1' }, () => {
  server.once('error', (error) => {
    if (error.code !== 'EADDRINUSE') {
      throw error;
    }

    server.listen({ port: 0, host: '127.0.0.1', signal: current.signal }, () => {
      old.abort('failed bind');
      if (!server.listening) {
        // Node 24 keeps the failed listen's signal until close and lets it close
        // the next listener. Observe that defect without dereferencing null.
        server.once('close', () => occupied.close(() => console.log('failure release', false)));

        return;
      }
      const client = createConnection({ port: server.address().port, host: '127.0.0.1' });
      let text = '';

      client.on('data', (data) => (text += data));

      client.once('end', () => {
        server.once('close', () =>
          occupied.close(() => console.log('failure release', text === 'recovered'))
        );

        current.abort();
      });
    });
  });

  server.listen({ port: occupied.address().port, host: '127.0.0.1', signal: old.signal });
});
