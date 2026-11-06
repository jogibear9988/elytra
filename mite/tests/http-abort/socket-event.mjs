/** Aborting inside socket assignment must prevent activation and preserve error order. */
import { get } from 'node:http';
const controller = new AbortController();
const events = [];
const pending = get({ host: '127.0.0.1', port: 9, agent: false, signal: controller.signal });

pending.once('socket', () => {
  events.push('socket');
  controller.abort('socket assignment');
});

pending.on('error', (error) => {
  if (error.code !== 'ABORT_ERR' || error.cause !== 'socket assignment') {
    throw error;
  }
  events.push('error');
});

pending.once('close', () => {
  events.push('close');
  console.log('socket event', events.join(','), pending.destroyed);
});
