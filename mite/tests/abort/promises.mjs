/** Actual timer consumers use private cancellation, unaffected by synthetic/stopped events. */
import timers, { setTimeout as sleep, setImmediate as immediate } from 'node:timers/promises';
import bare from 'timers/promises';
console.log(
  timers === bare,
  sleep === timers.setTimeout,
  await immediate(42),
  await sleep(1, 'done')
);
for (const operation of [sleep, (_delay, value, options) => immediate(value, options)]) {
  const before = new AbortController();
  before.abort('before');
  try {
    await operation(100, 0, { signal: before.signal });
  } catch (error) {
    console.log(error.name, error.code, error.cause);
  }
  const during = new AbortController();

  during.signal.addEventListener('abort', (event) => event.stopImmediatePropagation());

  const waiting = operation(100, 0, { signal: during.signal });
  during.abort('during');
  try {
    await waiting;
  } catch (error) {
    console.log(error.name, error.code, error.cause);
  }
}
for (const options of [null, 42, [], { signal: {} }, { ref: 1 }]) {
  try {
    await sleep(1, 0, options);
  } catch (error) {
    console.log(error.name, error.code);
  }
}
const completed = new AbortController();
console.log(await immediate('completed', { signal: completed.signal }));
completed.abort('after');
