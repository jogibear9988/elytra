/** No windows: exact production desktop application objects fork a plain Mite child. */
import { app } from 'firefly';
import { fork } from 'node:child_process';
import { once } from 'node:events';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

app.whenReady().then(async () => {
  const deadline = setTimeout(() => {
    console.error('Firefly fork acceptance timed out');
    app.exit(1);
  }, 7000);
  try {
    const child = fork(new URL('./firefly-child.mjs', import.meta.url), ['fork-boundary'], {
      execArgv: [],
      silent: true,
      env: { ...process.env, QT_QPA_PLATFORM: 'elytra-no-gui' }
    });
    let out = '';
    let err = '';

    child.stdout.on('data', (value) => (out += value));

    child.stderr.on('data', (value) => (err += value));

    const closing = once(child, 'close');
    const [ready] = await once(child, 'message');
    check(ready.ready, 'Actual same-executable Mite child starts');
    const response = once(child, 'message');
    const value = { text: 'plain Mite child', nested: { count: 7 } };
    child.send(value);
    value.nested.count = 999;
    const [reply] = await response;
    check(
      reply.role === 'plain-mite' &&
        reply.fireflyUnavailable &&
        reply.parentPort === false &&
        reply.value.nested.count === 7,
      'Child boundary and real fork IPC'
    );
    const [code, signal] = await closing;
    check(
      code === 0 && signal === null && !out && !err,
      'Child reaped without Qt/plugin initialization or stdio leakage'
    );
    console.log(
      'PASS production Firefly parent forks plain Mite child without Qt/desktop/utility privileges'
    );
    clearTimeout(deadline);
    app.exit(0);
  } catch (error) {
    clearTimeout(deadline);
    console.error(error);
    app.exit(1);
  }
});
