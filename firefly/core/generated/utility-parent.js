/* Generated from firefly/core/utility-process/parent-port.mjs; edit the ESM source. */
(function () {
'use strict';
// firefly/core/message-ports/remote.mjs
function createRemoteLinks(hooks) {
  const links = /* @__PURE__ */ new Set();
  const shift = Array.prototype.shift;
  const apply2 = Reflect.apply;
  function descriptor(value) {
    return value && Number.isInteger(value.id) && value.id > 0 && value.id <= 2147483647 && Number.isInteger(value.generation) && value.generation > 0 && value.generation <= 2147483647;
  }
  function connect(transport) {
    const routes = /* @__PURE__ */ new Map();
    let disconnected = false;
    let flushing = false;
    function valid(values) {
      if (disconnected || !Array.isArray(values) || values.length > 16) {
        throw hooks.error("ERR_FIREFLY_PORT_TRANSFER", "Invalid native port attachment");
      }
      const ids = /* @__PURE__ */ new Set();
      for (const value of values) {
        if (!descriptor(value) || ids.has(value.id) || routes.has(value.id)) {
          throw hooks.error("ERR_FIREFLY_PORT_TRANSFER", "Duplicate or invalid native port route");
        }
        ids.add(value.id);
      }
    }
    function disconnect() {
      if (disconnected) {
        return;
      }
      disconnected = true;
      links.delete(link);
      const owned = [...routes.values()];
      routes.clear();
      for (const route of owned) {
        route.closed = true;
        route.acks.length = 0;
        hooks.close(route.record, false);
      }
    }
    function accepted(status) {
      if (!status) {
        return true;
      }
      if (transport.blocked(status)) {
        return false;
      }
      disconnect();
      return false;
    }
    function flush() {
      if (flushing || disconnected) {
        return;
      }
      flushing = true;
      try {
        for (const route of routes.values()) {
          if (route.closing) {
            if (accepted(transport.close(route.id, route.generation))) {
              route.closed = true;
              route.acks.length = 0;
              routes.delete(route.id);
            }
            continue;
          }
          while (route.acks.length && !disconnected) {
            if (!accepted(transport.consume(route.id, route.generation, route.acks[0]))) {
              break;
            }
            apply2(shift, route.acks, []);
          }
        }
      } finally {
        flushing = false;
      }
    }
    function bind(record, value) {
      const route = {
        id: value.id,
        generation: value.generation,
        record,
        acks: [],
        closing: false,
        closed: false,
        send(wire) {
          return !disconnected && !route.closing && accepted(transport.send(route.id, route.generation, wire));
        },
        requestClose() {
          if (!disconnected && !route.closed) {
            route.closing = true;
            route.acks.length = 0;
            flush();
          }
        }
      };
      record.remote = route;
      record.started = true;
      routes.set(route.id, route);
      return route;
    }
    const link = {
      /** Prepare entirely before native admission; failed writes leave every source owner attached. */
      prepare(transfer) {
        if (disconnected) {
          throw hooks.error("ERR_FIREFLY_PORT_CHANNEL", "Native port link is closed");
        }
        const moves = hooks.transfers(transfer, null, null);
        for (const move of moves) {
          if (move.record.peer?.remote) {
            throw hooks.error(
              "ERR_FIREFLY_UNSUPPORTED_API",
              "Forwarding a native-backed port is not implemented"
            );
          }
          if (move.record.queue.some((entry) => entry.ports?.length)) {
            throw hooks.error(
              "ERR_FIREFLY_UNSUPPORTED_API",
              "Queued native port attachments are not implemented"
            );
          }
        }
        function unchanged() {
          for (const move of moves) {
            if (move.owned.record !== move.record || move.owned.detached || move.record.closed || move.record.lease.generation !== move.generation) {
              throw hooks.error(
                "ERR_FIREFLY_PORT_TRANSFER",
                "Port ownership changed while preparing native transfer"
              );
            }
          }
        }
        unchanged();
        let committed = false;
        return {
          count: moves.length,
          commit(values) {
            valid(values);
            if (committed || values.length !== moves.length) {
              disconnect();
              throw hooks.error(
                "ERR_FIREFLY_PORT_TRANSFER",
                "Native port attachment count changed"
              );
            }
            unchanged();
            committed = true;
            for (let index = 0; index < moves.length; index++) {
              const move = moves[index];
              hooks.detach(move);
              bind(move.record, values[index]);
              hooks.pump(move.record);
            }
          }
        };
      },
      /** Incoming attachments become genuine native-backed utility endpoints before parent-event delivery. */
      adopt(values) {
        valid(values);
        hooks.capacity(values.length * 2);
        const owners = values.map(() => hooks.shell());
        valid(values);
        hooks.capacity(values.length * 2);
        const ports = [];
        const allocated = [];
        try {
          for (let index = 0; index < values.length; index++) {
            const local = hooks.endpoint();
            allocated.push(local);
            const remote = hooks.endpoint();
            allocated.push(remote);
            local.peer = remote;
            remote.peer = local;
            bind(remote, values[index]);
            ports.push(hooks.attach(local, owners[index]));
          }
        } catch (failure) {
          for (const record of allocated) {
            hooks.close(record, false);
          }
          disconnect();
          throw failure;
        }
        return ports;
      },
      receive(id, generation, wire) {
        const route = routes.get(id);
        if (!route || route.generation !== generation || route.closed || route.closing || disconnected) {
          return;
        }
        const size = hooks.size(wire);
        const ack = () => {
          if (!route.closed && !route.closing && !disconnected) {
            route.acks.push(size);
            flush();
          }
        };
        if (!hooks.deliver(route.record.peer, wire, ack)) {
          route.requestClose();
        }
      },
      closed(id, generation) {
        const route = routes.get(id);
        if (!route || route.generation !== generation) {
          return;
        }
        route.closed = true;
        route.acks.length = 0;
        routes.delete(id);
        hooks.close(route.record, false);
      },
      writable() {
        flush();
        if (!disconnected) {
          for (const route of routes.values()) {
            hooks.pump(route.record);
          }
        }
      },
      disconnect
    };
    links.add(link);
    return link;
  }
  function shutdown() {
    for (const link of [...links]) {
      link.disconnect();
    }
  }
  return { connect, shutdown };
}

// firefly/core/message-ports/index.mjs
function createMessageChannels(native, EventEmitter, error) {
  const states = /* @__PURE__ */ new WeakMap();
  const apply2 = Reflect.apply;
  const weakSet = WeakMap.prototype.set;
  const weakGet = WeakMap.prototype.get;
  const define2 = Object.defineProperty;
  const shift = Array.prototype.shift;
  const isArray = Array.isArray;
  const descriptor = Object.getOwnPropertyDescriptor;
  const Weak = WeakRef;
  const deref = Weak.prototype.deref;
  const Registry = FinalizationRegistry;
  const register = Registry.prototype.register;
  const unregister = Registry.prototype.unregister;
  const records = /* @__PURE__ */ new Map();
  const token = Symbol("MessagePortMain constructor");
  const schedule = setImmediate;
  const tick = process.nextTick;
  const byteLength = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Uint8Array.prototype),
    "byteLength"
  ).get;
  let sequence = 0;
  let totalBytes = 0;
  let stopped = false;
  function ownerOf(record) {
    return record.owner ? apply2(deref, record.owner, []) : void 0;
  }
  function append(array, value) {
    define2(array, `${array.length}`, {
      value,
      writable: true,
      enumerable: true,
      configurable: true
    });
  }
  function state(port) {
    const value = apply2(weakGet, states, [port]);
    if (!value) {
      throw error("ERR_INVALID_THIS", "Invalid MessagePortMain receiver");
    }
    return value;
  }
  function release(lease) {
    if (lease.released) {
      return;
    }
    lease.released = true;
    totalBytes -= lease.bytes;
    lease.bytes = 0;
    lease.count = 0;
    records.delete(lease.id);
  }
  const finalizers = new Registry(({ id, generation }) => {
    const entry = records.get(id);
    if (!entry || entry.lease.generation !== generation) {
      return;
    }
    const record = apply2(deref, entry.weak, []);
    if (record) {
      close(record, true);
    } else {
      release(entry.lease);
    }
  });
  function shell() {
    const port = new MessagePortMain(token);
    return { port, owner: new Weak(port), gcToken: {} };
  }
  function attach(record, staged) {
    apply2(weakSet, states, [staged.port, { record, detached: false }]);
    record.owner = staged.owner;
    record.gcToken = staged.gcToken;
    apply2(register, finalizers, [
      staged.port,
      { id: record.lease.id, generation: record.lease.generation },
      staged.gcToken
    ]);
    return staged.port;
  }
  function notify(record, retainedOwner) {
    if (stopped || record.notified) {
      return;
    }
    record.notified = true;
    (retainedOwner ?? ownerOf(record))?.emit("close");
  }
  function consume(record, entry, delivered = true) {
    record.lease.count--;
    record.lease.bytes -= entry.size;
    totalBytes -= entry.size;
    if (delivered) {
      entry.ack?.();
    }
  }
  function discard(record) {
    const pending = record.queue;
    record.queue = [];
    for (const entry of pending) {
      if (entry.close) {
        continue;
      }
      consume(record, entry, false);
      for (const port of entry.ports) {
        const owned = apply2(weakGet, states, [port]);
        if (owned?.record) {
          close(owned.record, true);
        }
      }
    }
  }
  function close(record, local) {
    if (record.closed) {
      return;
    }
    record.closed = true;
    record.remote?.requestClose();
    const peer = record.peer;
    record.peer = null;
    if (peer && peer.peer === record) {
      peer.peer = null;
    }
    if (record.gcToken) {
      apply2(unregister, finalizers, [record.gcToken]);
    }
    discard(record);
    release(record.lease);
    if (!stopped) {
      if (local) {
        const owner = ownerOf(record);
        tick(() => notify(record, owner));
      } else {
        notify(record);
      }
      if (peer && !peer.closed) {
        append(peer.queue, { close: true });
        pump(peer);
      }
    }
  }
  function pump(record) {
    if (stopped || record.closed || !record.started || record.scheduled || !record.queue.length) {
      return;
    }
    record.scheduled = true;
    const generation = record.lease.generation;
    schedule(() => {
      if (record.lease.generation !== generation) {
        return;
      }
      record.scheduled = false;
      if (stopped || record.closed || !record.started || !record.queue.length) {
        return;
      }
      if (record.remote) {
        const entry2 = record.queue[0];
        if (entry2.close) {
          apply2(shift, record.queue, []);
          close(record, false);
        } else if (record.remote.send(entry2.wire)) {
          apply2(shift, record.queue, []);
          consume(record, entry2);
          pump(record);
        }
        return;
      }
      const owner = ownerOf(record);
      if (!owner) {
        close(record, true);
        return;
      }
      const entry = apply2(shift, record.queue, []);
      if (entry.close) {
        close(record, false);
        return;
      }
      consume(record, entry);
      let data;
      try {
        data = native.decodeIpc(entry.wire);
      } catch (failure) {
        for (const port of entry.ports) {
          const owned = apply2(weakGet, states, [port]);
          if (owned?.record) {
            close(owned.record, true);
          }
        }
        close(record, true);
        throw failure;
      }
      try {
        owner.emit("message", { data, ports: entry.ports });
      } finally {
        if (record.lease.generation === generation) {
          pump(record);
        }
      }
    });
  }
  function sweep() {
    for (const entry of [...records.values()]) {
      const record = apply2(deref, entry.weak, []);
      if (!record) {
        release(entry.lease);
      } else if (!record.remote && !ownerOf(record)) {
        close(record, true);
      }
    }
  }
  function endpoint() {
    if (sequence === Number.MAX_SAFE_INTEGER) {
      throw error("ERR_FIREFLY_PORT_LIMIT", "Port identity space exhausted");
    }
    const lease = { id: ++sequence, generation: 1, bytes: 0, count: 0, released: false };
    const record = {
      lease,
      queue: [],
      peer: null,
      owner: null,
      gcToken: null,
      started: false,
      closed: false,
      notified: false,
      scheduled: false,
      remote: null
    };
    records.set(lease.id, { weak: new Weak(record), lease });
    return record;
  }
  function transfers(value, source, destination) {
    if (value === void 0) {
      return [];
    }
    if (!isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", "Port transfer list must be an array");
    }
    const length = value.length;
    if (!Number.isSafeInteger(length) || length < 0 || length > 16) {
      throw error("ERR_FIREFLY_PORT_LIMIT", "At most 16 ports may be transferred");
    }
    const moves = [];
    for (let index = 0; index < length; index++) {
      const itemDescriptor = descriptor(value, String(index));
      if (!itemDescriptor || !Object.hasOwn(itemDescriptor, "value")) {
        throw error("ERR_INVALID_ARG_TYPE", "Port transfer entries must be own data values");
      }
      const port = itemDescriptor.value;
      const owned = apply2(weakGet, states, [port]);
      if (!owned || owned.detached || !owned.record) {
        throw error("ERR_INVALID_ARG_TYPE", "Transfer entry is not an owned MessagePortMain");
      }
      const record = owned.record;
      if (record === source) {
        throw error("ERR_FIREFLY_PORT_TRANSFER", "Cannot transfer the source port");
      }
      if (record === destination || record.closed) {
        throw error(
          "ERR_FIREFLY_UNSUPPORTED_API",
          "Closed or receiving endpoint transfer is not implemented"
        );
      }
      let duplicate = false;
      for (let slot = 0; slot < moves.length; slot++) {
        if (moves[slot].record === record) {
          duplicate = true;
        }
      }
      if (duplicate) {
        throw error("ERR_FIREFLY_PORT_TRANSFER", "Duplicate port transfer");
      }
      if (record.lease.generation === Number.MAX_SAFE_INTEGER) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "Port ownership generation exhausted");
      }
      append(moves, { port, owned, record, generation: record.lease.generation });
    }
    return moves;
  }
  const remote = createRemoteLinks({
    error,
    transfers,
    shell,
    attach,
    endpoint,
    pump,
    close,
    size: (wire) => apply2(byteLength, wire, []),
    capacity(count) {
      sweep();
      if (stopped || records.size + count > 256) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "At most 256 live endpoints are supported");
      }
    },
    detach(move) {
      const target = move.record;
      apply2(unregister, finalizers, [target.gcToken]);
      move.owned.detached = true;
      move.owned.record = null;
      target.lease.generation++;
      target.owner = null;
      target.started = false;
      target.scheduled = false;
    },
    deliver(record, wire, ack) {
      if (stopped || !record || record.closed) {
        return false;
      }
      const size = apply2(byteLength, wire, []);
      if (record.lease.count >= 64 || record.lease.bytes + size > 1048576 || totalBytes + size > 4194304) {
        close(record, true);
        return false;
      }
      append(record.queue, { wire, size, ports: [], ack });
      record.lease.count++;
      record.lease.bytes += size;
      totalBytes += size;
      pump(record);
      return true;
    }
  });
  class MessagePortMain extends EventEmitter {
    constructor(key) {
      super();
      if (key !== token) {
        throw error("ERR_ILLEGAL_CONSTRUCTOR", "MessagePortMain cannot be constructed directly");
      }
    }
    postMessage(message, transfer) {
      const owned = state(this);
      const record = owned.record;
      if (stopped || owned.detached || !record || record.closed) {
        return;
      }
      const destination = record.peer;
      const generation = record.lease.generation;
      const moves = transfers(transfer, record, destination);
      if (destination?.remote && moves.length) {
        throw error(
          "ERR_FIREFLY_UNSUPPORTED_API",
          "Port attachments over a native port are not implemented"
        );
      }
      let wire;
      try {
        wire = native.encodeIpc(message);
      } catch {
        throw error(
          "ERR_FIREFLY_PORT_SERIALIZATION",
          "Port message requires the supported bounded data/binary graph"
        );
      }
      const size = apply2(byteLength, wire, []);
      if (destination && !destination.closed && (destination.lease.count >= 64 || destination.lease.bytes + size > 1048576 || totalBytes + size > 4194304)) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "Port queue budget exceeded");
      }
      const staged = [];
      for (let slot = 0; slot < moves.length; slot++) {
        append(staged, shell());
      }
      if (owned.record !== record || owned.detached || record.closed || record.lease.generation !== generation || record.peer !== destination) {
        throw error("ERR_FIREFLY_PORT_TRANSFER", "Source changed while preparing a message");
      }
      for (const move of moves) {
        if (move.owned.record !== move.record || move.owned.detached || move.record.closed || move.record.lease.generation !== move.generation) {
          throw error(
            "ERR_FIREFLY_PORT_TRANSFER",
            "Transfer ownership changed while preparing a message"
          );
        }
      }
      if (!destination || destination.closed) {
        if (moves.length) {
          throw error(
            "ERR_FIREFLY_UNSUPPORTED_API",
            "Transfer to a disconnected endpoint is not implemented"
          );
        }
        return;
      }
      if (destination.lease.count >= 64 || destination.lease.bytes + size > 1048576 || totalBytes + size > 4194304) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "Port queue budget exceeded");
      }
      const received = [];
      for (let index = 0; index < moves.length; index++) {
        const move = moves[index];
        const target = move.record;
        apply2(unregister, finalizers, [target.gcToken]);
        move.owned.detached = true;
        move.owned.record = null;
        target.lease.generation++;
        target.started = false;
        target.scheduled = false;
        append(received, attach(target, staged[index]));
      }
      append(destination.queue, { wire, size, ports: received });
      destination.lease.count++;
      destination.lease.bytes += size;
      totalBytes += size;
      pump(destination);
    }
    start() {
      const owned = state(this);
      if (!stopped && !owned.detached && owned.record && !owned.record.closed) {
        owned.record.started = true;
        pump(owned.record);
      }
    }
    close() {
      const owned = state(this);
      if (!owned.detached && owned.record) {
        close(owned.record, true);
      }
    }
  }
  class MessageChannelMain {
    constructor() {
      if (new.target !== MessageChannelMain) {
        throw error(
          "ERR_FIREFLY_UNSUPPORTED_API",
          "MessageChannelMain subclassing is not implemented"
        );
      }
      if (stopped) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application has exited");
      }
      const firstOwner = shell();
      const secondOwner = shell();
      if (stopped) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application exited during port construction");
      }
      sweep();
      if (records.size + 2 > 256) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "At most 256 live main endpoints are supported");
      }
      const first = endpoint();
      const second = endpoint();
      first.peer = second;
      second.peer = first;
      Object.defineProperties(this, {
        port1: { value: attach(first, firstOwner), enumerable: true },
        port2: { value: attach(second, secondOwner), enumerable: true }
      });
    }
  }
  function shutdown() {
    stopped = true;
    remote.shutdown();
    for (const entry of [...records.values()]) {
      const record = apply2(deref, entry.weak, []);
      if (record) {
        close(record, true);
      } else {
        release(entry.lease);
      }
    }
  }
  return { MessageChannelMain, shutdown, connect: remote.connect };
}

// firefly/core/utility-process/port-packets.mjs
var ByteArray = Uint8Array;
var apply = Reflect.apply;
var typed = Object.getPrototypeOf(ByteArray.prototype);
var getters = Object.getOwnPropertyDescriptors(typed);
var tag = getters[Symbol.toStringTag].get;
var define = Object.defineProperty;
var maxFrame = 128 * 1024;
function invalid() {
  throw Object.assign(new Error("Invalid utility port packet"), {
    code: "ERR_FIREFLY_UTILITY_CHANNEL"
  });
}
function decodePortPacket(value, expected) {
  let bytes;
  let size;
  let buffer;
  let baseOffset;
  try {
    if (apply(tag, value, []) !== "Uint8Array") {
      invalid();
    }
    size = apply(getters.byteLength.get, value, []);
    if (size < 16 || size > maxFrame) {
      invalid();
    }
    buffer = apply(getters.buffer.get, value, []);
    baseOffset = apply(getters.byteOffset.get, value, []);
    bytes = new ByteArray(buffer, baseOffset, size);
  } catch {
    invalid();
  }
  if (bytes[0] !== 70 || bytes[1] !== 85 || bytes[2] !== 80 || bytes[3] !== 49 || bytes[6] !== 0 || bytes[7] !== 0) {
    invalid();
  }
  const number = (offset2) => bytes[offset2] * 16777216 + bytes[offset2 + 1] * 65536 + bytes[offset2 + 2] * 256 + bytes[offset2 + 3];
  const positive = (value2) => value2 > 0 && value2 <= 2147483647;
  const kind = bytes[4];
  const count = bytes[5];
  const id = number(8);
  const generation = number(12);
  const required = expected === "port-transfer" ? 1 : expected === "port-data" ? 2 : expected === "port-close" ? 3 : 0;
  if (!required || kind !== required) {
    invalid();
  }
  const descriptors = [];
  let offset = 16;
  if (kind === 1) {
    if (!count || count > 16 || id || generation || size <= offset + count * 8) {
      invalid();
    }
    for (let index = 0; index < count; index++) {
      const portId = number(offset);
      const portGeneration = number(offset + 4);
      if (!positive(portId) || !positive(portGeneration)) {
        invalid();
      }
      for (let previous = 0; previous < index; previous++) {
        if (descriptors[previous].id === portId) {
          invalid();
        }
      }
      define(descriptors, `${index}`, {
        value: { id: portId, generation: portGeneration },
        writable: true,
        configurable: true,
        enumerable: true
      });
      offset += 8;
    }
  } else if (count || !positive(id) || !positive(generation) || (kind === 3 ? size !== 16 : size <= 16)) {
    invalid();
  }
  return {
    id,
    generation,
    descriptors,
    wire: new ByteArray(buffer, baseOffset + offset, size - offset)
  };
}

// firefly/core/utility-process/parent-port.mjs
function createParentPort(native, EventEmitter, codec) {
  const schedule = setImmediate;
  const queue = [];
  let queuedBytes = 0;
  let scheduled = false;
  let started = false;
  let closed = false;
  let retired = false;
  const channels = createMessageChannels(codec, EventEmitter, failure);
  const link = channels.connect({
    send: (id, generation, wire) => native.sendPort(id, generation, wire),
    close: (id, generation) => native.closePort(id, generation),
    consume: (id, generation, size) => native.consumePort(id, generation, size),
    blocked: (status) => typeof native.portQueueFull === "number" && status === native.portQueueFull
  });
  function failure(code, message) {
    return Object.assign(new Error(message), { code });
  }
  function abort(error) {
    if (retired) {
      return;
    }
    closed = true;
    retired = true;
    queue.length = 0;
    queuedBytes = 0;
    link.disconnect();
    native.close();
    schedule(() => port.emit("error", error));
  }
  function pump() {
    if (retired || scheduled || !started || !queue.length) {
      return;
    }
    scheduled = true;
    schedule(() => {
      scheduled = false;
      if (retired || !started || !queue.length) {
        return;
      }
      const entry = queue.shift();
      if (entry.close) {
        retired = true;
        port.emit("close");
        return;
      }
      queuedBytes -= entry.wire.byteLength;
      let data;
      try {
        data = codec.decodeIpc(entry.wire);
      } catch {
        abort(failure("ERR_FIREFLY_UTILITY_CHANNEL", "Invalid utility parent message"));
        return;
      }
      try {
        port.emit("message", { data, ports: entry.ports });
      } finally {
        pump();
      }
    });
  }
  class ParentPort extends EventEmitter {
    /** Registering a message listener starts delivery, including once/prependOnceListener. */
    on(name, listener) {
      super.on(name, listener);
      if (name === "message") {
        this.start();
      }
      return this;
    }
    addListener(name, listener) {
      super.addListener(name, listener);
      if (name === "message") {
        this.start();
      }
      return this;
    }
    prependListener(name, listener) {
      super.prependListener(name, listener);
      if (name === "message") {
        this.start();
      }
      return this;
    }
    /** Explicit start also permits delivery without listeners, matching MessagePort behavior. */
    start() {
      started = true;
      pump();
    }
    postMessage(message, transfer) {
      if (transfer !== void 0 && (!Array.isArray(transfer) || transfer.length)) {
        throw failure(
          "ERR_FIREFLY_UNSUPPORTED_API",
          "Utility native MessagePort transfer is not implemented"
        );
      }
      if (closed) {
        throw failure("ERR_FIREFLY_UTILITY_CLOSED", "Utility parent channel has closed");
      }
      const wire = codec.encodeIpc(message);
      const status = native.send(wire);
      if (status) {
        throw failure(
          "ERR_FIREFLY_UTILITY_CHANNEL",
          `Utility parent channel send failed (${status})`
        );
      }
    }
  }
  const port = new ParentPort();
  native.setHandler((kind, value) => {
    if (closed || retired) {
      return;
    }
    if (kind === "close") {
      closed = true;
      link.disconnect();
      queue.push({ close: true });
      pump();
    } else if (kind === "port-writable") {
      link.writable();
    } else if (kind === "port-data" || kind === "port-close") {
      try {
        const packet = decodePortPacket(value, kind);
        if (kind === "port-data") {
          link.receive(packet.id, packet.generation, packet.wire);
        } else {
          link.closed(packet.id, packet.generation);
        }
      } catch {
        abort(failure("ERR_FIREFLY_UTILITY_CHANNEL", "Invalid utility port frame"));
      }
    } else if (kind === "message" || kind === "port-transfer") {
      let ports = [];
      if (kind === "port-transfer") {
        try {
          const packet = decodePortPacket(value, kind);
          ports = link.adopt(packet.descriptors);
          value = packet.wire;
        } catch {
          abort(failure("ERR_FIREFLY_UTILITY_CHANNEL", "Invalid utility port transfer"));
          return;
        }
      }
      if (!(value instanceof Uint8Array)) {
        abort(failure("ERR_FIREFLY_UTILITY_CHANNEL", "Invalid utility parent frame"));
        return;
      }
      if (queue.length >= 64 || queuedBytes + value.byteLength > 1048576) {
        abort(failure("ERR_FIREFLY_UTILITY_LIMIT", "Utility parent queue budget exceeded"));
        return;
      }
      queue.push({ wire: value, ports });
      queuedBytes += value.byteLength;
      pump();
    } else if (kind === "error") {
      abort(failure("ERR_FIREFLY_UTILITY_CHANNEL", String(value)));
    } else {
      abort(failure("ERR_FIREFLY_UTILITY_CHANNEL", "Unknown utility parent event"));
    }
  });
  return port;
}
return createParentPort;
})()
