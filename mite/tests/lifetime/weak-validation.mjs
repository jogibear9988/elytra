/** Native brands and valid symbol targets remain intact through the host adapters. */
const value = Symbol('weak');
const ref = new WeakRef(value);
console.log(
  'symbol',
  ref.deref() === value,
  ref instanceof WeakRef,
  Object.prototype.toString.call(ref)
);
const registry = new FinalizationRegistry(() => {});
const target = {};
for (const operation of [
  () => new WeakRef(1),
  () => new WeakRef(Symbol.for('registered')),
  () => WeakRef.prototype.deref.call({}),
  () => registry.register(target, target),
  () => registry.register(target, 1, Symbol.for('registered')),
  () => registry.unregister(1),
  () => FinalizationRegistry.prototype.register.call({}, target, 1)
]) {
  try {
    operation();
  } catch (error) {
    console.log(error.name);
  }
}
