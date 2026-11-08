/** Explicit unsupported options must refuse before creating a process. Mite-only policy. */
import { fork } from 'node:child_process';
import { once } from 'node:events';
const entry = new URL('./child.mjs', import.meta.url);
const cases = [
  { serialization: 'advanced' },
  { execArgv: ['--inspect=0'] },
  { execPath: '/not/our/runtime' },
  { stdio: ['pipe', 'pipe', 'pipe', 'ipc', 'pipe'] }
];
for (const options of cases) {
  let caught;
  try {
    fork(entry, [], options);
  } catch (error) {
    caught = error;
  }
  if (caught?.code !== 'ERR_MITE_UNSUPPORTED_API') {
    throw Error('Unsupported fork option accepted: ' + JSON.stringify(options));
  }
}
const child = fork(entry, ['echo'], { silent: true, execArgv: [] });
child.stdout.resume();
child.stderr.resume();
const closing = once(child, 'close');
let handleError;
try {
  child.send('not-transferred', {});
} catch (error) {
  handleError = error;
}
if (handleError?.code !== 'ERR_MITE_UNSUPPORTED_API') {
  throw Error('Handle passing accepted');
}

child.on('message', (value) => {
  if (value.ready) {
    child.send({ command: 'finish' });
  }
});

await closing;
console.log('PASS explicit unsupported fork modes');
