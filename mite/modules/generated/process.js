/* Generated from mite/modules/core/process.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/process-timing.mjs
function installProcessTiming(host, process, invalid) {
  const array = Array.isArray;
  const bigint = BigInt;
  const maximumCpu = Number.MAX_SAFE_INTEGER;
  function hrtime(previous) {
    const value = host.monotonicTime();
    if (previous === void 0) {
      return value;
    }
    if (!array(previous)) {
      throw invalid("ERR_INVALID_ARG_TYPE", "time must be a tuple Array");
    }
    if (previous.length !== 2) {
      throw invalid("ERR_OUT_OF_RANGE", "time must contain two values", RangeError);
    }
    const seconds = value[0] - previous[0];
    const nanos = value[1] - previous[1];
    return nanos < 0 ? [seconds - 1, nanos + 1e9] : [seconds, nanos];
  }
  hrtime.bigint = function bigintTime() {
    const value = host.monotonicTime();
    return bigint(value[0]) * 1000000000n + bigint(value[1]);
  };
  function cpuUsage(previous) {
    function validate(value2, name) {
      if (typeof value2 !== "number") {
        throw invalid("ERR_INVALID_ARG_TYPE", `${name} CPU time must be a number`);
      }
      if (!(value2 >= 0 && value2 <= maximumCpu)) {
        throw invalid(
          "ERR_INVALID_ARG_VALUE",
          `${name} CPU time is outside the supported range`,
          RangeError
        );
      }
    }
    if (previous) {
      validate(previous.user, "user");
      validate(previous.system, "system");
    }
    const value = host.cpuTime();
    const user = value[0];
    const system = value[1];
    return previous ? { user: user - previous.user, system: system - previous.system } : { user, system };
  }
  process.hrtime = hrtime;
  process.uptime = function uptime() {
    return host.processUptime();
  };
  process.cpuUsage = cpuUsage;
}

// mite/modules/core/process.mjs
function installProcessModule(host, process) {
  const own = Function.call.bind(Object.prototype.hasOwnProperty);
  function invalid(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    return error;
  }
  process.env = new Proxy(
    {},
    {
      get(target, key, receiver) {
        if (typeof key !== "string") {
          return Reflect.get(target, key, receiver);
        }
        const value = host.envGet(key);
        return value === void 0 ? Reflect.get(target, key, receiver) : value;
      },
      set(target, key, value) {
        if (typeof key !== "string") {
          throw new TypeError("Environment keys must be strings");
        }
        host.envSet(key, `${value}`);
        return true;
      },
      has(target, key) {
        return typeof key === "string" && host.envGet(key) !== void 0;
      },
      deleteProperty(target, key) {
        if (typeof key === "string") {
          host.envDelete(key);
        }
        return true;
      },
      ownKeys() {
        return host.envKeys();
      },
      getOwnPropertyDescriptor(target, key) {
        if (typeof key !== "string") {
          return void 0;
        }
        const value = host.envGet(key);
        if (value === void 0) {
          return void 0;
        }
        return { value, writable: true, enumerable: true, configurable: true };
      },
      defineProperty(target, key, descriptor) {
        if (typeof key !== "string" || !own(descriptor, "value") || !descriptor.writable || !descriptor.enumerable || !descriptor.configurable || own(descriptor, "get") || own(descriptor, "set")) {
          throw invalid(
            "ERR_INVALID_OBJECT_DEFINE_PROPERTY",
            "Environment properties must be configurable writable enumerable data properties"
          );
        }
        host.envSet(key, `${descriptor.value}`);
        return true;
      },
      preventExtensions() {
        return false;
      }
    }
  );
  let exitCode;
  Object.defineProperty(process, "exitCode", {
    configurable: true,
    enumerable: true,
    get() {
      return exitCode;
    },
    set(value) {
      if (value === void 0 || value === null) {
        exitCode = void 0;
        return;
      }
      if (typeof value === "string" && value !== "" && !Number.isNaN(+value)) {
        value = +value;
      }
      if (typeof value !== "number") {
        throw invalid("ERR_INVALID_ARG_TYPE", "Exit code must be a number or integer string");
      }
      if (!Number.isInteger(value)) {
        throw invalid("ERR_OUT_OF_RANGE", "Exit code must be an integer", RangeError);
      }
      exitCode = value | 0;
    }
  });
  process.exit = function exit(code) {
    if (arguments.length !== 0) {
      process.exitCode = code;
    }
    host.exit(process.exitCode === void 0 ? 0 : process.exitCode, process);
  };
  installProcessTiming(host, process, invalid);
  return process;
}
return installProcessModule;
})()
