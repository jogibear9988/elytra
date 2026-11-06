/**
 * Bridge LibJS weak targets to the host's end-of-checkpoint retention roots.
 * The native weak reference still owns lookup/weakness/brand validation; this
 * adapter supplies AddToKeptObjects until the runtime ends the current job.
 */
export default function createWeakRefAdapter(keepAlive) {
  const NativeWeakRef = globalThis.WeakRef;
  const dereference = Function.call.bind(NativeWeakRef.prototype.deref);
  return class WeakRef extends NativeWeakRef {
    constructor(target) {
      super(target);
      keepAlive(target);
    }

    deref() {
      const target = dereference(this);
      if (target !== undefined) {
        keepAlive(target);
      }
      return target;
    }
  };
}

/**
 * The pinned registry traces unregister tokens strongly. Keep user tokens as weak
 * keys and hand the native registry independent group tokens, so using a target
 * as its own unregister token cannot accidentally keep that target alive.
 */
export function createFinalizationAdapter() {
  const NativeRegistry = globalThis.FinalizationRegistry;
  const register = Function.call.bind(NativeRegistry.prototype.register);
  const unregister = Function.call.bind(NativeRegistry.prototype.unregister);
  const tokens = new WeakMap();
  const keyFor = Symbol.keyFor;

  function table(registry) {
    const value = tokens.get(registry);
    if (!value) {
      throw new TypeError('Value must be a FinalizationRegistry');
    }
    return value;
  }

  function weak(value) {
    return (
      (typeof value === 'object' && value !== null) ||
      typeof value === 'function' ||
      (typeof value === 'symbol' && keyFor(value) === undefined)
    );
  }

  return class FinalizationRegistry extends NativeRegistry {
    constructor(cleanup) {
      super(cleanup);
      tokens.set(this, new WeakMap());
    }

    register(target, held, token = undefined) {
      const groups = table(this);
      if (!weak(target) || target === held || (token !== undefined && !weak(token))) {
        throw new TypeError('Invalid finalization target, held value or unregister token');
      }
      if (token === undefined) {
        return register(this, target, held);
      }
      let group = groups.get(token);
      if (!group) {
        group = {};
        groups.set(token, group);
      }
      return register(this, target, held, group);
    }

    unregister(token) {
      const groups = table(this);
      if (!weak(token)) {
        throw new TypeError('Invalid unregister token');
      }
      const group = groups.get(token);
      if (!group) {
        return false;
      }
      const result = unregister(this, group);
      groups.delete(token);
      return result;
    }
  };
}
