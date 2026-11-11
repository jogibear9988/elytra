/* Generated from mite/modules/core/util.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/promisify.mjs
function createPromisify(state) {
  const customSymbol = Symbol.for("nodejs.util.promisify.custom");
  const argumentNamesSymbol = state.argumentNames = Symbol("customPromisifyArgs");
  function promisify(original) {
    if (typeof original !== "function") {
      throw invalid("original must be a function");
    }
    if (original[customSymbol]) {
      const custom = original[customSymbol];
      if (typeof custom !== "function") {
        throw invalid("promisify.custom must be a function");
      }
      Object.defineProperty(custom, customSymbol, {
        value: custom,
        configurable: true,
        enumerable: false,
        writable: false
      });
      return custom;
    }
    const names = original[argumentNamesSymbol];
    function wrapped(...args) {
      return new Promise((resolve, reject) => {
        args.push((error, ...values) => {
          if (error) {
            reject(error);
          } else {
            resolve(
              names && values.length > 1 ? Object.fromEntries(names.map((name, index) => [name, values[index]])) : values[0]
            );
          }
        });
        Reflect.apply(original, this, args);
      });
    }
    Object.setPrototypeOf(wrapped, Object.getPrototypeOf(original));
    Object.defineProperty(wrapped, customSymbol, {
      value: wrapped,
      configurable: true,
      enumerable: false,
      writable: false
    });
    return Object.defineProperties(wrapped, Object.getOwnPropertyDescriptors(original));
  }
  function invalid(message) {
    return Object.assign(new TypeError(message), { code: "ERR_INVALID_ARG_TYPE" });
  }
  promisify.custom = customSymbol;
  return promisify;
}

// mite/modules/core/inherits.mjs
function inherits(ctor, superCtor) {
  function invalid(name) {
    throw Object.assign(new TypeError(`${name} is not a supported inheritance target`), {
      code: "ERR_INVALID_ARG_TYPE"
    });
  }
  if (ctor === null || ctor === void 0) {
    invalid("ctor");
  }
  if (superCtor === null || superCtor === void 0) {
    invalid("superCtor");
  }
  if (superCtor.prototype === void 0) {
    invalid("superCtor.prototype");
  }
  Object.defineProperty(ctor, "super_", {
    value: superCtor,
    writable: true,
    configurable: true
  });
  Object.setPrototypeOf(ctor.prototype, superCtor.prototype);
}

// mite/modules/core/console.mjs
function createConsoleModule(host) {
  const states = /* @__PURE__ */ new WeakMap();
  const apply = Reflect.apply;
  const descriptors = Object.getOwnPropertyDescriptors;
  const keys = Object.keys;
  const methods = ["log", "info", "debug", "warn", "error", "dir", "assert"];
  function failure(Type, code, message) {
    return Object.assign(new Type(message), { code });
  }
  function unsupported(name) {
    throw failure(Error, "ERR_MITE_UNSUPPORTED_API", "Console." + name + " is not implemented");
  }
  function state(value) {
    const found = states.get(value);
    if (!found) {
      throw failure(TypeError, "ERR_INVALID_THIS", "Invalid Console receiver");
    }
    return found;
  }
  function inspect(value, depth = 2, seen = /* @__PURE__ */ new Set()) {
    if (typeof value === "string") {
      return JSON.stringify(value);
    }
    if (typeof value === "bigint") {
      return String(value) + "n";
    }
    if (typeof value === "function") {
      return "[Function]";
    }
    if (value === null || typeof value !== "object") {
      return String(value);
    }
    if (host.types?.isProxy(value)) {
      unsupported("Proxy inspection");
    }
    if (seen.has(value)) {
      return "[Circular]";
    }
    if (depth < 0) {
      return Array.isArray(value) ? "[Array]" : "[Object]";
    }
    seen.add(value);
    try {
      const entries = descriptors(value);
      const names = keys(entries).filter((name) => entries[name].enumerable);
      if (names.length > 100) {
        throw failure(
          RangeError,
          "ERR_MITE_CONSOLE_LIMIT",
          "Console object exceeds 100 enumerable properties"
        );
      }
      const values = names.map((name) => {
        const entry = entries[name];
        const rendered = "value" in entry ? inspect(entry.value, depth - 1, seen) : "[Getter/Setter]";
        return Array.isArray(value) ? rendered : name + ": " + rendered;
      });
      return Array.isArray(value) ? "[ " + values.join(", ") + " ]" : "{ " + values.join(", ") + " }";
    } finally {
      seen.delete(value);
    }
  }
  function format(args) {
    if (!args.length) {
      return "";
    }
    if (typeof args[0] !== "string") {
      return args.map((value) => typeof value === "string" ? value : inspect(value)).join(" ");
    }
    let index = 1;
    const head = args[0].replace(/%[%sdifjoO]/g, (token) => {
      if (token === "%%") {
        return "%";
      }
      if (index >= args.length) {
        return token;
      }
      const value = args[index++];
      if (token === "%s") {
        return typeof value === "object" && value !== null ? inspect(value) : String(value);
      }
      if (token === "%d") {
        return typeof value === "bigint" ? String(value) + "n" : String(Number(value));
      }
      if (token === "%i") {
        return String(parseInt(value, 10));
      }
      if (token === "%f") {
        return String(parseFloat(value));
      }
      if (token === "%j") {
        return JSON.stringify(value);
      }
      return inspect(value);
    });
    return [
      head,
      ...args.slice(index).map((value) => typeof value === "string" ? value : inspect(value))
    ].join(" ");
  }
  function write(receiver, error, text) {
    const record = state(receiver);
    if (text.length > 1024 * 1024) {
      throw failure(RangeError, "ERR_MITE_CONSOLE_LIMIT", "Console output exceeds 1 MiB");
    }
    const stream = error ? record.stderr : record.stdout;
    if (record.ignoreErrors) {
      const ignore = () => {
      };
      let listening = false;
      try {
        if (typeof stream.once === "function") {
          stream.once("error", ignore);
          listening = true;
        }
        apply(stream.write, stream, [
          text + "\n",
          (failure2) => {
            if (failure2 && typeof stream.once === "function" && (typeof stream.listenerCount !== "function" || stream.listenerCount("error") === 0)) {
              stream.once("error", ignore);
            }
          }
        ]);
      } catch {
      } finally {
        if (listening && typeof stream.removeListener === "function") {
          stream.removeListener("error", ignore);
        }
      }
    } else {
      apply(stream.write, stream, [text + "\n"]);
    }
  }
  function Console(stdout, stderr, ignoreErrors = true) {
    if (!new.target) {
      return new Console(stdout, stderr, ignoreErrors);
    }
    if (stdout && typeof stdout === "object" && typeof stdout.write !== "function") {
      const options = stdout;
      for (const name of Reflect.ownKeys(options)) {
        if (!["stdout", "stderr", "ignoreErrors", "inspectOptions", "colorMode"].includes(name)) {
          unsupported("option " + String(name));
        }
      }
      stdout = options.stdout;
      stderr = options.stderr;
      ignoreErrors = options.ignoreErrors ?? true;
      const inspectOptions = options.inspectOptions;
      if (inspectOptions !== void 0) {
        if (!inspectOptions || typeof inspectOptions !== "object" || Array.isArray(inspectOptions)) {
          throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "inspectOptions must be an object");
        }
        for (const name of Reflect.ownKeys(inspectOptions)) {
          if (name !== "colors" || inspectOptions[name] !== false) {
            unsupported("inspectOptions");
          }
        }
      }
      if (options.colorMode !== void 0 && options.colorMode !== false) {
        unsupported("colorMode");
      }
    }
    if (!stdout || typeof stdout.write !== "function" || stderr !== void 0 && (!stderr || typeof stderr.write !== "function")) {
      throw failure(
        TypeError,
        "ERR_CONSOLE_WRITABLE_STREAM",
        "Console requires writable stdout/stderr"
      );
    }
    if (typeof ignoreErrors !== "boolean") {
      throw failure(TypeError, "ERR_INVALID_ARG_TYPE", "ignoreErrors must be boolean");
    }
    states.set(this, { stdout, stderr: stderr ?? stdout, ignoreErrors });
    for (const method of methods) {
      this[method] = Console.prototype[method].bind(this);
    }
  }
  for (const name of ["log", "info", "debug"]) {
    Console.prototype[name] = function(...args) {
      write(this, false, format(args));
    };
  }
  for (const name of ["warn", "error"]) {
    Console.prototype[name] = function(...args) {
      write(this, true, format(args));
    };
  }
  Console.prototype.dir = function(value, options) {
    if (options !== void 0) {
      unsupported("dir options");
    }
    write(this, false, inspect(value));
  };
  Console.prototype.assert = function(value, ...args) {
    state(this);
    if (!value) {
      write(this, true, "Assertion failed" + (args.length ? ": " + format(args) : ""));
    }
  };
  for (const name of [
    "table",
    "time",
    "timeEnd",
    "timeLog",
    "count",
    "countReset",
    "group",
    "groupCollapsed",
    "groupEnd",
    "clear",
    "trace",
    "dirxml",
    "profile",
    "profileEnd",
    "timeStamp"
  ]) {
    Console.prototype[name] = function() {
      state(this);
      unsupported(name);
    };
  }
  const instance = new Console(host.process.stdout, host.process.stderr);
  const output = host.console ?? {};
  for (const name of [
    ...methods,
    "table",
    "time",
    "timeEnd",
    "timeLog",
    "count",
    "countReset",
    "group",
    "groupCollapsed",
    "groupEnd",
    "clear",
    "trace",
    "dirxml",
    "profile",
    "profileEnd",
    "timeStamp"
  ]) {
    if (!(name in output)) {
      output[name] = instance[name].bind(instance);
    }
  }
  output.Console = Console;
  return output;
}

// mite/modules/core/debuglog.mjs
/*!
Copyright Joyent, Inc. and other Node contributors.

Permission is hereby granted, free of charge, to any person obtaining a
copy of this software and associated documentation files (the
"Software"), to deal in the Software without restriction, including
without limitation the rights to use, copy, modify, merge, publish,
distribute, sublicense, and/or sell copies of the Software, and to permit
persons to whom the Software is furnished to do so, subject to the
following conditions:

The above copyright notice and this permission notice shall be included
in all copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS
OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN
NO EVENT SHALL THE AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM,
DAMAGES OR OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE
USE OR OTHER DEALINGS IN THE SOFTWARE.

*/
function createDebugLog(host) {
  const process = host.process;
  const implementations = /* @__PURE__ */ new Map();
  const debugEnv = process.env.NODE_DEBUG;
  const expression = debugEnv ? new RegExp(
    "^" + debugEnv.replace(/[|\\{}()[\]^$+?.]/g, "\\$&").replaceAll("*", ".*").replaceAll(",", "$|^") + "$",
    "i"
  ) : void 0;
  let output;
  const upper = Function.call.bind(String.prototype.toUpperCase);
  const apply = Reflect.apply;
  function implementation(enabled, category) {
    if (!implementations.has(category)) {
      let log = () => {
      };
      if (enabled) {
        if (category === "HTTP" || category === "HTTP2") {
          process.emitWarning(
            `Setting the NODE_DEBUG environment variable to '${category.toLowerCase()}' can expose sensitive data (such as passwords, tokens and authentication headers) in the resulting log.`
          );
        }
        log = (...args) => {
          if (!output) {
            const { Console } = createConsoleModule({ process, types: host.types, console: {} });
            output = new Console({
              stdout: process.stderr,
              stderr: process.stderr,
              ignoreErrors: false,
              colorMode: false
            });
          }
          const prefix = `${category} ${process.pid}: `;
          if (!args.length) {
            return output.error(prefix);
          }
          if (typeof args[0] === "string") {
            return output.error(prefix + args[0], ...args.slice(1));
          }
          return output.error(prefix + "%O", ...args);
        };
      }
      implementations.set(category, log);
    }
    return implementations.get(category);
  }
  function debuglog(category, callback) {
    let enabled;
    const initialize = () => {
      category = upper(category);
      enabled = expression !== void 0 && expression.test(category);
    };
    let debug = (...args) => {
      initialize();
      debug = implementation(enabled, category);
      if (typeof callback === "function") {
        Object.defineProperty(debug, "enabled", {
          get: () => enabled,
          configurable: true,
          enumerable: true
        });
        callback(debug);
      }
      return apply(debug, void 0, args);
    };
    let test = () => {
      initialize();
      test = () => enabled;
      return enabled;
    };
    const logger = (...args) => {
      if (enabled === false) {
        return;
      }
      return apply(debug, void 0, args);
    };
    Object.defineProperty(logger, "enabled", {
      get: () => test(),
      configurable: true,
      enumerable: true
    });
    return logger;
  }
  return debuglog;
}

// mite/modules/core/util.mjs
function createEncodingModule(host) {
  const native = host.native;
  const debuglog = createDebugLog(host);
  const promisify = createPromisify(host.promisifyState);
  const encoders = /* @__PURE__ */ new WeakSet();
  const decoders = /* @__PURE__ */ new WeakMap();
  function fail(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    throw error;
  }
  function options(value) {
    if (value == null) {
      return {};
    }
    if (typeof value !== "object") {
      fail("ERR_INVALID_ARG_TYPE", "options must be an object");
    }
    return value;
  }
  function encoder(value) {
    if (!encoders.has(value)) {
      fail("ERR_INVALID_THIS", "Invalid TextEncoder receiver");
    }
  }
  function decoder(value) {
    const state = decoders.get(value);
    if (!state) {
      fail("ERR_INVALID_THIS", "Invalid TextDecoder receiver");
    }
    return state;
  }
  class TextEncoder {
    constructor() {
      encoders.add(this);
    }
    get encoding() {
      encoder(this);
      return "utf-8";
    }
    encode(input = "") {
      encoder(this);
      return native.encode(`${input}`.toWellFormed());
    }
    encodeInto(source, destination) {
      encoder(this);
      if (typeof source !== "string") {
        fail("ERR_INVALID_ARG_TYPE", "source must be a string");
      }
      return native.encodeInto(source, destination);
    }
  }
  class TextDecoder {
    constructor(label = "utf-8", opts = void 0) {
      label = `${label}`;
      opts = options(opts);
      const encoding = native.standardize(label)?.toLowerCase();
      if (!encoding || encoding === "replacement") {
        fail("ERR_ENCODING_NOT_SUPPORTED", `The encoding ${label} is not supported`, RangeError);
      }
      const fatal = !!opts.fatal;
      const ignoreBOM = !!opts.ignoreBOM;
      decoders.set(this, {
        encoding,
        fatal,
        ignoreBOM,
        decode: native.createDecoder(encoding, fatal, ignoreBOM)
      });
    }
    get encoding() {
      return decoder(this).encoding;
    }
    get fatal() {
      return decoder(this).fatal;
    }
    get ignoreBOM() {
      return decoder(this).ignoreBOM;
    }
    decode(input = void 0, opts = void 0) {
      const state = decoder(this);
      return state.decode(input, !!options(opts).stream);
    }
  }
  for (const [constructor, name] of [
    [TextEncoder, "TextEncoder"],
    [TextDecoder, "TextDecoder"]
  ]) {
    Object.defineProperty(constructor.prototype, Symbol.toStringTag, {
      value: name,
      configurable: true
    });
    for (const property of Object.getOwnPropertyNames(constructor.prototype)) {
      if (property !== "constructor") {
        Object.defineProperty(constructor.prototype, property, {
          ...Object.getOwnPropertyDescriptor(constructor.prototype, property),
          enumerable: true
        });
      }
    }
  }
  for (const name of ["setTimeout", "setImmediate"]) {
    Object.defineProperty(host.timers[name], promisify.custom, {
      get: () => host.timerPromises[name],
      enumerable: true
    });
  }
  return {
    TextEncoder,
    TextDecoder,
    promisify,
    inherits,
    debuglog,
    debug: debuglog,
    ...host.types ? { types: host.types } : {}
  };
}
return createEncodingModule;
})()
