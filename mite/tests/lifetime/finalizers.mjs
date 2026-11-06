/** Cleanup is deferred from collection and followed by the normal Promise checkpoint. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
let collecting = false;
let reactions = 0;
const values = new Set();
const registry = new FinalizationRegistry((value) => {
  if (collecting || values.has(value) || value === 'unregistered') {
    throw Error('Invalid finalization dispatch');
  }
  values.add(value);

  Promise.resolve().then(() => reactions++);
});
const token = {};
registry.register({}, 'unregistered', token);
console.log('unregister', registry.unregister(token), registry.unregister(token));
for (let index = 0; index < 64; index++) {
  registry.register({}, index);
}
let turns = 0;

function check() {
  registry.unregister({}); // Keep the registry itself reachable while its targets die.
  collecting = true;
  collect();
  collecting = false;
  if (++turns < 8) {
    setImmediate(check);
    return;
  }
  console.log('cleanup', values.size > 0, reactions === values.size);
}

setImmediate(check);
