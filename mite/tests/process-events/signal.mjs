/** Parent sends two real signals only after each owned-child readiness record. */
import { constants } from 'node:os';
const type = process.argv[2];
let count = 0;
let once = 0;
const deadline = setTimeout(() => {
  throw new Error('Signal delivery deadline');
}, 5000);

function ready(phase) {
  console.log(JSON.stringify({ ready: phase, pid: process.pid, type }));
}

process.on(type, function (name, number) {
  if (this !== process || name !== type || number !== constants.signals[type]) {
    throw new Error('Native signal arguments/receiver');
  }
  ++count;

  queueMicrotask(() => {
    if (once !== 1) {
      throw new Error('Signal once ordering');
    }
    if (count === 1) {
      if (typeof collectHostGarbage === 'function') {
        collectHostGarbage();
      }
      ready(2);
    } else {
      process.removeAllListeners(type);
      clearTimeout(deadline);
      if (count !== 2 || process.listenerCount(type) !== 0) {
        throw new Error('Signal cleanup');
      }
      console.log('PASS two native signals/once/microtasks/cleanup');
    }
  });
});

process.once(type, () => ++once);

ready(1);
