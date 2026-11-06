/** Echo over real TCP, then cancel the socket and stop accepting connections. */
import { createConnection, createServer } from 'node:net';

const listenerLifetime = new AbortController();
const connectionLifetime = new AbortController();
const server = createServer((peer) => peer.pipe(peer));

server.on('error', (error) => {
  throw error;
});

server.once('close', () => console.log('Listener closed'));

server.listen({ port: 0, host: '127.0.0.1', signal: listenerLifetime.signal }, () => {
  const socket = createConnection({
    host: '127.0.0.1',
    port: server.address().port,
    signal: AbortSignal.any([connectionLifetime.signal, AbortSignal.timeout(5000)])
  });

  socket.on('error', (error) => {
    if (error.code !== 'ABORT_ERR' || error.cause !== 'Echo complete') {
      throw error;
    }
    console.log('Socket cancelled:', error.cause);
  });

  socket.once('connect', () => socket.write('Hello from TCP'));

  socket.once('data', (data) => {
    console.log(data.toString());
    connectionLifetime.abort('Echo complete');
  });

  socket.once('close', () => listenerLifetime.abort());
});
