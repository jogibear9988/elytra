/** Read-only OS queries and epoch-owned passive observations; shutdown cancellation is separate work. */
export default function createPowerMonitor(
  native,
  EventEmitter,
  isReady,
  isExited,
  error,
  unsupported
) {
  const known = new Set([
    'suspend',
    'resume',
    'on-ac',
    'on-battery',
    'thermal-state-change',
    'speed-limit-change',
    'shutdown',
    'lock-screen',
    'unlock-screen',
    'user-did-become-active',
    'user-did-resign-active'
  ]);
  const meta = new Set(['newListener', 'removeListener', 'error']);
  if (typeof EventEmitter.errorMonitor === 'symbol') {
    meta.add(EventEmitter.errorMonitor);
  }
  const thermal = new Set(['unknown', 'nominal', 'fair', 'serious', 'critical']);
  const idle = new Set(['active', 'idle', 'locked', 'unknown']);
  const parse = JSON.parse;
  const token = Symbol('powerMonitor');
  let capabilities;
  let stopped = false;
  let epoch = null;
  let next = 0;
  let transitioning = false;
  let cleaning = false;

  function alive() {
    if (stopped || isExited()) {
      throw error('ERR_FIREFLY_APP_EXITED', 'Power monitor application has exited');
    }
  }

  function ready() {
    alive();
    if (!isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'Power queries require app readiness');
    }
  }

  function receiver(value) {
    if (value !== powerMonitor) {
      throw error('ERR_INVALID_ARG_TYPE', 'Illegal powerMonitor receiver');
    }
  }

  function data(message) {
    return error('ERR_FIREFLY_POWER_MONITOR_DATA', message);
  }

  function decode(value) {
    if (typeof value !== 'string' || value.length > 16384) {
      throw data('Invalid native power snapshot');
    }
    try {
      return parse(value);
    } catch {
      throw data('Invalid native power JSON');
    }
  }

  function events() {
    if (!capabilities) {
      const value =
        typeof native.powerMonitorCapabilities === 'function'
          ? decode(native.powerMonitorCapabilities())
          : [];
      alive();
      if (
        !Array.isArray(value) ||
        value.length > known.size ||
        value.some((name) => !known.has(name)) ||
        new Set(value).size !== value.length
      ) {
        throw data('Invalid native power event capabilities');
      }
      capabilities = new Set(value);
    }
    return capabilities;
  }

  function validate(name, listener) {
    alive();
    if (typeof listener !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'Power listener must be a function');
    }
    if (!meta.has(name) && (!known.has(name) || !events().has(name))) {
      unsupported(`powerMonitor event ${String(name)}`);
    }
    alive();
  }

  function interested() {
    return [...known].some((name) => powerMonitor.listenerCount(name) > 0);
  }

  function clearNativeListeners() {
    // Rollback/shutdown does not invoke user meta hooks that could create new
    // subscriptions while the native generation is being retired.
    cleaning = true;
    try {
      for (const name of known) {
        EventEmitter.prototype.removeAllListeners.call(powerMonitor, name);
      }
    } finally {
      cleaning = false;
    }
  }

  function synchronize() {
    if (transitioning) {
      return;
    }
    transitioning = true;
    try {
      for (let count = 0; ; ++count) {
        if (count >= 32) {
          // Retire admission before cleanup; a synchronous stop callback may
          // mutate listeners, so clear them after it while reconciliation is held.
          const id = epoch;
          epoch = null;
          try {
            if (id !== null) {
              native.powerMonitorStop(id);
            }
          } catch {
          } finally {
            clearNativeListeners();
          }
          throw error(
            'ERR_FIREFLY_POWER_MONITOR_REENTRY',
            'Power listener transitions did not settle'
          );
        }
        const wanted = !stopped && !isExited() && isReady() && interested();
        if (wanted && epoch === null) {
          if (next >= 2147483647) {
            throw error('ERR_FIREFLY_POWER_MONITOR_LIMIT', 'Power observation epochs exhausted');
          }
          const id = ++next;
          epoch = id;
          try {
            native.powerMonitorStart(id);
          } catch (failure) {
            if (epoch === id) {
              epoch = null;
            }
            try {
              native.powerMonitorStop(id);
            } catch {}
            clearNativeListeners();
            throw failure;
          }
        } else if (!wanted && epoch !== null) {
          const id = epoch;
          epoch = null;
          native.powerMonitorStop(id);
        } else {
          break;
        }
      }
    } finally {
      transitioning = false;
    }
  }

  function add(object, name, listener, prepend) {
    receiver(object);
    validate(name, listener);
    const method = prepend ? EventEmitter.prototype.prependListener : EventEmitter.prototype.on;
    method.call(powerMonitor, name, listener);
    if (stopped || isExited()) {
      clearNativeListeners();
      alive();
    }
    synchronize();
    alive();
    return powerMonitor;
  }

  function query(kind, threshold) {
    ready();
    const snapshot = native.powerMonitorQuery(kind, threshold);
    // A native boundary may synchronously exit/reenter the app in an embedder.
    ready();
    const value = decode(snapshot);
    if (
      !value ||
      typeof value !== 'object' ||
      Array.isArray(value) ||
      !Object.hasOwn(value, 'value')
    ) {
      throw data('Native power query has no value');
    }
    const result = value.value;
    if (
      (kind === 'idle-time' && (!Number.isSafeInteger(result) || result < 0)) ||
      (kind === 'idle-state' && !idle.has(result)) ||
      (kind === 'thermal-state' && !thermal.has(result)) ||
      (kind === 'battery-power' && typeof result !== 'boolean')
    ) {
      throw data('Invalid native power query value');
    }
    return result;
  }

  class PowerMonitor extends EventEmitter {
    constructor(key) {
      super();
      if (key !== token) {
        throw error('ERR_ILLEGAL_CONSTRUCTOR', 'Use the shared powerMonitor object');
      }
    }

    on(name, listener) {
      return add(this, name, listener, false);
    }

    addListener(name, listener) {
      return add(this, name, listener, false);
    }

    prependListener(name, listener) {
      return add(this, name, listener, true);
    }

    removeListener(name, listener) {
      receiver(this);
      super.removeListener(name, listener);
      synchronize();
      return this;
    }

    off(name, listener) {
      return this.removeListener(name, listener);
    }

    removeAllListeners(...args) {
      receiver(this);
      super.removeAllListeners(...args);
      synchronize();
      return this;
    }

    emit(name, ...args) {
      if (cleaning && (name === 'newListener' || name === 'removeListener')) {
        return false;
      }
      return super.emit(name, ...args);
    }

    getSystemIdleTime() {
      receiver(this);
      return query('idle-time');
    }

    getSystemIdleState(threshold) {
      receiver(this);
      if (typeof threshold !== 'number' || !Number.isInteger(threshold)) {
        throw error('ERR_INVALID_ARG_TYPE', 'Idle threshold must be an integer');
      }
      if (threshold <= 0 || threshold > 2147483647) {
        throw error('ERR_OUT_OF_RANGE', 'Idle threshold must be positive and within INTMAX');
      }
      return query('idle-state', threshold);
    }

    getCurrentThermalState() {
      receiver(this);
      return query('thermal-state');
    }

    isOnBatteryPower() {
      receiver(this);
      return query('battery-power');
    }

    get onBatteryPower() {
      receiver(this);
      return query('battery-power');
    }
  }
  const powerMonitor = new PowerMonitor(token);

  function dispatch(value) {
    if (stopped || isExited() || epoch === null || value?.epoch !== epoch) {
      return;
    }
    if (transitioning) {
      // Do not expose a synchronous startup callback before native start succeeds.
      queueMicrotask(() => dispatch(value));

      return;
    }
    const name = value.type;
    if (!known.has(name) || !events().has(name)) {
      throw data('Unknown native power event');
    }
    if (name === 'thermal-state-change') {
      if (!thermal.has(value.state)) {
        throw data('Invalid native thermal state');
      }
      powerMonitor.emit(name, { state: value.state });
    } else if (name === 'speed-limit-change') {
      if (!Number.isInteger(value.limit) || value.limit < 0 || value.limit > 100) {
        throw data('Invalid native CPU speed limit');
      }
      powerMonitor.emit(name, { limit: value.limit });
    } else if (name === 'shutdown') {
      powerMonitor.emit(
        name,
        Object.freeze({
          defaultPrevented: false,

          preventDefault() {
            unsupported('powerMonitor shutdown cancellation');
          }
        })
      );
    } else {
      powerMonitor.emit(name);
    }
  }

  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const id = epoch;
    epoch = null;
    clearNativeListeners();
    if (id !== null) {
      try {
        native.powerMonitorStop(id);
      } catch {
        // Exit still retires the JS epoch and other services if host cleanup
        // fails. The native bridge also stops its controller before VM teardown.
      }
    }
  }

  return { powerMonitor, ready: synchronize, dispatch, stop };
}
