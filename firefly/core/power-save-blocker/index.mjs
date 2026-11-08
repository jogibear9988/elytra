/** Validate the public boundary; native code exclusively owns OS assertions and request IDs. */
export default function createPowerSaveBlocker(native, isExited, error, unsupported) {
  const minimumId = -2147483648;
  const maximumId = 2147483647;
  let stopped = false;

  function alive() {
    if (stopped || isExited()) {
      stop();
      throw error('ERR_FIREFLY_APP_EXITED', 'Power save blocker application has exited');
    }
  }

  function receiver(value) {
    if (value !== powerSaveBlocker) {
      throw error('ERR_INVALID_ARG_TYPE', 'Illegal powerSaveBlocker receiver');
    }
    alive();
  }

  function id(value) {
    if (typeof value !== 'number' || !Number.isInteger(value) || Object.is(value, -0)) {
      throw error('ERR_INVALID_ARG_TYPE', 'Power save blocker ID must be an int32');
    }
    if (value < minimumId || value > maximumId) {
      throw error('ERR_OUT_OF_RANGE', 'Power save blocker ID is outside int32 range');
    }
  }

  function call(name, value) {
    const method = native[name];
    alive();
    if (typeof method !== 'function') {
      const suffix = name.replace('powerSaveBlocker', '');
      return unsupported('powerSaveBlocker.' + suffix[0].toLowerCase() + suffix.slice(1));
    }
    const result = method.call(native, value);
    // A hostile/reentrant peer may admit a request after the first shutdown returned.
    if (name === 'powerSaveBlockerStart' && (stopped || isExited())) {
      if (stopped) {
        cleanup();
      } else {
        stop();
      }
    }
    alive();
    return result;
  }

  function boolean(value) {
    if (typeof value !== 'boolean') {
      stop();
      throw error('ERR_FIREFLY_POWER_SAVE_DATA', 'Invalid native power save blocker result');
    }
    return value;
  }

  const powerSaveBlocker = {
    start(type) {
      receiver(this);
      if (typeof type !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Power save blocker type must be a string');
      }
      if (type !== 'prevent-app-suspension' && type !== 'prevent-display-sleep') {
        throw error('ERR_INVALID_ARG_VALUE', 'Unknown power save blocker type');
      }
      const result = call('powerSaveBlockerStart', type);
      if (!Number.isInteger(result) || Object.is(result, -0) || result < 0 || result > maximumId) {
        stop();
        throw error('ERR_FIREFLY_POWER_SAVE_DATA', 'Invalid native power save blocker ID');
      }
      return result;
    },

    stop(value) {
      receiver(this);
      id(value);
      return boolean(call('powerSaveBlockerStop', value));
    },

    isStarted(value) {
      receiver(this);
      id(value);
      return boolean(call('powerSaveBlockerIsStarted', value));
    }
  };

  function cleanup() {
    try {
      native.powerSaveBlockerShutdown?.();
    } catch {
      // Application teardown must continue if OS assertion release fails.
    }
  }

  function stop() {
    if (stopped) {
      return;
    }
    // Revoke first: even native cleanup reentry cannot create another request.
    stopped = true;
    cleanup();
  }

  return { powerSaveBlocker, stop };
}
