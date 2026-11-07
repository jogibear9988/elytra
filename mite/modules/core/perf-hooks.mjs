/** Native process-relative clock facade; timeline/profiling APIs are not synthesized. */
export default function createPerformanceClock(host) {
  const performance = {};
  const origin = host.timeOrigin;
  const brand = Symbol('performance');
  const own = Object.hasOwn;
  Object.defineProperty(performance, brand, {
    value: true,
    writable: true,
    enumerable: true,
    configurable: true
  });

  function check(receiver) {
    // Match Node's own-brand test: transparent proxies work, inherited brands do not.
    if (receiver === null || typeof receiver !== 'object' || !own(receiver, brand)) {
      const error = new TypeError('Value of this must be the performance object');
      error.code = 'ERR_INVALID_THIS';
      throw error;
    }
  }

  Object.defineProperties(performance, {
    timeOrigin: {
      get() {
        check(this);
        return origin;
      },

      enumerable: true,
      configurable: true
    },
    now: {
      value: function now() {
        check(this);
        return host.now();
      },

      writable: true,
      enumerable: true,
      configurable: true
    }
  });
  return { performance };
}
