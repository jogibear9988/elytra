/* Generated from mite/modules/workers/index.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/workers/clone.mjs
function createClone(types) {
  const encoder = new TextEncoder();
  const decoder = new TextDecoder("utf-8", { fatal: true });
  const arrayBufferBytes = Object.getOwnPropertyDescriptor(ArrayBuffer.prototype, "byteLength").get;
  const typedPrototype = Object.getPrototypeOf(Uint8Array.prototype);
  const typedBuffer = Object.getOwnPropertyDescriptor(typedPrototype, "buffer").get;
  const typedOffset = Object.getOwnPropertyDescriptor(typedPrototype, "byteOffset").get;
  const typedBytes = Object.getOwnPropertyDescriptor(typedPrototype, "byteLength").get;
  const keys = Object.keys;
  const proto = Object.getPrototypeOf;
  const define = Object.defineProperty;
  function fail(message) {
    throw new DOMException(message, "DataCloneError");
  }
  function encode(value) {
    const seen = /* @__PURE__ */ new Map();
    const nodes = [];
    let properties = 0;
    let characters = 0;
    let binaryBytes = 0;
    function string(value2) {
      characters += value2.length;
      if (characters > 4 * 1024 * 1024) {
        fail("Worker message string budget exceeded");
      }
      return value2;
    }
    function visit(value2, depth) {
      if (depth > 128) {
        fail("Worker message nesting budget exceeded");
      }
      if (value2 === null || typeof value2 === "boolean") {
        return value2;
      }
      if (typeof value2 === "string") {
        return string(value2);
      }
      if (typeof value2 === "number") {
        if (Object.is(value2, -0)) {
          return ["n", "-0"];
        }
        return Number.isFinite(value2) ? value2 : ["n", String(value2)];
      }
      if (typeof value2 === "undefined") {
        return ["u"];
      }
      if (typeof value2 === "bigint") {
        return ["i", string(String(value2))];
      }
      if (typeof value2 !== "object" || types.isProxy(value2)) {
        fail("Value cannot be cloned");
      }
      if (types.isSharedArrayBuffer(value2)) {
        fail("Shared memory is not supported by workers");
      }
      if (seen.has(value2)) {
        return ["r", seen.get(value2)];
      }
      if (nodes.length >= 65536) {
        fail("Worker message node budget exceeded");
      }
      const id = nodes.length;
      seen.set(value2, id);
      nodes.push(null);
      if (types.isArrayBuffer(value2) || types.isUint8Array(value2)) {
        if (types.isUint8Array(value2)) {
          const backing = typedBuffer.call(value2);
          if (types.isSharedArrayBuffer(backing)) {
            fail("Shared worker buffers are unsupported");
          }
          const offset = typedOffset.call(value2);
          const length = typedBytes.call(value2);
          try {
            new Uint8Array(backing, offset, length);
          } catch {
            fail("Detached worker buffer");
          }
          nodes[id] = ["v", visit(backing, depth + 1), offset, length];
        } else {
          let bytes2;
          try {
            bytes2 = new Uint8Array(value2, 0, arrayBufferBytes.call(value2));
          } catch {
            fail("Detached worker message buffer");
          }
          binaryBytes += bytes2.byteLength;
          if (binaryBytes > 1024 * 1024) {
            fail("Worker binary snapshot exceeds 1 MiB");
          }
          nodes[id] = ["b", Array.from(bytes2)];
        }
      } else {
        const array = Array.isArray(value2);
        if (!array && proto(value2) !== Object.prototype && proto(value2) !== null) {
          fail("This worker clone supports plain objects, arrays and byte buffers");
        }
        const names = keys(value2);
        properties += names.length;
        if (properties > 131072 || array && value2.length > 65536) {
          fail("Worker message property budget exceeded");
        }
        const entries = names.map((name) => [string(name), visit(value2[name], depth + 1)]);
        nodes[id] = [array ? "a" : "o", entries, array ? value2.length : 0];
      }
      return ["r", id];
    }
    const root = visit(value, 0);
    const bytes = encoder.encode(JSON.stringify([1, root, nodes]));
    if (bytes.byteLength > 8 * 1024 * 1024) {
      fail("Worker message exceeds 8 MiB");
    }
    return bytes;
  }
  function decode(bytes) {
    if (bytes.byteLength > 8 * 1024 * 1024) {
      fail("Worker wire exceeds 8 MiB");
    }
    const [version, root, nodes] = JSON.parse(decoder.decode(bytes));
    if (version !== 1 || !Array.isArray(nodes) || nodes.length > 65536) {
      fail("Invalid worker wire");
    }
    let binaryBytes = 0;
    const objects = nodes.map((node) => {
      if (!Array.isArray(node)) {
        fail("Invalid worker node");
      }
      if (node[0] === "a") {
        if (!Number.isInteger(node[2]) || node[2] < 0 || node[2] > 65536) {
          fail("Invalid worker array");
        }
        return new Array(node[2]);
      }
      if (node[0] === "o") {
        return {};
      }
      if (node[0] === "v") {
        return null;
      }
      if (node[0] === "b") {
        if (!Array.isArray(node[1]) || node[1].length > 1024 * 1024 || node[1].some((n) => !Number.isInteger(n) || n < 0 || n > 255)) {
          fail("Invalid worker bytes");
        }
        binaryBytes += node[1].length;
        if (binaryBytes > 1024 * 1024) {
          fail("Worker binary snapshot exceeds 1 MiB");
        }
        const result = new Uint8Array(node[1]);
        return result.buffer;
      }
      fail("Invalid worker type");
    });
    function unpack(value) {
      if (!Array.isArray(value)) {
        if (value === null || ["boolean", "number", "string"].includes(typeof value)) {
          return value;
        }
        fail("Invalid worker scalar");
      }
      if (value[0] === "r" && value.length === 2 && Number.isInteger(value[1]) && value[1] >= 0 && value[1] < objects.length) {
        return objects[value[1]];
      }
      if (value[0] === "u" && value.length === 1) {
        return void 0;
      }
      if (value[0] === "i" && value.length === 2 && typeof value[1] === "string") {
        return BigInt(value[1]);
      }
      if (value[0] === "n" && ["-0", "NaN", "Infinity", "-Infinity"].includes(value[1])) {
        return Number(value[1]);
      }
      fail("Invalid worker reference");
    }
    for (let id = 0; id < nodes.length; id++) {
      const node = nodes[id];
      if (node[0] !== "v") {
        continue;
      }
      const backing = unpack(node[1]);
      if (!types.isArrayBuffer(backing) || !Number.isInteger(node[2]) || !Number.isInteger(node[3]) || node[2] < 0 || node[3] < 0) {
        fail("Invalid worker view");
      }
      try {
        objects[id] = new Uint8Array(backing, node[2], node[3]);
      } catch {
        fail("Invalid worker view bounds");
      }
    }
    let properties = 0;
    for (let id = 0; id < nodes.length; id++) {
      if (nodes[id][0] !== "a" && nodes[id][0] !== "o") {
        continue;
      }
      if (!Array.isArray(nodes[id][1])) {
        fail("Invalid worker properties");
      }
      for (const pair of nodes[id][1]) {
        if (++properties > 131072 || !Array.isArray(pair) || pair.length !== 2 || typeof pair[0] !== "string" || nodes[id][0] === "a" && pair[0] === "length" || Object.hasOwn(objects[id], pair[0])) {
          fail("Invalid worker property");
        }
        define(objects[id], pair[0], {
          value: unpack(pair[1]),
          enumerable: true,
          configurable: true,
          writable: true
        });
      }
    }
    return unpack(root);
  }
  return { encode, decode };
}

// mite/modules/workers/index.mjs
function createWorkers(host) {
  const { native, events, types, path, fileURLToPath, URL, process } = host;
  const { EventEmitter } = events;
  const tick = process.nextTick.bind(process);
  const clone = createClone(types);
  const workers = /* @__PURE__ */ new WeakMap();
  const ports = /* @__PURE__ */ new WeakMap();
  const secret = {};
  function unsupported(name) {
    throw Object.assign(new Error(`${name} is not implemented`), {
      code: "ERR_MITE_UNSUPPORTED_API"
    });
  }
  function invalid(message) {
    throw Object.assign(new TypeError(message), { code: "ERR_INVALID_ARG_TYPE" });
  }
  function transfer(value) {
    if (value === void 0) {
      return;
    }
    if (!Array.isArray(value)) {
      invalid("transferList must be an array");
    }
    if (value.length) {
      unsupported("Worker message transfer lists");
    }
  }
  function status(result) {
    if (result !== 0) {
      throw Object.assign(new Error("Worker channel cannot admit this message"), {
        code: result === native.queueFull ? "ERR_WORKER_MESSAGING_FAILED" : "ERR_WORKER_NOT_RUNNING"
      });
    }
  }
  function workerState(value) {
    const state = workers.get(value);
    if (!state) {
      invalid("Invalid Worker receiver");
    }
    return state;
  }
  function portState(value) {
    const state = ports.get(value);
    if (!state) {
      invalid("Invalid MessagePort receiver");
    }
    return state;
  }
  class MessagePort extends EventEmitter {
    constructor(token) {
      super();
      if (token !== secret) {
        throw new TypeError("Illegal constructor");
      }
      const state = {
        closed: false,
        queue: [],
        bytes: 0,
        started: false,
        scheduled: false,
        explicitRef: null
      };
      ports.set(this, state);
      const updateRef = () => {
        if (!state.closed) {
          native.parentRef(state.explicitRef ?? this.listenerCount("message") > 0);
        }
      };
      const pump = () => {
        if (state.scheduled || !state.started) {
          return;
        }
        state.scheduled = true;
        tick(() => {
          state.scheduled = false;
          while (!state.closed && state.queue.length && this.listenerCount("message")) {
            const wire = state.queue.shift();
            state.bytes -= wire.byteLength;
            this.emit("message", clone.decode(wire));
          }
        });
      };
      this.on("newListener", (event) => {
        if (event === "message") {
          state.started = true;
          tick(() => {
            updateRef();
            pump();
          });
        }
      });
      this.on("removeListener", (event) => {
        if (event === "message") {
          updateRef();
        }
      });
      state.pump = pump;
      state.updateRef = updateRef;
      native.parentSubscribe((kind, wire) => {
        if (state.closed) {
          return;
        }
        if (kind === "message") {
          if (state.queue.length >= 64 || state.bytes + wire.byteLength > 16 * 1024 * 1024) {
            this.close();
            this.emit(
              "messageerror",
              Object.assign(new Error("Worker receive queue is full"), {
                code: "ERR_WORKER_MESSAGING_FAILED"
              })
            );
            return;
          }
          state.queue.push(wire);
          state.bytes += wire.byteLength;
          pump();
        } else if (kind === "close") {
          state.closed = true;
          state.queue.length = 0;
          state.bytes = 0;
          this.emit("close");
        }
      });
      native.parentRef(false);
    }
    postMessage(value, transferList) {
      const state = portState(this);
      if (state.closed) {
        return;
      }
      transfer(transferList);
      status(native.parentPost(clone.encode(value)));
    }
    start() {
      const state = portState(this);
      state.started = true;
      state.pump();
    }
    ref() {
      const state = portState(this);
      state.explicitRef = true;
      state.updateRef();
      return this;
    }
    unref() {
      const state = portState(this);
      state.explicitRef = false;
      state.updateRef();
      return this;
    }
    hasRef() {
      const state = portState(this);
      return !state.closed && (state.explicitRef ?? this.listenerCount("message") > 0);
    }
    close() {
      const state = portState(this);
      if (state.closed) {
        return;
      }
      state.closed = true;
      state.queue.length = 0;
      state.bytes = 0;
      native.parentClose();
      tick(() => this.emit("close"));
    }
  }
  class Worker extends EventEmitter {
    constructor(filename, options = {}) {
      super();
      if (typeof filename !== "string") {
        if (filename instanceof URL) {
          filename = fileURLToPath(filename);
        } else {
          invalid("Worker filename must be a path or file URL");
        }
      }
      if (!filename || filename.includes("\0") || /[\uD800-\uDFFF]/u.test(filename.replace(/[\uD800-\uDBFF][\uDC00-\uDFFF]/gu, ""))) {
        invalid("Worker filename must be valid Unicode without NUL");
      }
      if (!options || typeof options !== "object" || Array.isArray(options)) {
        invalid("Worker options must be an object");
      }
      let name = "";
      for (const key of Object.keys(options)) {
        const value = options[key];
        if (key === "name") {
          if (typeof value !== "string") {
            invalid("Worker name must be a string");
          }
          if (value.includes("\0") || value.length > 256) {
            invalid("Invalid Worker name");
          }
          name = value;
        } else if (key === "execArgv" && Array.isArray(value) && value.length === 0) {
        } else {
          unsupported(`Worker option ${key}`);
        }
      }
      if (!path.isAbsolute(filename) && !filename.startsWith("./") && !filename.startsWith("../")) {
        throw Object.assign(
          new TypeError("Worker filename must be absolute or start with ./ or ../"),
          { code: "ERR_WORKER_PATH" }
        );
      }
      const state = {
        id: 0,
        exited: false,
        code: void 0,
        resolve: null,
        termination: null,
        name
      };
      workers.set(this, state);
      state.id = native.create(path.resolve(filename), name, (kind, value) => {
        if (state.exited) {
          return;
        }
        if (kind === "message") {
          this.emit("message", clone.decode(value));
        } else if (kind === "error") {
          const error = new Error(value.message);
          if (typeof value.name === "string") {
            error.name = value.name;
          }
          if (typeof value.code === "string") {
            error.code = value.code;
          }
          if (typeof value.stack === "string") {
            error.stack = value.stack;
          }
          this.emit("error", error);
        } else if (kind === "online") {
          this.emit("online");
        } else if (kind === "exit") {
          state.exited = true;
          state.code = value;
          if (state.resolve) {
            state.resolve(value);
          }
          this.emit("exit", value);
        }
      });
    }
    postMessage(value, transferList) {
      const state = workerState(this);
      if (state.exited) {
        return;
      }
      transfer(transferList);
      status(native.post(state.id, clone.encode(value)));
    }
    ref() {
      const state = workerState(this);
      if (!state.exited) {
        native.ref(state.id, true);
      }
      return this;
    }
    unref() {
      const state = workerState(this);
      if (!state.exited) {
        native.ref(state.id, false);
      }
      return this;
    }
    terminate() {
      const state = workerState(this);
      if (state.exited) {
        return Promise.resolve(state.code);
      }
      native.ref(state.id, true);
      if (!state.termination) {
        state.termination = new Promise((resolve) => {
          state.resolve = resolve;
        });
        native.terminate(state.id);
      }
      return state.termination;
    }
    get threadId() {
      const state = workerState(this);
      return state.exited ? -1 : state.id;
    }
    get threadName() {
      const state = workerState(this);
      return state.exited ? null : state.name;
    }
  }
  for (const name of ["stdin", "stdout", "stderr", "performance", "resourceLimits"]) {
    Object.defineProperty(Worker.prototype, name, {
      get() {
        workerState(this);
        return unsupported(`Worker.${name}`);
      }
    });
  }
  const parentPort = native.isMainThread ? null : new MessagePort(secret);
  return {
    Worker,
    MessagePort,
    isMainThread: native.isMainThread,
    isInternalThread: false,
    parentPort,
    workerData: native.isMainThread ? null : void 0,
    threadId: native.threadId,
    threadName: native.isMainThread ? "main" : native.name
  };
}
return createWorkers;
})()
