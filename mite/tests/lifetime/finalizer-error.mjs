/** Fatal cleanup exceptions follow ordinary asynchronous error/shutdown policy. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const registry = new FinalizationRegistry(() => {
  throw Error('finalizer failure fixture');
});
for (let index = 0; index < 64; index++) {
  registry.register({}, index);
}

setImmediate(function check() {
  registry.unregister({});
  collect();
  setImmediate(check);
});

console.log('entry completed');
