/** A TCP peer that never completes TLS must be cancellable without application data. */
import { connect } from 'node:tls';
import { readFileSync } from 'node:fs';
const [port, directory] = process.argv.slice(2);
const controller = new AbortController();
let secure = 0;
let correct = false;
const socket = connect({
  host: '127.0.0.1',
  port: Number(port),
  ca: readFileSync(directory + '/ca.pem'),
  signal: controller.signal
});

socket.once('connect', () => setTimeout(() => controller.abort('pending handshake'), 10));

socket.on('secureConnect', () => secure++);

socket.on('data', () => {
  throw Error('Unauthenticated application bytes exposed');
});

socket.on(
  'error',
  (error) =>
    (correct =
      error.name === 'AbortError' &&
      error.code === 'ABORT_ERR' &&
      error.cause === 'pending handshake')
);

socket.once('close', () => console.log('pending', secure, correct));
