/* Generated from mite/runtime/bootstrap/clone/structured-clone.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/runtime/bootstrap/clone/structured-clone.mjs
function createStructuredClone(host) {
  const apply = Reflect.apply;
  const keys = Object.keys;
  const descriptor = Object.getOwnPropertyDescriptor;
  const prototype = Object.getPrototypeOf;
  const define = Object.defineProperty;
  const array = Array.isArray;
  const iteratorSymbol = Symbol.iterator;
  const regexpPrototype = RegExp.prototype;
  const ArrayCtor = Array;
  const ObjectCtor = Object;
  const MapCtor = Map;
  const SetCtor = Set;
  const DateCtor = Date;
  const RegExpCtor = RegExp;
  const BufferCtor = ArrayBuffer;
  const BytesCtor = Uint8Array;
  const ViewCtor = DataView;
  const TypeErrorCtor = TypeError;
  const ErrorCtor = Error;
  const Exception = host.DOMException;
  const {
    isProxy,
    isSharedArrayBuffer,
    isArrayBuffer,
    isTypedArray,
    isDataView,
    isOrdinaryObject
  } = host.types;
  const mapHas = Map.prototype.has;
  const mapGet = Map.prototype.get;
  const mapSet = Map.prototype.set;
  const mapEach = Map.prototype.forEach;
  const mapSize = descriptor(Map.prototype, "size").get;
  const setEach = Set.prototype.forEach;
  const setAdd = Set.prototype.add;
  const setSize = descriptor(Set.prototype, "size").get;
  const dateTime = Date.prototype.getTime;
  const regexpSource = descriptor(RegExp.prototype, "source").get;
  const bigintString = BigInt.prototype.toString;
  const bufferSize = descriptor(ArrayBuffer.prototype, "byteLength").get;
  const resizable = descriptor(ArrayBuffer.prototype, "resizable")?.get;
  const typed = prototype(Uint8Array.prototype);
  const typedBuffer = descriptor(typed, "buffer").get;
  const typedOffset = descriptor(typed, "byteOffset").get;
  const typedLength = descriptor(typed, "length").get;
  const typedName = descriptor(typed, Symbol.toStringTag).get;
  const bytesSet = Uint8Array.prototype.set;
  const viewBuffer = descriptor(DataView.prototype, "buffer").get;
  const viewOffset = descriptor(DataView.prototype, "byteOffset").get;
  const viewLength = descriptor(DataView.prototype, "byteLength").get;
  const constructors = new MapCtor();
  for (const name of [
    "Int8Array",
    "Uint8Array",
    "Uint8ClampedArray",
    "Int16Array",
    "Uint16Array",
    "Int32Array",
    "Uint32Array",
    "Float16Array",
    "Float32Array",
    "Float64Array",
    "BigInt64Array",
    "BigUint64Array"
  ]) {
    if (typeof globalThis[name] === "function") {
      apply(mapSet, constructors, [name, globalThis[name]]);
    }
  }
  const flags = [];
  for (const [name, flag] of [
    ["hasIndices", "d"],
    ["global", "g"],
    ["ignoreCase", "i"],
    ["multiline", "m"],
    ["dotAll", "s"],
    ["unicode", "u"],
    ["unicodeSets", "v"],
    ["sticky", "y"]
  ]) {
    const getter = descriptor(RegExp.prototype, name)?.get;
    if (getter) {
      flags[flags.length] = [getter, flag];
    }
  }
  const unsupportedGetters = [];
  const unsupportedMethods = [];
  const brandArguments = ["x-mite-structured-clone-brand"];
  const unsupportedPrototypes = [];
  for (const [name, property] of [
    ["Blob", "size"],
    ["File", "name"],
    ["URL", "href"],
    ["URLSearchParams", "size"],
    ["AbortSignal", "aborted"],
    ["AbortController", "signal"],
    ["Request", "url"],
    ["Response", "status"],
    ["ReadableStream", "locked"],
    ["WritableStream", "locked"],
    ["TransformStream", "readable"]
  ]) {
    const ctor = globalThis[name];
    if (typeof ctor !== "function") {
      continue;
    }
    const base = ctor.prototype;
    unsupportedPrototypes[unsupportedPrototypes.length] = base;
    for (let p = base; p; p = prototype(p)) {
      const getter = descriptor(p, property)?.get;
      if (getter) {
        unsupportedGetters[unsupportedGetters.length] = getter;
        break;
      }
    }
  }
  for (const name of ["Headers", "FormData"]) {
    const ctor = globalThis[name];
    if (typeof ctor !== "function") {
      continue;
    }
    unsupportedPrototypes[unsupportedPrototypes.length] = ctor.prototype;
    const method = descriptor(ctor.prototype, "has")?.value;
    if (typeof method === "function") {
      unsupportedMethods[unsupportedMethods.length] = method;
    }
  }
  function fail(message) {
    throw new Exception(message, "DataCloneError");
  }
  function invalid(code, message) {
    const error = new TypeErrorCtor(message);
    define(error, "code", { value: code, configurable: true, writable: true, enumerable: true });
    throw error;
  }
  function brand(method, value, argumentsList = []) {
    try {
      return { value: apply(method, value, argumentsList) };
    } catch {
      return void 0;
    }
  }
  return function structuredClone(value, options = void 0) {
    if (arguments.length === 0) {
      invalid("ERR_MISSING_ARGS", "A value is required");
    }
    if (options != null && typeof options !== "object" && typeof options !== "function") {
      invalid("ERR_INVALID_ARG_TYPE", "options must be an object");
    }
    const transfer = options == null ? void 0 : options.transfer;
    if (transfer !== void 0) {
      const iteratorMethod = transfer == null ? void 0 : transfer[iteratorSymbol];
      if (transfer === null || typeof transfer !== "object" && typeof transfer !== "function" || typeof iteratorMethod !== "function") {
        invalid("ERR_INVALID_ARG_TYPE", "transfer must be an iterable object");
      }
      const iterator = apply(iteratorMethod, transfer, []);
      const next = iterator.next;
      const first = apply(next, iterator, []);
      if (first === null || typeof first !== "object") {
        invalid("ERR_INVALID_ARG_TYPE", "Invalid transfer iterator result");
      }
      if (!first.done) {
        const error = new ErrorCtor("structuredClone transfer is not implemented");
        define(error, "code", {
          value: "ERR_MITE_UNSUPPORTED_API",
          configurable: true,
          writable: true,
          enumerable: true
        });
        throw error;
      }
    }
    const seen = new MapCtor();
    let nodes = 0;
    let properties = 0;
    let characters = 0;
    let binary = 0;
    function string(value2) {
      characters += value2.length;
      if (characters > 4 * 1024 * 1024) {
        fail("Structured clone string budget exceeded");
      }
      return value2;
    }
    function property() {
      if (++properties > 131072) {
        fail("Structured clone property budget exceeded");
      }
    }
    function remember(source, target) {
      apply(mapSet, seen, [source, target]);
      return target;
    }
    function visit(source, depth) {
      if (depth > 128) {
        fail("Structured clone nesting budget exceeded");
      }
      if (typeof source === "string") {
        return string(source);
      }
      if (typeof source === "bigint") {
        string(apply(bigintString, source, []));
        return source;
      }
      if (source === null || typeof source === "undefined" || typeof source === "boolean" || typeof source === "number") {
        return source;
      }
      if (typeof source !== "object" || isProxy(source)) {
        fail("Value cannot be cloned");
      }
      if (isSharedArrayBuffer(source)) {
        fail("Shared memory cloning is not implemented");
      }
      if (apply(mapHas, seen, [source])) {
        return apply(mapGet, seen, [source]);
      }
      if (++nodes > 65536) {
        fail("Structured clone node budget exceeded");
      }
      if (isArrayBuffer(source)) {
        let result2;
        try {
          if (resizable && apply(resizable, source, [])) {
            fail("Resizable buffers are not implemented");
          }
          const length2 = apply(bufferSize, source, []);
          binary += length2;
          if (binary > 1024 * 1024) {
            fail("Structured clone binary budget exceeded");
          }
          const bytes = new BytesCtor(source, 0, length2);
          result2 = new BufferCtor(length2);
          apply(bytesSet, new BytesCtor(result2), [bytes]);
        } catch {
          fail("ArrayBuffer cannot be cloned");
        }
        return remember(source, result2);
      }
      if (isTypedArray(source) || isDataView(source)) {
        let backing;
        let offset;
        let length2;
        let ctor;
        try {
          if (isDataView(source)) {
            backing = apply(viewBuffer, source, []);
            offset = apply(viewOffset, source, []);
            length2 = apply(viewLength, source, []);
            ctor = ViewCtor;
          } else {
            backing = apply(typedBuffer, source, []);
            offset = apply(typedOffset, source, []);
            length2 = apply(typedLength, source, []);
            ctor = apply(mapGet, constructors, [apply(typedName, source, [])]);
          }
        } catch {
          fail("Detached view cannot be cloned");
        }
        if (!ctor) {
          fail("Typed array kind is unsupported");
        }
        return remember(source, new ctor(visit(backing, depth + 1), offset, length2));
      }
      const date = brand(dateTime, source);
      if (date) {
        return remember(source, new DateCtor(date.value));
      }
      const regexp = source === regexpPrototype ? void 0 : brand(regexpSource, source);
      if (regexp) {
        let encoded = "";
        for (let i = 0; i < flags.length; ++i) {
          if (apply(flags[i][0], source, [])) {
            encoded += flags[i][1];
          }
        }
        return remember(source, new RegExpCtor(string(regexp.value), encoded));
      }
      if (brand(mapSize, source)) {
        const result2 = remember(source, new MapCtor());
        apply(mapEach, source, [
          (v, k) => {
            property();
            property();
            apply(mapSet, result2, [visit(k, depth + 1), visit(v, depth + 1)]);
          }
        ]);
        return result2;
      }
      if (brand(setSize, source)) {
        const result2 = remember(source, new SetCtor());
        apply(setEach, source, [
          (v) => {
            property();
            apply(setAdd, result2, [visit(v, depth + 1)]);
          }
        ]);
        return result2;
      }
      const isArray = array(source);
      if (!isArray && !isOrdinaryObject(source)) {
        fail("Native or host value cannot be cloned");
      }
      if (!isArray) {
        for (let i = 0; i < unsupportedGetters.length; ++i) {
          if (brand(unsupportedGetters[i], source)) {
            fail("Host value cloning is not implemented");
          }
        }
        for (let i = 0; i < unsupportedMethods.length; ++i) {
          if (brand(unsupportedMethods[i], source, brandArguments)) {
            fail("Host value cloning is not implemented");
          }
        }
        let p = prototype(source);
        for (let n = 0; p; ++n) {
          if (n >= 128 || isProxy(p)) {
            fail("Unsupported object prototype chain");
          }
          for (let i = 0; i < unsupportedPrototypes.length; ++i) {
            if (p === unsupportedPrototypes[i]) {
              fail("Host value cloning is not implemented");
            }
          }
          p = prototype(p);
        }
      }
      const length = isArray ? source.length : 0;
      if (length > 65536) {
        fail("Structured clone array budget exceeded");
      }
      const result = remember(source, isArray ? new ArrayCtor(length) : new ObjectCtor());
      const names = keys(source);
      if (properties + names.length > 131072) {
        fail("Structured clone property budget exceeded");
      }
      for (let i = 0; i < names.length; ++i) {
        const name = names[i];
        const own = descriptor(source, name);
        if (!own?.enumerable) {
          continue;
        }
        property();
        string(name);
        define(result, name, {
          value: visit(source[name], depth + 1),
          enumerable: true,
          writable: true,
          configurable: true
        });
      }
      return result;
    }
    return visit(value, 0);
  };
}
return createStructuredClone;
})()
