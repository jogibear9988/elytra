/** Public main-process port acceptance, using real Firefly/Electron runtimes without windows. */
import { app, MessageChannelMain } from 'firefly';

const limited = process.versions.electron === undefined;

const wait = (duration) => new Promise((resolve) => setTimeout(resolve, duration));

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const channels = [];

function channel() {
  const value = new MessageChannelMain();
  channels.push(value);
  return value;
}

function failure(action, name) {
  let caught;
  try {
    action();
  } catch (error) {
    caught = error;
  }
  check(caught instanceof Error, name + ' must reject explicitly');
  return caught;
}

function next(port) {
  return new Promise((resolve) => port.once('message', resolve));
}

async function startAndRead(port) {
  const message = next(port);
  port.start();
  return message;
}

function cleanup() {
  for (const value of channels) {
    value.port1.close();
    value.port2.close();
  }
}

const watchdog = setTimeout(() => {
  console.error('FAIL public main MessageChannelMain: timed out');
  app.exit(1);
}, 15000);

// Electron ESM readiness must be observed after the entry module returns.
app
  .whenReady()
  .then(async () => {
    const pair = channel();
    check(pair.port1 !== pair.port2, 'distinct connected endpoints');
    const delivered = [];

    pair.port2.on('message', (event) => delivered.push(event.data));

    pair.port1.postMessage('first');
    pair.port1.postMessage('second');
    await wait(30);
    check(delivered.length === 0, 'adding a listener must not start delivery');
    const third = new Promise((resolve) => {
      pair.port2.on('message', () => {
        if (delivered.length === 3) {
          resolve();
        }
      });
    });
    pair.port2.start();
    pair.port2.start();
    pair.port1.postMessage('third');
    await third;
    check(delivered.join(',') === 'first,second,third', 'FIFO across pre/post-start messages');

    const snapshotPair = channel();
    const backing = new ArrayBuffer(16);
    const bytes = new Uint8Array(backing);
    bytes.set([1, 2, 3, 4]);
    const shared = { label: 'before' };
    const payload = {
      first: shared,
      second: shared,
      backing,
      bytes,
      view: new DataView(backing, 1, 2),
      undef: undefined,
      minusZero: -0,
      nan: NaN
    };
    payload.self = payload;
    snapshotPair.port1.postMessage(payload);
    shared.label = 'after';
    bytes[1] = 99;
    const event = await startAndRead(snapshotPair.port2);
    check(event.ports.length === 0 && event.data !== payload, 'destination-owned event snapshot');
    const copy = event.data;
    check(copy.self === copy && copy.first === copy.second, 'cycles and object aliases');
    check(copy.first.label === 'before', 'snapshot occurs when postMessage is called');
    check(copy.bytes instanceof Uint8Array && copy.view instanceof DataView, 'binary view types');
    check(
      copy.bytes.buffer === copy.backing && copy.view.buffer === copy.backing,
      'binary backing aliases'
    );
    check(copy.bytes[1] === 2 && copy.view.getUint8(0) === 2, 'copied binary bytes');
    check(
      copy.backing !== backing && backing.byteLength === 16,
      'message buffers copy, not detach'
    );
    check(
      'undef' in copy &&
        copy.undef === undefined &&
        Object.is(copy.minusZero, -0) &&
        Number.isNaN(copy.nan),
      'primitive fidelity'
    );
    copy.bytes[2] = 55;
    check(
      copy.view.getUint8(1) === 55 && bytes[2] === 3,
      'destination aliases stay isolated from sender'
    );

    const carry = channel();
    const data = channel();
    let oldDeliveries = 0;

    data.port2.on('message', () => oldDeliveries++);

    data.port1.postMessage('queued-before-handoff');
    data.port2.start();
    const incoming = next(carry.port2);
    carry.port2.start();
    carry.port1.postMessage({ kind: 'handoff' }, [data.port2]);

    data.port2.postMessage(() => {});

    data.port2.start();
    data.port2.close();
    const handoff = await incoming;
    check(
      handoff.data.kind === 'handoff' && handoff.ports.length === 1,
      'transferred endpoint event'
    );
    const received = handoff.ports[0];
    check(received !== data.port2, 'transferred wrapper is a different owner');
    const queued = [];

    received.on('message', (message) => queued.push(message.data));

    await wait(30);
    check(
      queued.length === 0 && oldDeliveries === 0,
      'transfer revokes old owner and resets start'
    );
    const firstHandoffMessage = next(received);
    received.start();
    check(
      (await firstHandoffMessage).data === 'queued-before-handoff',
      'queued payload follows ownership'
    );

    // A scheduled task from an older owner must not consume the new owner's queue.
    const relay = channel();
    const relayIncoming = next(relay.port2);
    relay.port2.start();
    data.port1.postMessage('queued-before-second-handoff');
    relay.port1.postMessage(null, [received]);
    received.close();
    const finalPort = (await relayIncoming).ports[0];
    check(finalPort !== received, 'second transfer changes owner again');
    const finalMessage = next(finalPort);
    finalPort.start();
    check(
      (await finalMessage).data === 'queued-before-second-handoff',
      'stale scheduled task cannot drain reassigned port'
    );
    const reply = next(data.port1);
    data.port1.start();
    finalPort.postMessage('reply-through-transferred-endpoint');
    check(
      (await reply).data === 'reply-through-transferred-endpoint',
      'transferred endpoint remains bidirectional'
    );

    const atomic = channel();

    failure(() => carry.port1.postMessage(() => {}, [atomic.port2]), 'uncloneable payload');

    atomic.port1.postMessage('still-owned-after-failure');
    check(
      (await startAndRead(atomic.port2)).data === 'still-owned-after-failure',
      'failed serialization leaves transfer ownership intact'
    );

    failure(() => carry.port1.postMessage(null, [carry.port1]), 'source port transfer');

    failure(() => carry.port1.postMessage(null, [new ArrayBuffer(1)]), 'non-port transfer');

    for (const started of [true, false]) {
      const closing = channel();
      const events = [];
      let selfClosed = 0;
      const peerClosed = new Promise((resolve) => {
        closing.port2.once('close', () => {
          events.push('close');
          resolve();
        });
      });

      closing.port1.on('close', () => selfClosed++);

      closing.port2.on('message', (message) => events.push(message.data));

      if (started) {
        closing.port2.start();
      }
      closing.port1.postMessage(1);
      closing.port1.postMessage(2);
      closing.port1.close();
      if (!started) {
        await wait(30);
        check(events.length === 0, 'unstarted peer retains data and close marker');
        closing.port2.start();
      }
      await peerClosed;
      check(
        events.join(',') === '1,2,close' && selfClosed === 1,
        'accepted messages precede peer close exactly once'
      );

      closing.port1.postMessage(() => {});

      closing.port1.start();
      closing.port1.close();
    }

    if (limited) {
      const closed = channel();
      closed.port1.close();
      const unsupported = failure(
        () => carry.port1.postMessage(null, [closed.port1]),
        'closed transfer limitation'
      );
      check(
        unsupported.code === 'ERR_FIREFLY_UNSUPPORTED_API',
        'closed transfer reports unavailable contract'
      );
      const destination = channel();
      const refusal = failure(
        () => destination.port1.postMessage(null, [destination.port2]),
        'destination transfer limitation'
      );
      check(
        refusal.code === 'ERR_FIREFLY_UNSUPPORTED_API',
        'destination transfer reports unavailable contract'
      );
    }

    finalPort.close();
    cleanup();

    if (limited) {
      const quota = channel();
      for (let index = 0; index < 64; index++) {
        quota.port1.postMessage(index);
      }
      const movable = channel();
      const full = failure(
        () => quota.port1.postMessage(null, [movable.port2]),
        'queued-message quota'
      );
      check(full.code === 'ERR_FIREFLY_PORT_LIMIT', 'message quota reports a resource limit');
      movable.port1.postMessage('owned-after-quota-refusal');
      check(
        (await startAndRead(movable.port2)).data === 'owned-after-quota-refusal',
        'quota refusal preserves transferable ownership'
      );
      const numbers = [];
      const drained = new Promise((resolve) =>
        quota.port2.on('message', (message) => {
          numbers.push(message.data);
          if (numbers.length === 64) {
            resolve();
          }
        })
      );
      quota.port2.start();
      await drained;
      check(
        numbers.every((value, index) => value === index),
        'quota drain preserves FIFO'
      );
      const afterDrain = next(quota.port2);
      quota.port1.postMessage('after-drain');
      check((await afterDrain).data === 'after-drain', 'dequeue refunds message quota');
      cleanup();

      const bytes = new Uint8Array(65536);
      const byteQuota = channel();
      let accepted = 0;
      for (; accepted < 64; accepted++) {
        try {
          byteQuota.port1.postMessage(bytes);
        } catch (error) {
          check(error.code === 'ERR_FIREFLY_PORT_LIMIT', 'byte queue reports a resource limit');
          break;
        }
      }
      check(
        accepted > 0 && accepted < 64,
        'per-endpoint byte quota is independent of message quota'
      );
      cleanup();
      // Four mostly full endpoint queues leave less than one endpoint's budget.
      for (let owner = 0; owner < 4; owner++) {
        const filled = channel();
        for (let index = 0; index < 15; index++) {
          filled.port1.postMessage(bytes);
        }
      }
      const global = channel();
      let globalAccepted = 0;
      for (; globalAccepted < 15; globalAccepted++) {
        try {
          global.port1.postMessage(bytes);
        } catch (error) {
          check(
            error.code === 'ERR_FIREFLY_PORT_LIMIT',
            'aggregate byte quota reports resource limit'
          );
          break;
        }
      }
      check(
        globalAccepted < 15,
        'aggregate byte budget applies across otherwise independent queues'
      );
      cleanup();
      const fresh = channel();
      fresh.port1.postMessage(bytes);
      const freshEvent = await startAndRead(fresh.port2);
      check(freshEvent.data.byteLength === 65536, 'queue destruction refunds aggregate bytes');
      const oversized = failure(
        () => fresh.port1.postMessage(new Uint8Array(65537)),
        'backing-store copy cap'
      );
      check(
        oversized.code === 'ERR_FIREFLY_PORT_SERIALIZATION',
        'oversize copy is refused before delivery'
      );
      const illegal = failure(
        () => new fresh.port1.constructor(),
        'direct MessagePortMain constructor'
      );
      check(
        illegal.code === 'ERR_ILLEGAL_CONSTRUCTOR',
        'port construction requires an actual channel capability'
      );
      const forged = Object.create(Object.getPrototypeOf(fresh.port1));
      check(
        failure(() => forged.postMessage('fake'), 'forged receiver').code === 'ERR_INVALID_THIS',
        'prototype forgery cannot create an endpoint'
      );
      cleanup();
      for (let index = 0; index < 128; index++) {
        channel();
      }
      check(
        failure(() => channel(), 'endpoint quota').code === 'ERR_FIREFLY_PORT_LIMIT',
        'endpoint allocation is bounded'
      );
      cleanup();
      const reclaimed = channel();
      reclaimed.port1.postMessage('reclaimed');
      check(
        (await startAndRead(reclaimed.port2)).data === 'reclaimed',
        'explicit close releases endpoint slots'
      );
      cleanup();
    }
    clearTimeout(watchdog);
    console.log(
      'PASS public main MessageChannelMain: start/FIFO, copy/cycles/binary aliases, multi-hop transfer, atomic failure and close ordering'
    );
    app.exit(0);
  })
  .catch((error) => {
    console.error(error.stack ?? error);
    clearTimeout(watchdog);
    cleanup();
    app.exit(1);
  });
