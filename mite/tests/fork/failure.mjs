/** A failed real fork retains a channel facade and reports native admission errors asynchronously. */
import { fork } from 'node:child_process';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const child = fork(new URL('./child.mjs', import.meta.url), [], {
  execArgv: [],
  silent: true,
  cwd: process.cwd() + '/missing-fork-working-directory'
});
check(
  child.connected && typeof child.channel?.ref === 'function',
  'Failed fork has initial IPC facade'
);
let errors = 0;
let early;
let inline = true;

child.on('error', (error) => {
  check(error.code === 'ENOENT', 'Native spawn error');
  errors++;
});

const closing = new Promise((resolve) => child.once('close', resolve));
const accepted = child.send('early', (error) => {
  check(!inline, 'Failed fork write callback asynchronous');
  early = error.code;
});
check(typeof accepted === 'boolean', 'Send preserves boolean admission result');
inline = false;
await closing;
await new Promise((resolve) => setTimeout(resolve, 1));
check(
  errors === 1 && early === 'EPIPE' && !child.connected && child.channel == null,
  'Native failed fork settles IPC ownership'
);
let late;
check(
  child.send('late', (error) => (late = error.code)) === false,
  'Failed closed channel returns false'
);
await new Promise((resolve) => setTimeout(resolve, 1));
check(late === 'ERR_IPC_CHANNEL_CLOSED', 'Failed closed channel callback');
console.log(JSON.stringify({ errors, early, late, connected: child.connected }));
