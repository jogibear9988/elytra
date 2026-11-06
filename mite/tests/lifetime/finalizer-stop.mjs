/** A fatal current callback discards queued cleanup rather than reviving JS at shutdown. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const registry = new FinalizationRegistry(() => console.log('UNEXPECTED cleanup after fatal stop'));
for (let index = 0; index < 32; index++) {
  registry.register({}, index);
}

setImmediate(() => {
  registry.unregister({});
  collect();
  throw Error('stopped before cleanup fixture');
});

console.log('entry completed');
