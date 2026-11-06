/** Pre-aborted construction must defer the error so callers can install listeners. */
import { Socket } from 'node:net';
const controller = new AbortController();
const reason = { pre: true };
controller.abort(reason);
const socket = new Socket({ signal: controller.signal });
const events = ['returned'];

socket.on('error', (error) => {
  events.push('error');
  if (error.name !== 'AbortError' || error.code !== 'ABORT_ERR' || error.cause !== reason) {
    throw Error('Wrong pre-abort error');
  }
});

socket.once('close', () => {
  events.push('close');
  console.log('pre', events.join(','), socket.destroyed, socket.connecting);
});
