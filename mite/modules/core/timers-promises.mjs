/** Promise timers share native handles and private, propagation-independent abort hooks. */
export default function createTimerPromises(host) {
  const { timers, abort } = host;

  function invalid(code, message) {
    return Object.assign(new TypeError(message), { code });
  }

  function options(value) {
    if (value === undefined) {
      value = {};
    }
    if (value === null || typeof value !== 'object' || Array.isArray(value)) {
      throw invalid('ERR_INVALID_ARG_TYPE', 'options must be an object');
    }
    const { signal, ref = true } = value;
    if (signal !== undefined && !abort.isSignal(signal)) {
      throw invalid('ERR_INVALID_ARG_TYPE', 'signal must be an AbortSignal');
    }
    if (typeof ref !== 'boolean') {
      throw invalid('ERR_INVALID_ARG_TYPE', 'ref must be a boolean');
    }
    return { signal, ref };
  }

  function abortError(signal) {
    const error = new Error('The operation was aborted');
    error.name = 'AbortError';
    error.code = 'ABORT_ERR';
    error.cause = abort.reason(signal);
    return error;
  }

  function wait(schedule, cancel, value, config) {
    if (config.signal !== undefined && config.signal.aborted) {
      return Promise.reject(abortError(config.signal));
    }

    let unsubscribe = () => {};

    const promise = new Promise((resolve, reject) => {
      let complete = false;
      const handle = schedule(() => {
        if (complete) {
          return;
        }
        complete = true;
        resolve(value);
      });
      if (!config.ref) {
        handle.unref();
      }
      if (config.signal !== undefined) {
        unsubscribe = abort.subscribe(config.signal, () => {
          if (complete) {
            return;
          }
          complete = true;
          cancel(handle);
          reject(abortError(config.signal));
        });
      }
    });
    return config.signal === undefined ? promise : promise.finally(() => unsubscribe());
  }

  return {
    setTimeout(delay = 1, value, opts) {
      try {
        if (typeof delay !== 'number') {
          throw invalid('ERR_INVALID_ARG_TYPE', 'delay must be a number');
        }
        return wait(
          (callback) => timers.setTimeout(callback, delay),
          timers.clearTimeout,
          value,
          options(opts)
        );
      } catch (error) {
        return Promise.reject(error);
      }
    },

    setImmediate(value, opts) {
      try {
        return wait(timers.setImmediate, timers.clearImmediate, value, options(opts));
      } catch (error) {
        return Promise.reject(error);
      }
    }
  };
}
