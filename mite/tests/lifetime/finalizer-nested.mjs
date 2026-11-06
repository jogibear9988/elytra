/** A cleanup can make more objects collectible without losing the next cleanup wakeup. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
let first = false;
let second = false;
const nested = new FinalizationRegistry(() => {
  second = true;
});
const registry = new FinalizationRegistry(() => {
  if (!first) {
    first = true;
    nested.register({}, 'nested');
    collect();
  }
});
for (let index = 0; index < 32; index++) {
  registry.register({}, index);
}
let turns = 0;

function check() {
  registry.unregister({});
  nested.unregister({});
  collect();
  if (++turns < 12) {
    setImmediate(check);
    return;
  }
  console.log('nested cleanup', first, second);
}

setImmediate(check);
