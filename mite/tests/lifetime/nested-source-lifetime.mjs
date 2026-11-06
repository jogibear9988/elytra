/** A reachable leaf must still abort after an intermediate composite is collected. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const source = new AbortController();
let intermediate;
const leaf = (() => {
  const middle = AbortSignal.any([source.signal]);
  intermediate = new WeakRef(middle);
  return AbortSignal.any([middle]);
})();
let notifications = 0;

leaf.addEventListener('abort', () => notifications++);

let turns = 0;

function check() {
  collect();
  if (++turns < 8) {
    setImmediate(check);
    return;
  }
  source.abort('original reason');
  console.log('intermediate collected', intermediate.deref() === undefined);
  console.log('leaf', leaf.aborted, leaf.reason === 'original reason', notifications);
}

setImmediate(check);
