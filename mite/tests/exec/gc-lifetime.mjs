/** Actual shells and capture callbacks remain rooted while the embedded LibJS collector runs. */
import { exec } from 'node:child_process';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

check(typeof collectHostGarbage === 'function', 'Requires actual embedded LibJS collector');
let collections = 0;
let successes = 0;
let timeouts = 0;
const collector = setInterval(() => {
  collectHostGarbage();
  ++collections;
}, 1);

function capture(command, options, timeout) {
  return new Promise((resolve, reject) => {
    // The returned real ChildProcess has no application reference after this frame.
    exec(command, options, (error, stdout, stderr) => {
      try {
        if (timeout) {
          check(error?.killed && error.signal === 'SIGTERM', 'Timeout kills the actual child');
          check(error.cmd === command, 'Timeout error retains original shell command');
          check(stdout === '' && stderr === '', 'Timeout capture has no invented output');
          ++timeouts;
        } else {
          check(!error, 'Successful shell has no callback error');
          check(stdout === 'abc'.repeat(8), 'Complete stdout survives collection');
          check(stderr === 'ERR'.repeat(8), 'Complete stderr survives collection');
          ++successes;
        }
        resolve();
      } catch (failure) {
        reject(failure);
      }
    });
  });
}

try {
  await Promise.all([
    capture(
      'i=0; while [ "$i" -lt 8 ]; do printf abc; printf ERR >&2; sleep 0.01; i=$((i+1)); done',
      {},
      false
    ),
    // POSIX exec replaces the shell, so timeout kills the owned sleep process;
    // it cannot leave an unmonitored descendant holding the output pipes open.
    capture('exec sleep 10', { timeout: 30 }, true)
  ]);
  check(collections > 0, 'Collector ran while real shell work was pending');
  check(successes === 1 && timeouts === 1, 'Capture and timeout each call back once');
  console.log('PASS real shell capture and timeout retain callbacks across native GC');
} finally {
  clearInterval(collector);
}
