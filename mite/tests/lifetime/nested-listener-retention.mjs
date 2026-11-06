/** Reachable original sources must retain listener-only leaves across intermediate collection. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const source = new AbortController();
let notifications = 0;
for (let index = 0; index < 32; index++) {
  const middle = AbortSignal.any([source.signal]);

  AbortSignal.any([middle]).addEventListener('abort', () => notifications++, { once: true });
}
let turns = 0;

function check() {
  collect();
  if (++turns < 8) {
    setImmediate(check);
    return;
  }
  source.abort('original reason');
  console.log('nested listeners', notifications);
}

setImmediate(check);
