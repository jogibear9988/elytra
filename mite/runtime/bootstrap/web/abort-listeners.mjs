/** Track real abort listeners without globally rooting otherwise unreachable signals. */
export default function installAbortListeners(
  AbortSignal,
  { EventTarget, getEventAttributeValue, setEventAttributeValue, state, update, subscribe }
) {
  const add = Function.call.bind(EventTarget.prototype.addEventListener);
  const remove = Function.call.bind(EventTarget.prototype.removeEventListener);
  const apply = Reflect.apply;
  const guarded = new FinalizationRegistry((unsubscribe) => unsubscribe());

  function guardedRemoval(weakSignal, weakCallback, weakRecord, capture) {
    return () => {
      const signal = weakSignal.deref();
      const callback = weakCallback.deref();
      const record = weakRecord.deref();
      if (signal && callback && record) {
        remove(signal, 'abort', record.wrapper, capture);
        forget(signal, callback, capture, record);
      }
    };
  }

  function forget(signal, callback, capture, record) {
    const entry = state(signal);
    const records = entry.listeners.get(callback);
    if (records?.get(capture) !== record) {
      return;
    }
    records.delete(capture);
    if (records.size === 0) {
      entry.listeners.delete(callback);
    }
    entry.listenerCount--;
    guarded.unregister(record);
    record.unsubscribe();
    update(signal);
  }

  function addEventListener(type, callback, options) {
    const signal = this;
    const entry = state(signal);
    // Normalize once: caller-provided option getters must not be evaluated twice.
    type = String(type);
    if (type !== 'abort' || callback == null || !['function', 'object'].includes(typeof callback)) {
      return add(signal, type, callback, options);
    }
    const normalized =
      typeof options === 'object' && options !== null
        ? {
            capture: !!options.capture,
            passive: !!options.passive,
            once: !!options.once,
            signal: options.signal ?? undefined
          }
        : { capture: !!options, passive: false, once: false, signal: undefined };
    if (normalized.signal !== undefined) {
      // Foreign option signals cannot participate in the private lifetime registry.
      state(normalized.signal);
      if (state(normalized.signal).aborted) {
        return;
      }
    }
    let records = entry.listeners.get(callback);
    const existing = records?.get(normalized.capture);
    if (existing) {
      return add(signal, type, existing.wrapper, normalized);
    }
    const record = { unsubscribe: () => {}, wrapper: null };

    record.wrapper = function (event) {
      if (normalized.once) {
        forget(signal, callback, normalized.capture, record);
      }
      if (typeof callback === 'function') {
        return apply(callback, signal, [event]);
      }
      return apply(callback.handleEvent, callback, [event]);
    };
    // We own signal-option removal through private algorithms. The base primitive
    // still supplies dispatch, capture/once/passive ordering and error reporting.
    add(signal, type, record.wrapper, { ...normalized, signal: undefined });
    if (!records) {
      records = new Map();
      entry.listeners.set(callback, records);
    }
    records.set(normalized.capture, record);
    entry.listenerCount++;
    if (normalized.signal !== undefined) {
      record.unsubscribe = subscribe(
        normalized.signal,
        guardedRemoval(
          new WeakRef(signal),
          new WeakRef(callback),
          new WeakRef(record),
          normalized.capture
        )
      );
      guarded.register(record, record.unsubscribe, record);
    }
    update(signal);
  }

  function removeEventListener(type, callback, options) {
    const signal = this;
    const entry = state(signal);
    type = String(type);
    const capture = typeof options === 'object' && options !== null ? !!options.capture : !!options;
    const record = type === 'abort' ? entry.listeners.get(callback)?.get(capture) : undefined;
    remove(signal, type, record ? record.wrapper : callback, capture);
    if (record) {
      forget(signal, callback, capture, record);
    }
  }

  Object.defineProperties(AbortSignal.prototype, {
    addEventListener: { value: addEventListener, configurable: true, writable: true },
    removeEventListener: { value: removeEventListener, configurable: true, writable: true },
    onabort: {
      get() {
        state(this);
        return getEventAttributeValue(this, 'abort');
      },

      set(value) {
        state(this);
        setEventAttributeValue(this, 'abort', value);
        update(this);
      },

      configurable: true,
      enumerable: true
    }
  });
}
