/** Process EventEmitter methods plus explicit, bounded native signal subscriptions. */
export default function installProcessEvents(host, process) {
  const { EventEmitter, signals, native } = host;
  const apply = Reflect.apply;
  const bind = Function.call.bind(Function.prototype.bind);
  const base = EventEmitter.prototype;
  const listeners = base.listenerCount;
  const active = new Map();
  const reserved = new Set([
    'beforeExit',
    'exit',
    'uncaughtException',
    'uncaughtExceptionMonitor',
    'unhandledRejection',
    'rejectionHandled',
    'warning',
    'message',
    'disconnect',
    'multipleResolves',
    'worker',
    'workerMessage'
  ]);

  function isSignal(type) {
    return typeof type === 'string' && Object.hasOwn(signals, type);
  }

  function reconcile() {
    for (const [type, number] of active) {
      if (apply(listeners, process, [type]) === 0) {
        native.stop(number);
        active.delete(type);
      }
    }
  }

  function adding(method) {
    return function (type, listener) {
      if (this !== process || typeof listener !== 'function') {
        return apply(method, this, arguments);
      }
      if (reserved.has(type)) {
        const error = new Error(`Process event is not implemented: ${type}`);
        error.code = 'ERR_MITE_UNSUPPORTED_PROCESS_EVENT';
        throw error;
      }
      if (isSignal(type) && !active.has(type)) {
        const number = signals[type];
        native.start(number, bind(process.emit, process, type, type));
        active.set(type, number);
      }
      try {
        return apply(method, this, arguments);
      } finally {
        // newListener can throw or reenter; retain only actual subscriptions.
        reconcile();
      }
    };
  }

  function removing(method) {
    return function () {
      try {
        return apply(method, this, arguments);
      } finally {
        if (this === process) {
          reconcile();
        }
      }
    };
  }

  const prototype = Object.create(base);
  const on = adding(base.on);
  const remove = removing(base.removeListener);
  Object.defineProperties(prototype, {
    on: { value: on, configurable: true, writable: true },
    addListener: { value: on, configurable: true, writable: true },
    prependListener: { value: adding(base.prependListener), configurable: true, writable: true },
    removeListener: { value: remove, configurable: true, writable: true },
    off: { value: remove, configurable: true, writable: true },
    removeAllListeners: {
      value: removing(base.removeAllListeners),
      configurable: true,
      writable: true
    }
  });
  Object.setPrototypeOf(process, prototype);
  EventEmitter.call(process);
}
