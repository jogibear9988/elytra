/** OS signal delivery, independent of permission to install signal watchers. */
export default function installProcessKill(process, signals, native) {
  // Capture the real OS table before application code can mutate os.constants.
  const numbers = Object.assign(Object.create(null), signals);
  const hasOwn = Object.hasOwn;
  const send = native.kill;

  function kill(pid, sig) {
    // Node's legacy API intentionally accepts values loosely equal to int32.
    // Keep coercion order: signal validation precedes the final native PID cast.
    if (pid != (pid | 0)) {
      throw Object.assign(new TypeError('The pid argument must be a number'), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }
    if (sig !== (sig | 0)) {
      sig ||= 'SIGTERM';
      if (!hasOwn(numbers, sig)) {
        throw Object.assign(new TypeError(`Unknown signal: ${sig}`), {
          code: 'ERR_UNKNOWN_SIGNAL'
        });
      }
      sig = numbers[sig];
    }
    send(pid | 0, sig | 0);
    return true;
  }

  Object.defineProperties(process, {
    kill: { value: kill, writable: true, configurable: true, enumerable: true },
    _kill: {
      value: function _kill() {
        throw Object.assign(new Error('The private Node process._kill hook is not supported'), {
          code: 'ERR_MITE_UNSUPPORTED_API'
        });
      },

      writable: true,
      configurable: true,
      enumerable: true
    }
  });
}
