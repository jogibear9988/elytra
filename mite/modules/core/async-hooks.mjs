/** Async scope propagation is provided by the native VM job/resource carriers. */
export default function createAsyncHooks({ native }) {
  const apply = Reflect.apply;
  const same = Object.is;
  const states = new WeakMap();
  const resources = new WeakMap();

  function unsupported(name) {
    const error = new Error(`async_hooks.${name} is not implemented`);
    error.code = 'ERR_MITE_UNSUPPORTED_API';
    throw error;
  }

  function callback(value) {
    if (typeof value !== 'function') {
      const error = new TypeError('Callback must be a function');
      error.code = 'ERR_INVALID_ARG_TYPE';
      throw error;
    }
  }

  function state(map, receiver) {
    if (!map.has(receiver)) {
      throw new TypeError('Invalid async context receiver');
    }
    return map.get(receiver);
  }

  class AsyncLocalStorage {
    constructor(options) {
      if (options !== undefined) {
        unsupported('AsyncLocalStorage options');
      }
      states.set(this, native.storage());
    }

    getStore() {
      return state(states, this).get();
    }

    enterWith(store) {
      state(states, this).enter(store);
    }

    disable() {
      state(states, this).disable();
    }

    run(store, fn, ...args) {
      callback(fn);
      const local = state(states, this);
      if (same(local.get(), store)) {
        return apply(fn, null, args);
      }
      return local.run(store, false, fn, null, ...args);
    }

    exit(fn, ...args) {
      callback(fn);
      return this.run(undefined, fn, ...args);
    }

    static bind(fn) {
      callback(fn);
      const resource = new AsyncResource('bound-anonymous-fn');
      return function (...args) {
        return resource.runInAsyncScope(fn, this, ...args);
      };
    }

    static snapshot() {
      const resource = new AsyncResource('bound-anonymous-fn');
      return (fn, ...args) => resource.runInAsyncScope(fn, undefined, ...args);
    }
  }
  class AsyncResource {
    constructor(type, options) {
      if (typeof type !== 'string') {
        throw new TypeError('AsyncResource type must be a string');
      }
      if (options !== undefined) {
        unsupported('AsyncResource options');
      }
      resources.set(this, native.resource());
    }

    runInAsyncScope(fn, receiver, ...args) {
      callback(fn);
      return state(resources, this).run(fn, receiver, ...args);
    }

    asyncId() {
      return state(resources, this).id;
    }

    triggerAsyncId() {
      return state(resources, this).trigger;
    }

    emitDestroy() {
      // No createHook observers are admitted. Destruction notification has no
      // observable hooks; Node still permits runInAsyncScope after emitDestroy.
      state(resources, this);
      return this;
    }

    bind(fn, receiver) {
      callback(fn);
      const owner = this;
      const fixed = arguments.length > 1;
      return function (...args) {
        return owner.runInAsyncScope(fn, fixed ? receiver : this, ...args);
      };
    }

    static bind(fn, type, receiver) {
      callback(fn);
      const resource = new AsyncResource(type ?? (fn.name || 'bound-anonymous-fn'));
      return arguments.length > 2 ? resource.bind(fn, receiver) : resource.bind(fn);
    }
  }
  return {
    AsyncLocalStorage,
    AsyncResource,

    executionAsyncId: () => native.ids(false),

    triggerAsyncId: () => native.ids(true),

    executionAsyncResource: () => unsupported('executionAsyncResource'),

    createHook: () => unsupported('createHook')
  };
}
