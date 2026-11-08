/** Main-realm port ownership/queue core. Native document/process transport is a separate binding. */
export default function createMessageChannels(native, EventEmitter, error) {
  const states = new WeakMap();
  const apply = Reflect.apply;
  const weakSet = WeakMap.prototype.set;
  const weakGet = WeakMap.prototype.get;
  const define = Object.defineProperty;
  const shift = Array.prototype.shift;
  const isArray = Array.isArray;
  const descriptor = Object.getOwnPropertyDescriptor;
  const Weak = WeakRef;
  const deref = Weak.prototype.deref;
  const Registry = FinalizationRegistry;
  const register = Registry.prototype.register;
  const unregister = Registry.prototype.unregister;
  const records = new Map();
  const token = Symbol('MessagePortMain constructor');
  const schedule = setImmediate;
  const tick = process.nextTick;
  const byteLength = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Uint8Array.prototype),
    'byteLength'
  ).get;
  let sequence = 0;
  let totalBytes = 0;
  let stopped = false;

  function append(array, value) {
    // Define an own slot without consulting mutable Array.prototype setters/push.
    define(array, `${array.length}`, {
      value,
      writable: true,
      enumerable: true,
      configurable: true
    });
  }

  function state(port) {
    const value = apply(weakGet, states, [port]);
    if (!value) {
      throw error('ERR_INVALID_THIS', 'Invalid MessagePortMain receiver');
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
    const record = apply(deref, entry.weak, []);
    if (record) {
      close(record, true);
    } else {
      release(entry.lease);
    }
  });

  // Emitter construction can invoke inherited user hooks. Finish every such
  // allocation before a transfer changes any old owner or queue accounting.
  function shell() {
    const port = new MessagePortMain(token);
    return { port, owner: new Weak(port), gcToken: {} };
  }

  function attach(record, staged) {
    apply(weakSet, states, [staged.port, { record, detached: false }]);
    record.owner = staged.owner;
    record.gcToken = staged.gcToken;
    apply(register, finalizers, [
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
    (retainedOwner ?? apply(deref, record.owner, []))?.emit('close');
  }

  function consume(record, entry) {
    record.lease.count--;
    record.lease.bytes -= entry.size;
    totalBytes -= entry.size;
  }

  function discard(record) {
    const pending = record.queue;
    record.queue = [];
    for (const entry of pending) {
      if (entry.close) {
        continue;
      }
      consume(record, entry);
      for (const port of entry.ports) {
        const owned = apply(weakGet, states, [port]);
        if (owned?.record) {
          close(owned.record, true);
        }
      }
    }
  }

  /** Explicit local close retires incoming work; accepted peer work precedes its close marker. */
  function close(record, local) {
    if (record.closed) {
      return;
    }
    record.closed = true;
    const peer = record.peer;
    record.peer = null;
    if (peer && peer.peer === record) {
      peer.peer = null;
    }
    apply(unregister, finalizers, [record.gcToken]);
    discard(record);
    release(record.lease);
    if (!stopped) {
      if (local) {
        // An explicit close owns its notification even if application code
        // releases the wrapper and GC runs before the deferred callback.
        const owner = apply(deref, record.owner, []);

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
      // A moved endpoint's old task must not clear a new owner's scheduled task.
      if (record.lease.generation !== generation) {
        return;
      }
      record.scheduled = false;
      if (stopped || record.closed || !record.started || !record.queue.length) {
        return;
      }
      const owner = apply(deref, record.owner, []);
      if (!owner) {
        close(record, true);
        return;
      }
      const entry = apply(shift, record.queue, []);
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
          const owned = apply(weakGet, states, [port]);
          if (owned?.record) {
            close(owned.record, true);
          }
        }
        close(record, true);
        throw failure;
      }
      try {
        owner.emit('message', { data, ports: entry.ports });
      } finally {
        if (record.lease.generation === generation) {
          pump(record);
        }
      }
    });
  }

  /** The table holds weak records plus scalar leases, allowing unreachable queued-port cycles to collect. */
  function sweep() {
    for (const entry of [...records.values()]) {
      const record = apply(deref, entry.weak, []);
      if (!record) {
        release(entry.lease);
      } else if (!apply(deref, record.owner, [])) {
        close(record, true);
      }
    }
  }

  function endpoint() {
    if (sequence === Number.MAX_SAFE_INTEGER) {
      throw error('ERR_FIREFLY_PORT_LIMIT', 'Port identity space exhausted');
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
      scheduled: false
    };
    records.set(lease.id, { weak: new Weak(record), lease });
    return record;
  }

  function transfers(value, source, destination) {
    if (value === undefined) {
      return [];
    }
    if (!isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', 'Port transfer list must be an array');
    }
    const length = value.length;
    if (!Number.isSafeInteger(length) || length < 0 || length > 16) {
      throw error('ERR_FIREFLY_PORT_LIMIT', 'At most 16 ports may be transferred');
    }
    const moves = [];
    for (let index = 0; index < length; index++) {
      const itemDescriptor = descriptor(value, String(index));
      if (!itemDescriptor || !Object.hasOwn(itemDescriptor, 'value')) {
        throw error('ERR_INVALID_ARG_TYPE', 'Port transfer entries must be own data values');
      }
      const port = itemDescriptor.value;
      const owned = apply(weakGet, states, [port]);
      if (!owned || owned.detached || !owned.record) {
        throw error('ERR_INVALID_ARG_TYPE', 'Transfer entry is not an owned MessagePortMain');
      }
      const record = owned.record;
      if (record === source) {
        throw error('ERR_FIREFLY_PORT_TRANSFER', 'Cannot transfer the source port');
      }
      if (record === destination || record.closed) {
        throw error(
          'ERR_FIREFLY_UNSUPPORTED_API',
          'Closed or receiving endpoint transfer is not implemented'
        );
      }
      let duplicate = false;
      for (let slot = 0; slot < moves.length; slot++) {
        if (moves[slot].record === record) {
          duplicate = true;
        }
      }
      if (duplicate) {
        throw error('ERR_FIREFLY_PORT_TRANSFER', 'Duplicate port transfer');
      }
      if (record.lease.generation === Number.MAX_SAFE_INTEGER) {
        throw error('ERR_FIREFLY_PORT_LIMIT', 'Port ownership generation exhausted');
      }
      append(moves, { port, owned, record, generation: record.lease.generation });
    }
    return moves;
  }

  class MessagePortMain extends EventEmitter {
    constructor(key) {
      super();
      if (key !== token) {
        throw error('ERR_ILLEGAL_CONSTRUCTOR', 'MessagePortMain cannot be constructed directly');
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
      let wire;
      try {
        wire = native.encodeIpc(message);
      } catch {
        throw error(
          'ERR_FIREFLY_PORT_SERIALIZATION',
          'Port message requires the supported bounded data/binary graph'
        );
      }
      const size = apply(byteLength, wire, []);
      // Preflight resource checks avoid needless wrappers on an already full queue.
      if (
        destination &&
        !destination.closed &&
        (destination.lease.count >= 64 ||
          destination.lease.bytes + size > 1048576 ||
          totalBytes + size > 4194304)
      ) {
        throw error('ERR_FIREFLY_PORT_LIMIT', 'Port queue budget exceeded');
      }
      const staged = [];
      for (let slot = 0; slot < moves.length; slot++) {
        append(staged, shell());
      }
      // Transfer-list proxies may have reentered public methods. Validate every
      // ownership/entanglement snapshot again before changing any endpoint.
      if (
        owned.record !== record ||
        owned.detached ||
        record.closed ||
        record.lease.generation !== generation ||
        record.peer !== destination
      ) {
        throw error('ERR_FIREFLY_PORT_TRANSFER', 'Source changed while preparing a message');
      }
      for (const move of moves) {
        if (
          move.owned.record !== move.record ||
          move.owned.detached ||
          move.record.closed ||
          move.record.lease.generation !== move.generation
        ) {
          throw error(
            'ERR_FIREFLY_PORT_TRANSFER',
            'Transfer ownership changed while preparing a message'
          );
        }
      }
      if (!destination || destination.closed) {
        if (moves.length) {
          throw error(
            'ERR_FIREFLY_UNSUPPORTED_API',
            'Transfer to a disconnected endpoint is not implemented'
          );
        }
        return;
      }
      if (
        destination.lease.count >= 64 ||
        destination.lease.bytes + size > 1048576 ||
        totalBytes + size > 4194304
      ) {
        throw error('ERR_FIREFLY_PORT_LIMIT', 'Port queue budget exceeded');
      }
      const received = [];
      for (let index = 0; index < moves.length; index++) {
        const move = moves[index];
        const target = move.record;
        apply(unregister, finalizers, [target.gcToken]);
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
          'ERR_FIREFLY_UNSUPPORTED_API',
          'MessageChannelMain subclassing is not implemented'
        );
      }
      if (stopped) {
        throw error('ERR_FIREFLY_APP_EXITED', 'Application has exited');
      }
      const firstOwner = shell();
      const secondOwner = shell();
      if (stopped) {
        throw error('ERR_FIREFLY_APP_EXITED', 'Application exited during port construction');
      }
      sweep();
      if (records.size + 2 > 256) {
        throw error('ERR_FIREFLY_PORT_LIMIT', 'At most 256 live main endpoints are supported');
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
    for (const entry of [...records.values()]) {
      const record = apply(deref, entry.weak, []);
      if (record) {
        close(record, true);
      } else {
        release(entry.lease);
      }
    }
  }

  return { MessageChannelMain, shutdown };
}
