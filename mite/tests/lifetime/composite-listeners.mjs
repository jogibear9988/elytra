/** Source-owned listener retention survives collection without an application signal variable. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const source = new AbortController();
let functions = 0;
let attributes = 0;
let objects = 0;
for (let index = 0; index < 32; index++) {
  AbortSignal.any([source.signal]).addEventListener('abort', () => functions++, { once: true });

  AbortSignal.any([source.signal]).onabort = () => attributes++;

  AbortSignal.any([source.signal]).addEventListener('abort', {
    handleEvent() {
      objects++;
    }
  });
}

setImmediate(() => {
  collect();
  source.abort('done');
  console.log('listeners', functions, attributes, objects);
});
