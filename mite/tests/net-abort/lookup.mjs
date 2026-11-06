/** Cancel pending lookup and buffered writes; late resolver results must not revive a socket. */
import { createConnection } from 'node:net';
const controller = new AbortController();
const reason = new Error('lookup cancellation');
let lookupDone;
let attempts = 0;
let errors = 0;
let writes = 0;
let errorMatches = false;
let writeMatches = false;
const socket = createConnection({
  host: 'cancel.invalid',
  port: 9,
  autoSelectFamily: false,
  signal: controller.signal,

  lookup(host, options, callback) {
    lookupDone = callback;
  }
});

socket.on('connectionAttempt', () => attempts++);

socket.on('error', (error) => {
  errors++;
  errorMatches =
    error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === reason;
});

socket.write('queued', (error) => {
  writes++;
  writeMatches = error?.code === 'ERR_SOCKET_CLOSED_BEFORE_CONNECTION';
});

socket.once('close', () => {
  lookupDone(null, '127.0.0.1', 4);

  setImmediate(() => {
    console.log(
      'lookup',
      socket.destroyed,
      socket.connecting,
      attempts,
      errors,
      errorMatches,
      writes,
      writeMatches
    );
  });
});

controller.abort(reason);
