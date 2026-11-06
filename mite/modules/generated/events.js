/* Generated from mite/modules/core/events.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/event-rejections.mjs
function createRejectionObserver(capture, rejectionSymbol, schedule) {
  const apply = Reflect.apply;
  function deliver(emitter, error, type, args) {
    const handler = emitter[rejectionSymbol];
    if (typeof handler === "function") {
      apply(handler, emitter, [error, type, ...args]);
      return;
    }
    const previous = emitter[capture];
    emitter[capture] = false;
    try {
      emitter.emit("error", error);
    } finally {
      emitter[capture] = previous;
    }
  }
  function enqueue(record) {
    schedule(() => {
      try {
        deliver(record.emitter, record.error, record.type, record.args);
      } finally {
        record.emitter = void 0;
        record.error = void 0;
        record.type = void 0;
        record.args = void 0;
      }
    });
  }
  return function observe(emitter, result, type, args) {
    if (!emitter[capture] || result === void 0 || result === null) {
      return;
    }
    try {
      const then = result.then;
      if (typeof then === "function") {
        apply(then, result, [
          void 0,
          (error) => {
            enqueue({ emitter, error, type, args });
          }
        ]);
      }
    } catch (error) {
      emitter.emit("error", error);
    }
  };
}

// mite/modules/core/events.mjs
function createEventsModule(host) {
  const apply = Reflect.apply;
  const ownKeys = Reflect.ownKeys;
  const hasOwn = Function.call.bind(Object.prototype.hasOwnProperty);
  const slice = Function.call.bind(Array.prototype.slice);
  const errorMonitor = Symbol("events.errorMonitor");
  const captureRejectionSymbol = Symbol.for("nodejs.rejection");
  const capturedError = Error;
  const capture = Symbol("capture");
  const observeRejection = createRejectionObserver(
    capture,
    captureRejectionSymbol,
    (callback) => host.process.nextTick(callback)
  );
  let defaultMaxListeners = 10;
  function failure(Type, code, message) {
    const error = new Type(message);
    error.code = code;
    return error;
  }
  function checkListener(listener) {
    if (typeof listener !== "function") {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "The listener argument must be a function");
    }
  }
  function checkMax(value) {
    if (typeof value !== "number") {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "The n argument must be a number");
    }
    if (value < 0 || Number.isNaN(value)) {
      throw failure(RangeError, "ERR_OUT_OF_RANGE", "The n argument must be a non-negative number");
    }
  }
  function init(target, options) {
    const enabled = options?.captureRejections;
    if (enabled && typeof enabled !== "boolean") {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "captureRejections must be boolean");
    }
    if (!hasOwn(target, "_events") || target._events === void 0) {
      target._events = /* @__PURE__ */ Object.create(null);
      target._eventsCount = 0;
    }
    target._maxListeners = target._maxListeners;
    target[capture] = enabled || EventEmitter.prototype[capture];
  }
  function EventEmitter(options) {
    init(this, options);
  }
  EventEmitter.prototype[capture] = false;
  function events(target) {
    if (!hasOwn(target, "_events") || target._events === void 0) {
      init(target);
    }
    return target._events;
  }
  function original(listener) {
    return listener.listener || listener;
  }
  function add(target, type, listener, prepend) {
    checkListener(listener);
    let map = events(target);
    if (map.newListener && map.newListener.length) {
      target.emit("newListener", type, original(listener));
      map = events(target);
    }
    if (!hasOwn(map, type)) {
      map[type] = [];
      target._eventsCount++;
    }
    const list = map[type];
    if (prepend) {
      list.unshift(listener);
    } else {
      list.push(listener);
    }
    const maximum = target.getMaxListeners();
    if (maximum > 0 && list.length > maximum && !list.warned) {
      list.warned = true;
      const message = `Possible EventEmitter memory leak detected. ${list.length} ${String(type)} listeners added. MaxListeners is ${maximum}. Use emitter.setMaxListeners() to increase limit`;
      queueMicrotask(() => host.warn("MaxListenersExceededWarning", message));
    }
    return target;
  }
  function onceWrapper(target, type, listener) {
    let fired = false;
    function wrapper(...args) {
      if (fired) {
        return;
      }
      fired = true;
      target.removeListener(type, wrapper);
      return apply(listener, target, args);
    }
    wrapper.listener = listener;
    return wrapper;
  }
  EventEmitter.prototype.addListener = EventEmitter.prototype.on = function on(type, listener) {
    return add(this, type, listener, false);
  };
  EventEmitter.prototype.prependListener = function prependListener(type, listener) {
    return add(this, type, listener, true);
  };
  EventEmitter.prototype.once = function once(type, listener) {
    checkListener(listener);
    return this.on(type, onceWrapper(this, type, listener));
  };
  EventEmitter.prototype.prependOnceListener = function prependOnceListener(type, listener) {
    checkListener(listener);
    return this.prependListener(type, onceWrapper(this, type, listener));
  };
  EventEmitter.prototype.emit = function emit(type, ...args) {
    const map = events(this);
    if (type === "error") {
      if (map[errorMonitor]) {
        this.emit(errorMonitor, ...args);
      }
      if (!map.error || map.error.length === 0) {
        if (args[0] instanceof capturedError) {
          throw args[0];
        }
        const error = failure(
          capturedError,
          "ERR_UNHANDLED_ERROR",
          `Unhandled error. (${String(args[0])})`
        );
        error.context = args[0];
        throw error;
      }
    }
    const list = map[type];
    if (!list || list.length === 0) {
      return false;
    }
    for (const listener of slice(list)) {
      const result = apply(listener, this, args);
      observeRejection(this, result, type, args);
    }
    return true;
  };
  EventEmitter.prototype.removeListener = EventEmitter.prototype.off = function removeListener(type, listener) {
    checkListener(listener);
    const map = events(this);
    const list = map[type];
    if (!list) {
      return this;
    }
    for (let index = list.length - 1; index >= 0; --index) {
      if (list[index] === listener || list[index].listener === listener) {
        const removed = original(list[index]);
        list.splice(index, 1);
        if (list.length === 0) {
          delete map[type];
          this._eventsCount--;
        }
        if (map.removeListener && map.removeListener.length) {
          this.emit("removeListener", type, removed);
        }
        break;
      }
    }
    return this;
  };
  EventEmitter.prototype.removeAllListeners = function removeAllListeners(type) {
    const map = events(this);
    if (arguments.length === 0) {
      for (const key of ownKeys(map)) {
        if (key !== "removeListener") {
          this.removeAllListeners(key);
        }
      }
      this.removeAllListeners("removeListener");
      return this;
    }
    const list = map[type];
    if (!list) {
      return this;
    }
    for (const listener of slice(list).reverse()) {
      this.removeListener(type, listener);
    }
    return this;
  };
  EventEmitter.prototype.listeners = function listeners(type) {
    return this.rawListeners(type).map(original);
  };
  EventEmitter.prototype.rawListeners = function rawListeners(type) {
    return slice(events(this)[type] || []);
  };
  EventEmitter.prototype.listenerCount = function listenerCount(type, listener) {
    const list = events(this)[type] || [];
    if (listener === void 0) {
      return list.length;
    }
    return list.filter((item) => item === listener || item.listener === listener).length;
  };
  EventEmitter.prototype.eventNames = function eventNames() {
    return ownKeys(events(this));
  };
  EventEmitter.prototype.setMaxListeners = function setMaxListeners(value) {
    checkMax(value);
    this._maxListeners = value;
    return this;
  };
  EventEmitter.prototype.getMaxListeners = function getMaxListeners() {
    return this._maxListeners === void 0 ? defaultMaxListeners : this._maxListeners;
  };
  Object.defineProperty(EventEmitter, "defaultMaxListeners", {
    enumerable: true,
    get() {
      return defaultMaxListeners;
    },
    set(value) {
      checkMax(value);
      defaultMaxListeners = value;
    }
  });
  Object.defineProperty(EventEmitter, "captureRejections", {
    enumerable: true,
    get() {
      return EventEmitter.prototype[capture];
    },
    set(value) {
      if (typeof value !== "boolean") {
        throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "captureRejections must be boolean");
      }
      EventEmitter.prototype[capture] = value;
    }
  });
  EventEmitter.EventEmitter = EventEmitter;
  EventEmitter.errorMonitor = errorMonitor;
  EventEmitter.captureRejectionSymbol = captureRejectionSymbol;
  EventEmitter.listenerCount = function listenerCount(emitter, type) {
    return emitter.listenerCount(type);
  };
  EventEmitter.getEventListeners = function getEventListeners(emitter, type) {
    if (!emitter || typeof emitter.listeners !== "function") {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "Expected an EventEmitter");
    }
    return emitter.listeners(type);
  };
  EventEmitter.getMaxListeners = function getMaxListeners(emitter) {
    if (!emitter || typeof emitter.getMaxListeners !== "function") {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "Expected an EventEmitter");
    }
    return emitter.getMaxListeners();
  };
  EventEmitter.setMaxListeners = function setMaxListeners(value, ...targets) {
    checkMax(value);
    if (targets.length === 0) {
      defaultMaxListeners = value;
    }
    for (const target of targets) {
      if (!target || typeof target.setMaxListeners !== "function") {
        throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "Expected an EventEmitter");
      }
      target.setMaxListeners(value);
    }
  };
  EventEmitter.once = async function once(emitter, type, options = {}) {
    if (options === null || typeof options !== "object" || Array.isArray(options)) {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "options must be an object");
    }
    const { signal } = options;
    if (signal !== void 0 && !host.abort.isSignal(signal)) {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "options.signal must be an AbortSignal");
    }
    function aborted() {
      const error = new capturedError("The operation was aborted");
      error.name = "AbortError";
      error.code = "ABORT_ERR";
      error.cause = host.abort.reason(signal);
      return error;
    }
    if (signal !== void 0 && signal.aborted) {
      throw aborted();
    }
    if (!emitter || typeof emitter.once !== "function" || typeof emitter.removeListener !== "function") {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "Expected an EventEmitter");
    }
    return new Promise((resolve, reject) => {
      let complete = false;
      let unsubscribe = () => {
      };
      function cleanup() {
        unsubscribe();
        emitter.removeListener(type, success);
        emitter.removeListener("error", rejected);
      }
      function finish(error, args) {
        if (complete) {
          return;
        }
        complete = true;
        cleanup();
        if (args === void 0) {
          reject(error);
        } else {
          resolve(args);
        }
      }
      function success(...args) {
        finish(void 0, args);
      }
      function rejected(error) {
        finish(error);
      }
      if (signal !== void 0) {
        unsubscribe = host.abort.subscribe(signal, () => rejected(aborted()));
      }
      try {
        if (!complete) {
          emitter.once(type, success);
        }
        if (!complete && type !== "error") {
          emitter.once("error", rejected);
        }
      } catch (error) {
        rejected(error);
      } finally {
        if (complete) {
          cleanup();
        }
      }
    });
  };
  return EventEmitter;
}
return createEventsModule;
})()
