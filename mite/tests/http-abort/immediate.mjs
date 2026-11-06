/** Abort a buffered request before its deferred socket association. */
import { request } from 'node:http';
const controller = new AbortController();
const pending = request({ host: '127.0.0.1', port: 9, agent: false, signal: controller.signal });
let sockets = 0;
let responses = 0;
let errors = 0;
let correct = false;

pending.on('socket', () => sockets++);

pending.on('response', () => responses++);

pending.on('error', (error) => {
  errors++;
  correct = error.code === 'ABORT_ERR' && error.cause === 'immediate';
});

pending.once('close', () =>
  console.log('immediate', sockets, responses, errors, correct, pending.destroyed)
);

pending.write('buffered');
pending.end();
controller.abort('immediate');
