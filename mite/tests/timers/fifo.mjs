/** FIFO batches and microtask/nextTick checkpoints between individual immediates. */
const events = [];

setImmediate(() => {
  events.push('first');

  process.nextTick(() => events.push('tick'));

  queueMicrotask(() => events.push('microtask'));

  setImmediate(() => console.log(events.join(',')));
});

setImmediate(() => events.push('second'));

console.log('sync');
