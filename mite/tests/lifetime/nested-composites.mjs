/** Deep/diamond composition must retain observed leaves without keeping intermediates. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const source = new AbortController();
const middleRefs = [];
let notifications = 0;
for (let index = 0; index < 32; index++) {
  const left = AbortSignal.any([source.signal]);
  const right = AbortSignal.any([source.signal]);
  const leaf = AbortSignal.any([left, right, source.signal, AbortSignal.any([])]);
  middleRefs.push(new WeakRef(left), new WeakRef(right));

  leaf.addEventListener('abort', () => notifications++, { once: true });
}
let turns = 0;

function check() {
  collect();
  if (++turns < 8) {
    setImmediate(check);
    return;
  }
  const intermediatesCollected = middleRefs.every((ref) => ref.deref() === undefined);
  source.abort('original reason');
  console.log('nested', intermediatesCollected, notifications);
}

setImmediate(check);
