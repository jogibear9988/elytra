/** Node-style process timing over immutable native samples; no JS wall-clock fallback. */
export default function installProcessTiming(host, process, invalid) {
  const array = Array.isArray;
  const bigint = BigInt;
  const maximumCpu = Number.MAX_SAFE_INTEGER;

  function hrtime(previous) {
    const value = host.monotonicTime();
    if (previous === undefined) {
      return value;
    }
    if (!array(previous)) {
      throw invalid('ERR_INVALID_ARG_TYPE', 'time must be a tuple Array');
    }
    if (previous.length !== 2) {
      throw invalid('ERR_OUT_OF_RANGE', 'time must contain two values', RangeError);
    }
    // Preserve legacy arithmetic coercion and the single nanosecond borrow.
    const seconds = value[0] - previous[0];
    const nanos = value[1] - previous[1];
    return nanos < 0 ? [seconds - 1, nanos + 1e9] : [seconds, nanos];
  }

  hrtime.bigint = function bigintTime() {
    const value = host.monotonicTime();
    return bigint(value[0]) * 1000000000n + bigint(value[1]);
  };

  function cpuUsage(previous) {
    function validate(value, name) {
      if (typeof value !== 'number') {
        throw invalid('ERR_INVALID_ARG_TYPE', `${name} CPU time must be a number`);
      }
      if (!(value >= 0 && value <= maximumCpu)) {
        throw invalid(
          'ERR_INVALID_ARG_VALUE',
          `${name} CPU time is outside the supported range`,
          RangeError
        );
      }
    }

    if (previous) {
      validate(previous.user, 'user');
      validate(previous.system, 'system');
    }
    const value = host.cpuTime();
    const user = value[0];
    const system = value[1];
    return previous
      ? { user: user - previous.user, system: system - previous.system }
      : { user, system };
  }

  process.hrtime = hrtime;

  process.uptime = function uptime() {
    return host.processUptime();
  };

  process.cpuUsage = cpuUsage;
}
