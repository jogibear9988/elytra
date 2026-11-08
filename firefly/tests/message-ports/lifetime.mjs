/** Actual native codec, VM GC and job scheduling exercise port ownership/resource invariants. */
import native from 'port-lifetime-native';
import { EventEmitter } from 'node:events';
import createMessageChannels from '../../core/message-ports/index.mjs';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const failure = (code, message) => Object.assign(new Error(message), { code });

const fresh = () => createMessageChannels(native, EventEmitter, failure);

const turn = () => new Promise((resolve) => setImmediate(resolve));

const tick = () => new Promise((resolve) => process.nextTick(resolve));

async function collect() {
  // WeakRef construction/deref protects its target until the current checkpoint.
  // Collect only after yielding and do not inspect weak refs until all rounds finish.
  for (let index = 0; index < 12; index++) {
    await turn();
    native.collectGarbage();
  }
  await turn();
}

function closeChannels(channels) {
  for (const channel of channels) {
    channel.port1.close();
    channel.port2.close();
  }
}

// A public close owns its accepted notification until that queued callback runs,
// even when application references disappear and GC runs before nextTick.
{
  const api = fresh();
  let channel;
  let port;
  const peer = (() => {
    const created = new api.MessageChannelMain();
    channel = created;
    port = created.port1;
    return created.port2;
  })();
  let called = 0;

  port.on('close', () => called++);

  peer.start();
  await turn();

  function closeAndDrop() {
    port.close();
    port = null;
    channel = null;
  }

  closeAndDrop();
  native.collectGarbage();
  await tick();
  check(called === 1, 'GC lost the pending public close notification');
  await turn();
  native.collectGarbage();
  await tick();
  check(called === 1, 'Public close notification was repeated');
  peer.close();
  api.shutdown();
}

// Losing an unstarted endpoint must disconnect its still-reachable peer.
{
  const api = fresh();
  let peer;
  let weak;
  (() => {
    const channel = new api.MessageChannelMain();
    peer = channel.port1;
    weak = new WeakRef(channel.port2);
  })();
  let closed = 0;

  peer.on('close', () => closed++);

  peer.start();
  await collect();
  check(weak.deref() === undefined && closed === 1, 'GC did not disconnect the surviving endpoint');
  const capacity = Array.from({ length: 128 }, () => new api.MessageChannelMain());
  closeChannels(capacity);
  api.shutdown();
}

// Queued transfers can form cycles even though no application wrapper remains.
// Scalar resource leases must not root those records/queued wrappers forever.
{
  const api = fresh();
  const weak = [];
  for (let index = 0; index < 64; index++) {
    (() => {
      const a = new api.MessageChannelMain();
      const b = new api.MessageChannelMain();
      weak.push(new WeakRef(a.port1), new WeakRef(b.port1));
      a.port1.postMessage(new Uint8Array(16384), [b.port2]);
      b.port1.postMessage(new Uint8Array(16384), [a.port2]);
    })();
  }
  await collect();
  check(
    weak.every((ref) => ref.deref() === undefined),
    'Unreachable channel wrappers remained rooted'
  );
  const capacity = Array.from({ length: 128 }, () => new api.MessageChannelMain());
  closeChannels(capacity);
  // Also prove that dead cycles released their queued native wire-byte budget.
  const budget = Array.from({ length: 4 }, () => new api.MessageChannelMain());
  for (const channel of budget) {
    for (let index = 0; index < 30; index++) {
      channel.port1.postMessage(new Uint8Array(32768));
    }
  }
  closeChannels(budget);
  api.shutdown();
}

// An old transferred wrapper may collect without closing its new owner. Native
// payload snapshots must survive GC and preserve queued bytes across the move.
{
  const api = fresh();
  // Finish the setup frame before collection; TLA operand temporaries are not
  // evidence of broker ownership and must not keep the old argument wrapper alive.
  const setup = () => {
    const carrier = new api.MessageChannelMain();
    const payload = new api.MessageChannelMain();
    const peer = payload.port1;
    const weak = new WeakRef(payload.port2);
    const bytes = new Uint8Array(32768);
    bytes[0] = 7;
    bytes[bytes.length - 1] = 9;
    peer.postMessage(bytes);
    bytes.fill(1);
    const received = new Promise((resolve) =>
      carrier.port2.once('message', (event) => resolve(event.ports[0]))
    );
    carrier.port2.start();
    carrier.port1.postMessage(null, [payload.port2]);
    return { carrier, peer, weak, received };
  };

  const { carrier, peer, weak, received } = setup();
  let closed = 0;

  peer.on('close', () => closed++);

  peer.start();
  const moved = await received;
  await collect();
  check(weak.deref() === undefined && closed === 0, 'Old wrapper GC closed transferred ownership');
  const data = new Promise((resolve) => moved.once('message', (event) => resolve(event.data)));
  moved.start();
  const result = await data;
  check(
    result instanceof Uint8Array &&
      result.length === 32768 &&
      result[0] === 7 &&
      result[result.length - 1] === 9,
    'Native queued snapshot did not survive ownership/GC'
  );
  moved.close();
  peer.close();
  closeChannels([carrier]);
  api.shutdown();
}

// Transfer wrapper construction may execute hostile inherited emitter accessors.
// Such an exception must occur before detaching any source endpoint.
{
  const api = fresh();
  const carrier = new api.MessageChannelMain();
  const payload = new api.MessageChannelMain();
  const prototype = Object.getPrototypeOf(payload.port2);
  const previous = Object.getOwnPropertyDescriptor(prototype, '_maxListeners');
  const sentinel = new Error('wrapper constructor accessor');
  Object.defineProperty(prototype, '_maxListeners', {
    configurable: true,

    get() {
      throw sentinel;
    }
  });
  let caught;
  try {
    carrier.port1.postMessage(null, [payload.port2]);
  } catch (error) {
    caught = error;
  } finally {
    if (previous) {
      Object.defineProperty(prototype, '_maxListeners', previous);
    } else {
      delete prototype._maxListeners;
    }
  }
  check(caught === sentinel, 'Constructor exception was hidden');
  const reply = new Promise((resolve) =>
    payload.port1.once('message', (event) => resolve(event.data))
  );
  payload.port1.start();
  payload.port2.postMessage('still-owned');
  check((await reply) === 'still-owned', 'Failed transfer detached the old owner');
  let delivered = 0;

  carrier.port2.on('message', () => delivered++);

  carrier.port2.start();
  await turn();
  await turn();
  check(delivered === 0, 'Failed transfer queued a receiver message');
  closeChannels([carrier, payload]);
  api.shutdown();
}

// If construction reenters and moves an endpoint elsewhere, the outer snapshot
// must fail validation and must not steal ownership from the successful inner move.
{
  const api = fresh();
  const outer = new api.MessageChannelMain();
  const inner = new api.MessageChannelMain();
  const payload = new api.MessageChannelMain();
  const prototype = Object.getPrototypeOf(payload.port2);
  const previous = Object.getOwnPropertyDescriptor(prototype, '_maxListeners');
  let reentered = false;
  Object.defineProperty(prototype, '_maxListeners', {
    configurable: true,

    get() {
      if (!reentered) {
        reentered = true;
        inner.port1.postMessage('inner', [payload.port2]);
      }
      return undefined;
    },

    set(value) {
      Object.defineProperty(this, '_maxListeners', { value, configurable: true, writable: true });
    }
  });
  let rejected;
  try {
    outer.port1.postMessage('outer', [payload.port2]);
  } catch (error) {
    rejected = error;
  } finally {
    if (previous) {
      Object.defineProperty(prototype, '_maxListeners', previous);
    } else {
      delete prototype._maxListeners;
    }
  }
  check(
    reentered && rejected?.code === 'ERR_FIREFLY_PORT_TRANSFER',
    'Reentrant construction did not invalidate the outer snapshot'
  );
  const received = new Promise((resolve) =>
    inner.port2.once('message', (event) => resolve(event.ports[0]))
  );
  inner.port2.start();
  const moved = await received;
  const response = new Promise((resolve) => moved.once('message', (event) => resolve(event.data)));
  moved.start();
  payload.port1.postMessage('new-owner');
  check((await response) === 'new-owner', 'Outer failure corrupted the successful inner owner');
  let delivered = 0;

  outer.port2.on('message', () => delivered++);

  outer.port2.start();
  await turn();
  check(delivered === 0, 'Outer reentrant transfer committed a message');
  moved.close();
  closeChannels([outer, inner, payload]);
  api.shutdown();
}

// Transfer-list evaluation may replace an ordinary Array method. The commit
// path must still use own-slot insertion rather than call that replacement.
{
  const api = fresh();
  const carrier = new api.MessageChannelMain();
  const payload = new api.MessageChannelMain();
  const original = Array.prototype.push;
  let reached = false;
  const list = new Proxy([payload.port2], {
    get(target, name, receiver) {
      if (name === 'length') {
        reached = true;

        Array.prototype.push = () => {
          throw Error('mutable push entered port commit');
        };
      }
      return Reflect.get(target, name, receiver);
    }
  });
  try {
    carrier.port1.postMessage('committed', list);
  } finally {
    Array.prototype.push = original;
  }
  check(reached, 'Transfer-list proxy did not run');
  const received = new Promise((resolve) =>
    carrier.port2.once('message', (event) => resolve(event.ports[0]))
  );
  carrier.port2.start();
  const moved = await received;
  const response = new Promise((resolve) => moved.once('message', (event) => resolve(event.data)));
  moved.start();
  payload.port1.postMessage('push-neutral-owner');
  check((await response) === 'push-neutral-owner', 'Mutable push corrupted committed ownership');
  moved.close();
  closeChannels([carrier, payload]);
  api.shutdown();
}

// Dropping an undelivered transfer closes its received endpoint rather than
// leaving an ownerless live capability and resource lease behind.
{
  const api = fresh();
  const carrier = new api.MessageChannelMain();
  const payload = new api.MessageChannelMain();
  const disconnected = new Promise((resolve) => payload.port1.once('close', resolve));
  payload.port1.start();
  carrier.port1.postMessage(null, [payload.port2]);
  carrier.port2.close();
  await disconnected;
  closeChannels([carrier, payload]);
  api.shutdown();
}

// A reentrant constructor hook cannot create endpoints after broker shutdown.
{
  const api = fresh();
  const channel = new api.MessageChannelMain();
  const prototype = Object.getPrototypeOf(channel.port1);
  closeChannels([channel]);
  const previous = Object.getOwnPropertyDescriptor(prototype, '_maxListeners');
  Object.defineProperty(prototype, '_maxListeners', {
    configurable: true,

    get() {
      api.shutdown();
      return undefined;
    },

    set(value) {
      Object.defineProperty(this, '_maxListeners', { value, configurable: true, writable: true });
    }
  });
  let error;
  try {
    new api.MessageChannelMain();
  } catch (value) {
    error = value;
  } finally {
    if (previous) {
      Object.defineProperty(prototype, '_maxListeners', previous);
    } else {
      delete prototype._maxListeners;
    }
  }
  check(error?.code === 'ERR_FIREFLY_APP_EXITED', 'Constructor escaped a reentrant shutdown');
}

await tick();
console.log(
  'PASS native port codec/GC, queued ownership cycles, lease reclamation and atomic reentrant transfer'
);
