/** Real Node/Mite comparison: cancellation must release both emitter listeners. */
import { EventEmitter, once } from 'node:events';

function check(value, label) {
  if (!value) {
    throw new Error(label);
  }
}

function empty(emitter) {
  check(
    emitter.listenerCount('work') === 0 && emitter.listenerCount('error') === 0,
    'listener leak'
  );
}

let validations = 0;
for (const options of [
  null,
  [],
  3,
  false,
  'invalid',
  { signal: null },
  { signal: {} },
  { signal: new EventTarget() }
]) {
  const emitter = new EventEmitter();
  let promise;
  try {
    promise = once(emitter, 'work', options);
  } catch {
    throw new Error('once must reject a Promise rather than throwing synchronously');
  }
  await promise.then(
    () => {
      throw new Error('invalid options accepted');
    },
    (error) => {
      check(
        error.name === 'TypeError' && error.code === 'ERR_INVALID_ARG_TYPE',
        'validation error'
      );
      validations++;
    }
  );
  empty(emitter);
}

for (const reason of [new Error('stop'), { custom: 42 }, undefined, null, 0]) {
  const controller = new AbortController();
  controller.abort(reason);
  const emitter = new EventEmitter();
  await once(emitter, 'work', { signal: controller.signal }).then(
    () => {
      throw new Error('pre-aborted signal accepted');
    },
    (error) =>
      check(
        error.name === 'AbortError' &&
          error.code === 'ABORT_ERR' &&
          error.cause === controller.signal.reason,
        'pre-abort reason identity'
      )
  );
  empty(emitter);
}

const controller = new AbortController();
const emitter = new EventEmitter();

controller.signal.addEventListener('abort', (event) => event.stopImmediatePropagation());

const waiting = once(emitter, 'work', { signal: controller.signal });
check(
  emitter.listenerCount('work') === 1 && emitter.listenerCount('error') === 1,
  'missing wait listeners'
);
const reason = new Error('cancelled');
controller.abort(reason);
await waiting.then(
  () => {
    throw new Error('abort suppressed');
  },
  (error) => {
    check(
      error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === reason,
      'late abort'
    );
  }
);
empty(emitter);
check(!emitter.emit('work'), 'aborted resolver remains installed');

for (const operation of ['success', 'error', 'explicit-error', 'symbol']) {
  const emitter = new EventEmitter();
  const controller = new AbortController();
  const name =
    operation === 'explicit-error' ? 'error' : operation === 'symbol' ? Symbol('work') : 'work';
  const waiting = once(emitter, name, { signal: controller.signal });
  const value = new Error('original');
  if (operation === 'error') {
    emitter.emit('error', value);
  } else {
    emitter.emit(name, value, 42);
  }
  // Completing an event/error releases cancellation immediately, before awaiting.
  controller.abort(new Error('too late'));
  await waiting.then(
    (args) => {
      check(
        operation !== 'error' && args.length === 2 && args[0] === value && args[1] === 42,
        'resolved args'
      );
    },
    (error) => check(operation === 'error' && error === value, 'original error identity')
  );
  check(emitter.eventNames().length === 0, 'completed listeners remain');
}

const shared = new EventEmitter();
const first = new AbortController();
const second = new AbortController();
const a = once(shared, 'work', { signal: first.signal }).catch((error) => error.code);
const b = once(shared, 'work', { signal: second.signal });
first.abort();
check(
  shared.listenerCount('work') === 1 && shared.listenerCount('error') === 1,
  'unrelated wait removed'
);
shared.emit('work', 'remaining');
check((await a) === 'ABORT_ERR' && (await b)[0] === 'remaining', 'independent wait results');
empty(shared);

const throwing = new EventEmitter();
const setupError = new Error('newListener failed');

throwing.on('newListener', () => {
  throw setupError;
});

await once(throwing, 'work', { signal: new AbortController().signal }).then(
  () => {
    throw new Error('registration error ignored');
  },
  (error) => check(error === setupError, 'registration error changed')
);
empty(throwing);
console.log(
  'PASS events.once cancellation',
  validations,
  'validation cases; pre/later abort, cause, stop propagation, settlement, isolation and setup failure'
);
