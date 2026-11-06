/** Pre-aborted requests expose destroyed state on return and defer error delivery. */
import { request } from 'node:http';
const controller = new AbortController();
controller.abort('before request');
const events = [];
let correct = false;
const pending = request({ host: '127.0.0.1', port: 9, agent: false, signal: controller.signal });
events.push('returned:' + pending.destroyed);

pending.on('socket', () => events.push('socket'));

pending.on('response', () => events.push('response'));

pending.on('abort', () => events.push('abort'));

pending.on('error', (error) => {
  correct =
    error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === 'before request';
  events.push('error');
});

pending.once('close', () =>
  console.log('pre', events.join(','), correct, pending.socket === null, pending.aborted)
);

pending.end();
