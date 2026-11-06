/** A target used as its token is still weak; one user token can remove an entire group. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const seen = new Set();
const registry = new FinalizationRegistry((value) => {
  seen.add(value);
});
const shared = {};
registry.register({}, 'cancelled', shared);
registry.register({}, 'cancelled', shared);
console.log('group', registry.unregister(shared), registry.unregister(shared));
const refs = [];
for (let index = 0; index < 32; index++) {
  const target = {};
  registry.register(target, index, target);
  refs.push(new WeakRef(target));
}
let turns = 0;

function check() {
  registry.unregister({});
  collect();
  if (++turns < 8) {
    setImmediate(check);
    return;
  }
  console.log(
    'tokens',
    refs.some((ref) => ref.deref() === undefined),
    seen.size > 0,
    !seen.has('cancelled')
  );
}

setImmediate(check);
