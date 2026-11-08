/** Record actual synchronous JSON send errors, then close/reap the test-owned child. */
import { fork } from 'node:child_process';
import { once } from 'node:events';
const child = fork(new URL('./child.mjs', import.meta.url), ['echo'], {
  silent: true,
  execArgv: []
});
child.stdout.resume();
child.stderr.resume();
const closing = once(child, 'close');
const rows = [];
const cycle = {};
cycle.self = cycle;
for (const value of [undefined, 1n, () => {}, Symbol('message'), cycle, { big: 1n }]) {
  try {
    child.send(value);
    rows.push('ACCEPTED');
  } catch (error) {
    rows.push([error.name, error.code ?? null]);
  }
}

child.on('message', (value) => {
  if (value.ready) {
    child.send(null);
  } else if (value.done) {
    return;
  } else if (value.value === null) {
    child.send({ command: 'finish' });
  }
});

await closing;
console.log(JSON.stringify(rows));
