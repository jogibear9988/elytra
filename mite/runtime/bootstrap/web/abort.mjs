/**
 * Realm-local cancellation state. Private abort algorithms run independently of
 * public event propagation, so synthetic events or stopImmediatePropagation cannot
 * cancel or suppress native consumer work. Composite links use weak dependents.
 */
export default function createAbortGlobals({
  Event,
  EventTarget,
  defineEventAttribute,
  DOMException
}) {
  const signals = new WeakMap();
  const controllers = new WeakMap();
  const token = {};
  const schedule = globalThis.setTimeout;
  const clear = globalThis.clearTimeout;
  const tick = process.nextTick;

  function error(Type, code, message) {
    return Object.assign(new Type(message), { code });
  }

  function state(signal) {
    const found = signals.get(signal);
    if (!found) {
      throw error(TypeError, 'ERR_INVALID_THIS', 'Value must be an AbortSignal');
    }
    return found;
  }

  function reason(value) {
    return value === undefined
      ? new DOMException('This operation was aborted', 'AbortError')
      : value;
  }

  /** Mark the complete dependent graph before any application callback can run. */
  function abort(signal, value) {
    if (state(signal).aborted) {
      return;
    }
    const pending = [signal];
    const seen = new Set();
    for (let index = 0; index < pending.length; index++) {
      const current = pending[index];
      const entry = state(current);
      if (seen.has(current) || entry.aborted) {
        continue;
      }
      seen.add(current);
      entry.aborted = true;
      entry.reason = value;
      for (const dependent of entry.dependents) {
        const target = dependent.deref();
        if (target) {
          pending.push(target);
        } else {
          entry.dependents.delete(dependent);
        }
      }
    }
    for (const current of seen) {
      const entry = state(current);
      for (const [source, link] of entry.sources) {
        state(source).dependents.delete(link);
      }
      entry.sources = [];
      entry.dependents.clear();
      const algorithms = [...entry.algorithms];
      entry.algorithms.clear();
      for (const algorithm of algorithms) {
        try {
          algorithm();
        } catch (failure) {
          tick(() => {
            throw failure;
          });
        }
      }
      current.dispatchEvent(new Event('abort'));
    }
  }

  class AbortSignal extends EventTarget {
    constructor(key) {
      if (key !== token) {
        throw error(TypeError, 'ERR_ILLEGAL_CONSTRUCTOR', 'Illegal constructor');
      }
      super();
      signals.set(this, {
        aborted: false,
        reason: undefined,
        algorithms: new Set(),
        dependents: new Set(),
        sources: []
      });
    }

    get aborted() {
      return state(this).aborted;
    }

    get reason() {
      return state(this).reason;
    }

    throwIfAborted() {
      const entry = state(this);
      if (entry.aborted) {
        throw entry.reason;
      }
    }

    static abort(value) {
      const signal = new AbortSignal(token);
      abort(signal, reason(value));
      return signal;
    }

    static timeout(delay) {
      if (typeof delay !== 'number') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'delay must be a number');
      }
      if (!Number.isInteger(delay) || delay < 0 || delay > 0xffffffff) {
        throw error(RangeError, 'ERR_OUT_OF_RANGE', 'delay must be an unsigned 32-bit integer');
      }
      const signal = new AbortSignal(token);
      const weak = new WeakRef(signal);
      // A deadline alone must not keep the process alive or strongly retain a signal.
      const timer = schedule(() => {
        const target = weak.deref();
        if (target) {
          abort(
            target,
            new DOMException('The operation was aborted due to timeout', 'TimeoutError')
          );
        }
      }, delay);
      timer.unref();

      state(signal).algorithms.add(() => clear(timer));

      return signal;
    }

    static any(values) {
      if (values == null || typeof values[Symbol.iterator] !== 'function') {
        throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'signals must be iterable');
      }
      const sources = [];
      for (const signal of values) {
        if (!signals.has(signal)) {
          throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'Every entry must be an AbortSignal');
        }
        sources.push(signal);
      }
      const result = new AbortSignal(token);
      const already = sources.find((signal) => state(signal).aborted);
      if (already) {
        abort(result, state(already).reason);
        return result;
      }
      for (const source of new Set(sources)) {
        const link = new WeakRef(result);
        state(source).dependents.add(link);
        state(result).sources.push([source, link]);
      }
      return result;
    }
  }
  defineEventAttribute(AbortSignal.prototype, 'abort');

  class AbortController {
    constructor() {
      controllers.set(this, new AbortSignal(token));
    }

    get signal() {
      if (!controllers.has(this)) {
        throw new TypeError('Value must be an AbortController');
      }
      return controllers.get(this);
    }

    abort(value) {
      const signal = controllers.get(this);
      if (!signal) {
        throw new TypeError('Value must be an AbortController');
      }
      abort(signal, reason(value));
    }
  }
  for (const Type of [AbortSignal, AbortController]) {
    Object.defineProperty(Type.prototype, Symbol.toStringTag, {
      value: Type.name,
      configurable: true
    });
  }
  return {
    AbortSignal,
    AbortController,
    // Only trusted builtin factories receive these hooks; never publish them globally.
    isSignal: (value) => signals.has(value),

    reason: (signal) => state(signal).reason,

    subscribe(signal, callback) {
      const entry = state(signal);
      if (entry.aborted) {
        callback();
        return () => {};
      }
      entry.algorithms.add(callback);
      return () => entry.algorithms.delete(callback);
    }
  };
}
