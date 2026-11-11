/* Generated from mite/modules/core/process-events.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/process-warnings.mjs
function installWarnings(process) {
  const capture = Error.captureStackTrace;
  const tick = process.nextTick.bind(process);
  let pending = 0;
  let pendingCharacters = 0;
  function invalid(message) {
    throw Object.assign(new TypeError(message), { code: "ERR_INVALID_ARG_TYPE" });
  }
  function text(value, name) {
    if (typeof value !== "string") {
      invalid(`${name} must be a string`);
    }
    if (value.length > 65536) {
      throw Object.assign(new RangeError("Warning text exceeds 65536 code units"), {
        code: "ERR_MITE_WARNING_LIMIT"
      });
    }
    return value;
  }
  function onWarning(warning) {
    if (!(warning instanceof Error)) {
      return;
    }
    const deprecation = warning.name === "DeprecationWarning";
    if (deprecation && process.noDeprecation) {
      return;
    }
    const trace = process.traceProcessWarnings || deprecation && process.traceDeprecation;
    const body = trace && warning.stack ? String(warning.stack) : String(warning);
    const detail = typeof warning.detail === "string" ? `
${warning.detail}` : "";
    process.stderr.write(
      `(${process.release.name}:${process.pid}) ${warning.code ? `[${warning.code}] ` : ""}${body}${detail}
`
    );
  }
  function emitWarning(warning, type, code, ctor) {
    if (process.noDeprecation && type === "DeprecationWarning") {
      return;
    }
    let detail;
    if (type !== null && typeof type === "object" && !Array.isArray(type)) {
      for (const key of Object.keys(type)) {
        if (!["type", "code", "detail", "ctor"].includes(key)) {
          throw Object.assign(new Error(`Warning option ${key} is not implemented`), {
            code: "ERR_MITE_UNSUPPORTED_API"
          });
        }
      }
      const options = type;
      ctor = options.ctor;
      code = options.code;
      detail = typeof options.detail === "string" ? text(options.detail, "detail") : void 0;
      type = options.type || "Warning";
    } else if (typeof type === "function") {
      ctor = type;
      code = void 0;
      type = "Warning";
    }
    if (type !== void 0) {
      text(type, "type");
    }
    if (typeof code === "function") {
      ctor = code;
      code = void 0;
    } else if (code !== void 0) {
      text(code, "code");
    }
    if (ctor !== void 0 && typeof ctor !== "function") {
      invalid("ctor must be a function");
    }
    let characters;
    if (typeof warning === "string") {
      const message = text(warning, "warning");
      warning = new Error(message);
      warning.name = type || "Warning";
      if (code !== void 0) {
        warning.code = code;
      }
      if (detail !== void 0) {
        warning.detail = detail;
      }
      capture(warning, ctor || emitWarning);
      characters = message.length + warning.name.length + (code?.length || 0) + (detail?.length || 0);
    } else if (warning instanceof Error) {
      characters = text(String(warning.message), "message").length;
    } else {
      invalid("warning must be a string or Error");
    }
    if (warning.name === "DeprecationWarning" && process.noDeprecation) {
      return;
    }
    if (pending >= 256 || pendingCharacters + characters > 1024 * 1024) {
      throw Object.assign(new RangeError("Pending warning budget exceeded"), {
        code: "ERR_MITE_WARNING_LIMIT"
      });
    }
    pending++;
    pendingCharacters += characters;
    const shouldThrow = warning.name === "DeprecationWarning" && process.throwDeprecation;
    tick(() => {
      pending--;
      pendingCharacters -= characters;
      if (shouldThrow) {
        throw warning;
      }
      process.emit("warning", warning);
    });
  }
  Object.defineProperty(process, "emitWarning", {
    value: emitWarning,
    writable: true,
    configurable: true,
    enumerable: true
  });
  process.on("warning", onWarning);
}

// mite/modules/core/process-events.mjs
function installProcessEvents(host, process) {
  const { EventEmitter, signals, native } = host;
  const apply = Reflect.apply;
  const bind = Function.call.bind(Function.prototype.bind);
  const base = EventEmitter.prototype;
  const listeners = base.listenerCount;
  const emit = base.emit;
  const active = /* @__PURE__ */ new Map();
  const reserved = /* @__PURE__ */ new Set([
    "beforeExit",
    "exit",
    "uncaughtException",
    "uncaughtExceptionMonitor",
    "unhandledRejection",
    "rejectionHandled",
    "message",
    "disconnect",
    "multipleResolves",
    "worker",
    "workerMessage"
  ]);
  if (host.childIpc) {
    reserved.delete("message");
    reserved.delete("disconnect");
  }
  if (host.exitEvents === true) {
    reserved.delete("exit");
  }
  function isSignal(type) {
    return typeof type === "string" && Object.hasOwn(signals, type);
  }
  function reconcile() {
    for (const [type, number] of active) {
      if (apply(listeners, process, [type]) === 0) {
        native.stop(number);
        active.delete(type);
      }
    }
  }
  function adding(method) {
    return function(type, listener) {
      if (this !== process || typeof listener !== "function") {
        return apply(method, this, arguments);
      }
      if (reserved.has(type)) {
        const error = new Error(`Process event is not implemented: ${type}`);
        error.code = "ERR_MITE_UNSUPPORTED_PROCESS_EVENT";
        throw error;
      }
      if (isSignal(type) && !active.has(type)) {
        const number = signals[type];
        native.start(number, bind(process.emit, process, type, type));
        active.set(type, number);
      }
      try {
        return apply(method, this, arguments);
      } finally {
        reconcile();
      }
    };
  }
  function removing(method) {
    return function() {
      try {
        return apply(method, this, arguments);
      } finally {
        if (this === process) {
          reconcile();
        }
      }
    };
  }
  const prototype = Object.create(base);
  const on = adding(base.on);
  const remove = removing(base.removeListener);
  Object.defineProperties(prototype, {
    on: { value: on, configurable: true, writable: true },
    addListener: { value: on, configurable: true, writable: true },
    prependListener: { value: adding(base.prependListener), configurable: true, writable: true },
    removeListener: { value: remove, configurable: true, writable: true },
    off: { value: remove, configurable: true, writable: true },
    removeAllListeners: {
      value: removing(base.removeAllListeners),
      configurable: true,
      writable: true
    }
  });
  Object.setPrototypeOf(process, prototype);
  EventEmitter.call(process);
  if (typeof host.installErrorHandler === "function") {
    host.installErrorHandler((type, value, auxiliary) => {
      if (type === "uncaughtException") {
        apply(emit, process, ["uncaughtExceptionMonitor", value, auxiliary]);
        return apply(emit, process, [type, value, auxiliary]);
      }
      if (type === "unhandledRejection") {
        return apply(emit, process, [type, value, auxiliary]);
      }
      if (type === "rejectionHandled") {
        return apply(emit, process, [type, value]);
      }
      throw new Error("Invalid native process error event");
    });
    for (const type of [
      "uncaughtException",
      "uncaughtExceptionMonitor",
      "unhandledRejection",
      "rejectionHandled"
    ]) {
      reserved.delete(type);
    }
  }
  installWarnings(process);
}
return installProcessEvents;
})()
