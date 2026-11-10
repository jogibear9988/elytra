/** Parent exits independently; the short-lived owned child writes its survival acknowledgement. */
import { fork } from 'node:child_process';
const child = fork(
  new URL('./child.mjs', import.meta.url),
  ['independent', process.argv[2], process.argv[3]],
  {
    execArgv: [],
    stdio: 'ignore',
    // libuv's Windows non-detached children belong to a kill-on-parent-exit job.
    // unref changes loop liveness; actual detachment is required for survival there.
    ...(process.platform === 'win32' ? { detached: true } : {})
  }
);

child.on('error', (error) => {
  throw error;
});

child.unref();
child.channel.unref();
console.log('PARENT_DONE');
