/** Record native-policy differences explicitly; no synthetic abort may impersonate cancellation. */
import { EventEmitter, once } from 'node:events';
import { setImmediate } from 'node:timers/promises';

const controller = new AbortController();
const emitter = new EventEmitter();
const result = once(emitter, 'work', { signal: controller.signal }).then(
  (args) => args[0],
  (error) => error.code
);
controller.signal.dispatchEvent(new Event('abort'));
emitter.emit('work', 'payload');
console.log(
  'synthetic',
  await result,
  controller.signal.aborted,
  emitter.listenerCount('work'),
  emitter.listenerCount('error')
);

const reentrant = new AbortController();
const target = new EventEmitter();

target.on('newListener', (name) => {
  if (name === 'work') {
    reentrant.abort('during registration');
  }
});

const waiting = once(target, 'work', { signal: reentrant.signal }).then(
  () => 'unexpected',
  (error) => error.code
);
console.log(
  'reentrant',
  await Promise.race([waiting, setImmediate('pending')]),
  target.listenerCount('work'),
  target.listenerCount('error')
);
target.removeAllListeners();

const partial = new EventEmitter();
const partialController = new AbortController();
const setupError = new Error('error listener registration failed');

partial.on('newListener', (name) => {
  if (name === 'error') {
    throw setupError;
  }
});

const rejection = await once(partial, 'work', { signal: partialController.signal }).then(
  () => 'unexpected',
  (error) => error === setupError
);
console.log('partial', rejection, partial.listenerCount('work'), partial.listenerCount('error'));
partial.removeAllListeners();
partialController.abort();
