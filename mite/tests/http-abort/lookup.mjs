/** Late resolver completions cannot reopen an aborted HTTP connection. */
import { get } from 'node:http';
const controller = new AbortController();
let finishLookup;
let errors = 0;
let correct = false;
let responses = 0;
const pending = get({
  hostname: 'cancel.invalid',
  port: 9,
  agent: false,
  signal: controller.signal,
  autoSelectFamily: false,

  lookup(host, options, callback) {
    finishLookup = callback;
    controller.abort('lookup');
  }
});

pending.on('response', () => responses++);

pending.on('error', (error) => {
  errors++;
  correct = error.code === 'ABORT_ERR' && error.cause === 'lookup';
});

pending.once('close', () => {
  finishLookup(null, '127.0.0.1', 4);

  setImmediate(() => console.log('lookup', responses, errors, correct, pending.destroyed));
});
