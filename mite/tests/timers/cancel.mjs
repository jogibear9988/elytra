/** Cancellation can remove another call from this batch or from a future batch. */
let second;

setImmediate(() => {
  clearImmediate(second);
  const nested = setImmediate(() => console.log('UNEXPECTED'));
  clearImmediate(nested);

  setImmediate(() => console.log('next iteration'));

  process.nextTick(() => console.log('tick'));
});

second = setImmediate(() => console.log('UNEXPECTED'));
// Cancel repeatedly before yielding; stale native IDs must not accumulate forever.
for (let index = 0; index < 5000; index++) {
  clearImmediate(setImmediate(() => console.log('UNEXPECTED')));
}
