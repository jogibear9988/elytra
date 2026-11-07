/**
 * TCP argument/error and AbortSignal policy shared by sockets and listeners.
 * The host provides native address validation and branded cancellation hooks;
 * these private helpers perform no I/O and never become application exports.
 */
export default function createNetworkPolicy(host) {
  const native = host.native;
  const abort = host.abort;

  function error(Type, code, message) {
    const e = new Type(message);
    e.code = code;
    return e;
  }

  function unsupported(name) {
    throw error(Error, 'ERR_MITE_UNSUPPORTED_API', `${name} is not implemented`);
  }

  function validateSignal(signal) {
    if (signal !== undefined && !abort.isSignal(signal)) {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'signal must be an AbortSignal');
    }
    return signal;
  }

  function abortError(signal) {
    const failure = error(Error, 'ABORT_ERR', 'The operation was aborted');
    failure.name = 'AbortError';
    failure.cause = abort.reason(signal);
    return failure;
  }

  /** Dispose at resource shutdown, including before a delayed lookup can complete. */
  function releaseSignal(state) {
    state.unsubscribe?.();
    state.unsubscribe = null;
  }

  function port(value) {
    if (typeof value !== 'number' && typeof value !== 'string') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'port must be a number or string');
    }
    const number = Number(value);
    if (
      (typeof value === 'string' && value.trim() === '') ||
      !Number.isInteger(number) ||
      number < 0 ||
      number > 65535
    ) {
      throw error(RangeError, 'ERR_SOCKET_BAD_PORT', `Invalid port: ${String(value)}`);
    }
    return number;
  }

  function ip(value) {
    if (typeof value !== 'string') {
      return 0;
    }
    const zone = value.indexOf('%');
    if (zone !== -1 && !/^[0-9A-Za-z.:-]+$/.test(value.slice(zone + 1))) {
      return 0;
    }
    return native.isIP(value);
  }

  function options(args) {
    const list = [...args];
    let callback;
    if (typeof list.at(-1) === 'function') {
      callback = list.pop();
    }
    let result;
    if (list[0] !== null && typeof list[0] === 'object') {
      result = { ...list[0] };
    } else if (typeof list[0] === 'string' && Number.isNaN(Number(list[0]))) {
      unsupported('Unix sockets and named pipes');
    } else {
      result = {
        port: list[0],
        host: typeof list[1] === 'string' ? list[1] : undefined,
        backlog: typeof list[1] === 'number' ? list[1] : list[2]
      };
    }
    return [result, callback];
  }

  function hostname(value, fallback) {
    value = value || fallback;
    if (typeof value !== 'string') {
      throw error(TypeError, 'ERR_INVALID_ARG_TYPE', 'host must be a string');
    }

    return value;
  }

  function systemError(status, syscall, address, port) {
    const info = native.error(status);
    const e = error(
      Error,
      info.code,
      `${syscall} ${info.code}: ${info.message}${address === undefined ? '' : ` ${address}:${port}`}`
    );
    e.errno = status;
    e.syscall = syscall;
    if (address !== undefined) {
      e.address = address;
      e.port = port;
    }
    return e;
  }

  return {
    error,
    unsupported,
    validateSignal,
    abortError,
    releaseSignal,
    port,
    ip,
    options,
    hostname,
    systemError
  };
}
