import installWarnings from './process-warnings.mjs';

/** Process events backed by the native loop, terminal emitter and bounded signals. */
export default function installProcessEvents(host, process) {
  const { EventEmitter, signals, native } = host;
  const apply = Reflect.apply;
  const bind = Function.call.bind(Function.prototype.bind);
  const base = EventEmitter.prototype;
  const listeners = base.listenerCount;
  const emit = base.emit;
  const active = new Map();
  const reserved = new Set([
    'beforeExit',
    'exit',
    'uncaughtException',
    'uncaughtExceptionMonitor',
    'unhandledRejection',
    'rejectionHandled',
    'message',
    'disconnect',
    'multipleResolves',
    'worker',
    'workerMessage'
  ]);
  // Only the runtime's validated inherited-channel role admits process IPC events.
  if (host.childIpc) {
    reserved.delete('message');
    reserved.delete('disconnect');
  }
  // The native runtime installs a real once-only terminal emitter before granting this flag.
  if (host.exitEvents === true) {
    reserved.delete('exit');
  }

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
  if (typeof host.installErrorHandler === 'function') {
    // Install actual delivery before admitting subscriptions. The dispatcher is
    // private and rooted by the native loop; user code never receives the host.
    host.installErrorHandler((type, value, auxiliary) => {
      if (type === 'uncaughtException') {
        apply(emit, process, ['uncaughtExceptionMonitor', value, auxiliary]);
        return apply(emit, process, [type, value, auxiliary]);
      }
      if (type === 'unhandledRejection') {
        return apply(emit, process, [type, value, auxiliary]);
      }
      if (type === 'rejectionHandled') {
        return apply(emit, process, [type, value]);
      }
      throw new Error('Invalid native process error event');
    });

    for (const type of [
      'uncaughtException',
      'uncaughtExceptionMonitor',
      'unhandledRejection',
      'rejectionHandled'
    ]) {
      reserved.delete(type);
    }
  }
  installWarnings(process);
}
