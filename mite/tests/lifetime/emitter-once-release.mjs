/** Settled once waits must not retain emitters/payloads through a still-live controller. */
import { EventEmitter, once } from 'node:events';

const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const controllers = [];
const refs = [];

function settle(mode) {
  const controller = new AbortController();
  controllers.push(controller);
  const emitter = new EventEmitter();
  const payload = { value: mode };
  refs.push(new WeakRef(emitter), new WeakRef(payload));

  once(emitter, 'work', { signal: controller.signal }).catch(() => {});

  if (mode === 'success') {
    emitter.emit('work', payload);
  } else if (mode === 'error') {
    emitter.emit('error', payload);
  } else {
    controller.abort();
  }
}

for (let index = 0; index < 16; index++) {
  for (const mode of ['success', 'error', 'abort']) {
    settle(mode);
  }
}

setImmediate(() => {
  collect();
  console.log(
    'once released',
    refs.every((ref) => ref.deref() === undefined),
    controllers.length,
    refs.length
  );
});
