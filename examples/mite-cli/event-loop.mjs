/** Check-phase FIFO, per-callback nextTick and explicit cancellation. */
import { setImmediate, clearImmediate } from 'node:timers';

const cancelled = setImmediate(() => console.log('This callback was cancelled'));
clearImmediate(cancelled);

setImmediate(() => {
  console.log('first immediate');

  process.nextTick(() => console.log('nextTick between callbacks'));

  setImmediate(() => console.log('next iteration'));
});

setImmediate(() => console.log('second immediate'));

console.log('synchronous entry');
