/* Generated from mite/modules/core/process.mjs; edit the ESM source. */
(function () {
'use strict';
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
    if (code !== void 0) {
      process.exitCode = code;
    }
    host.exit(process.exitCode === void 0 ? 0 : process.exitCode);
  };
  return process;
}
return installProcessModule;
})()
