/** Unused, removed, consumed-once and guard-cancelled listeners must not retain composites. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const source = new AbortController();
const guard = new AbortController();
const groups = { unused: [], removed: [], once: [], guarded: [], attributes: [] };
let once = 0;
for (let index = 0; index < 32; index++) {
  groups.unused.push(new WeakRef(AbortSignal.any([source.signal])));
  const removed = AbortSignal.any([source.signal]);

  const callback = () => {
    throw Error('Removed listener fired');
  };

  removed.addEventListener('abort', callback);
  removed.removeEventListener('abort', callback);
  groups.removed.push(new WeakRef(removed));
  const consumed = AbortSignal.any([source.signal]);

  consumed.addEventListener('abort', () => once++, { once: true });

  consumed.dispatchEvent(new Event('abort'));
  groups.once.push(new WeakRef(consumed));
  const guarded = AbortSignal.any([source.signal]);
  guarded.addEventListener('abort', callback, { signal: guard.signal });
  groups.guarded.push(new WeakRef(guarded));
  const attribute = AbortSignal.any([source.signal]);
  attribute.onabort = callback;
  attribute.onabort = null;
  groups.attributes.push(new WeakRef(attribute));
}
guard.abort();
let turns = 0;

function check() {
  collect();
  if (++turns < 8) {
    setImmediate(check);
    return;
  }
  console.log(
    'release',
    Object.values(groups).every((refs) => refs.some((ref) => ref.deref() === undefined)),
    once === 32
  );
  source.abort();
}

setImmediate(check);
