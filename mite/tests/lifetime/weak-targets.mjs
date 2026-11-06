/** Targets survive synchronous code and microtasks, then become weak in a later task. */
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const refs = Array.from(
  { length: 64 },
  (_, index) => new WeakRef(index % 2 ? Symbol('kept') : { value: 42 })
);
collect();
console.log(
  'creation',
  refs.every((ref) => ref.deref() !== undefined)
);

queueMicrotask(() => {
  collect();
  console.log(
    'microtask',
    refs.every((ref) => ref.deref() !== undefined)
  );
});

setImmediate(() => {
  collect();
  console.log(
    'next',
    refs.some((ref) => ref.deref() === undefined)
  );
});
