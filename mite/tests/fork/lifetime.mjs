/** Genuine channel references, child listener removal and default/silent stdio. */
import { fork } from 'node:child_process';
import { once } from 'node:events';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const entry = new URL('./child.mjs', import.meta.url);
const rows = [];
for (const mode of ['idle', 'unref-channel', 'hold']) {
  const child = fork(entry, [mode], { silent: true, execArgv: [] });
  child.stdout.resume();
  child.stderr.resume();
  const closing = once(child, 'close');
  const [message] = await once(child, 'message');
  check(message.ready === mode, 'Real child role setup');
  check(
    typeof child.channel.ref === 'function' && typeof child.channel.unref === 'function',
    'Real channel reference API'
  );
  child.channel.unref();
  child.channel.ref();
  if (mode === 'hold') {
    child.send('release');
  }
  const [code, signal] = await closing;
  check(code === 0 && signal === null, 'Unreferenced idle channel permits natural child exit');
  rows.push(mode);
}
const delayed = fork(entry, ['delayed'], { silent: true, execArgv: [] });
delayed.stdout.resume();
delayed.stderr.resume();
const delayedClose = once(delayed, 'close');
const delayedReply = once(delayed, 'message');
delayed.send('buffered-before-child-handler');
check(
  (await delayedReply)[0].value === 'buffered-before-child-handler',
  'Child buffers frames before message listener registration'
);
await delayedClose;
rows.push('delayed-child-listener');
const lateParent = fork(entry, ['echo'], { silent: true, execArgv: [] });
lateParent.stdout.resume();
lateParent.stderr.resume();
const lateClose = once(lateParent, 'close');
await new Promise((resolve) => setTimeout(resolve, 50));
const [lateReady] = await once(lateParent, 'message');
check(lateReady.ready === 'echo', 'Parent buffers frames before message listener registration');
lateParent.send({ command: 'finish' });
await lateClose;
rows.push('delayed-parent-listener');
const args = ['spaces in arg', '文🪲', '"quoted"'];
for (const silent of [true, false]) {
  const child = fork(entry, ['stdio', ...args], {
    silent,
    execArgv: [],
    cwd: process.cwd(),
    env: { ...process.env, FORK_TOKEN: 'é日本語' }
  });
  const closing = once(child, 'close');
  let out = '';
  let err = '';
  if (silent) {
    child.stdout.on('data', (value) => (out += value));

    child.stderr.on('data', (value) => (err += value));

    check(child.stdin !== null, 'Silent stdin pipe');
  } else {
    check(
      child.stdin === null && child.stdout === null && child.stderr === null,
      'Default fork stdio inherits'
    );
  }
  const [message] = await once(child, 'message');
  check(
    message.ready === 'stdio' &&
      message.connected &&
      message.send === 'function' &&
      message.channel,
    'Child gets private channel without stdio exposure'
  );
  check(
    message.cwd === process.cwd() &&
      message.token === 'é日本語' &&
      JSON.stringify(message.args) === JSON.stringify(args),
    'Fork captures cwd/env/argv'
  );
  await closing;
  if (silent) {
    check(out === 'FORK_STDOUT\n' && err === 'FORK_STDERR\n', 'Silent captures independent pipes');
  }
  rows.push(silent ? 'silent' : 'inherited');
}
console.log(JSON.stringify(rows));
