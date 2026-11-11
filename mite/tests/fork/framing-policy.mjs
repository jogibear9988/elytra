/** Controlled transport policy only; real self-fork acceptance remains in main.mjs. */
import { EventEmitter } from 'node:events';
import createIpcChannel from '../../modules/process/ipc-channel.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function component() {
  const target = new EventEmitter();
  const ticks = [];
  const messages = [];
  const errors = [];
  const metrics = { copied: 0, maximum: 0, closed: 0, shutdown: 0 };
  const instrumented = {
    alloc(length) {
      metrics.maximum = Math.max(metrics.maximum, length);
      const buffer = Buffer.alloc(length);

      buffer.set = function (source, offset) {
        metrics.copied += source.length;
        Uint8Array.prototype.set.call(this, source, offset);
      };

      return buffer;
    },

    from(...args) {
      return Buffer.from(...args);
    },

    concat() {
      throw Error('Incremental framing must not repeatedly concatenate previous bytes');
    }
  };
  const channel = createIpcChannel({
    target,
    Buffer: instrumented,

    tick: (callback) => ticks.push(callback),

    childMode: true,
    native: {
      ref() {},

      close() {
        metrics.closed++;
      },

      shutdown(callback) {
        metrics.shutdown++;

        ticks.push(() => callback(0));

        return 0;
      },

      write() {
        throw Error('Unexpected outgoing write');
      }
    },

    systemError: (status) => Object.assign(Error('native'), { code: String(status) })
  });

  target.on('message', (value) => messages.push(value));

  target.on('error', (error) => errors.push(error.code));

  function flush() {
    let count = 0;
    while (ticks.length) {
      check(++count < 1000, 'Bounded asynchronous policy delivery');
      ticks.shift()();
    }
  }

  return { target, channel, messages, errors, metrics, flush };
}

const unicode = component();
const expected = [{ text: 'Grüße 日本語 🪲', escaped: 'line\nline' }, null, true, [1, 'a']];
const wire = Buffer.from(expected.map((value) => JSON.stringify(value) + '\n').join(''));
for (const byte of wire) {
  unicode.channel.receive('data', Uint8Array.of(byte));
}
unicode.flush();
check(
  JSON.stringify(unicode.messages) === JSON.stringify(expected),
  'Split UTF-8/newline FIFO and offsets'
);
check(
  unicode.metrics.copied < wire.length * 3 && unicode.metrics.maximum <= 1024,
  'Linear tiny-fragment copy budget'
);

const snapshot = component();
const prefix = Buffer.from('{"value":"kept');
snapshot.channel.receive('data', prefix);
prefix.fill(0x78);
snapshot.channel.receive('data', Buffer.from('"}\n'));
snapshot.flush();
check(
  snapshot.messages[0].value === 'kept',
  'Partial native input is snapshotted before caller mutation'
);

const large = component();
const bigWire = Buffer.from(JSON.stringify('x'.repeat(768 * 1024)) + '\n');
for (let offset = 0; offset < bigWire.length; offset += 8191) {
  large.channel.receive('data', bigWire.subarray(offset, offset + 8191));
}
large.flush();
check(
  large.messages.length === 1 && large.messages[0].length === 768 * 1024,
  'Large fragmented complete frame'
);
check(
  large.metrics.copied < bigWire.length * 3 && large.metrics.maximum <= 1048576,
  'Geometric accumulator bounded by frame budget'
);

const boundary = component();
const maximumWire = Buffer.from(JSON.stringify('x'.repeat(1048573)) + '\n');
check(maximumWire.length === 1048576, 'Frame limit includes newline');
boundary.channel.receive('data', maximumWire);
boundary.flush();
check(boundary.messages.length === 1 && !boundary.errors.length, 'Exact 1 MiB wire frame accepted');
const excessive = component();
excessive.channel.receive('data', Buffer.alloc(1048576, 0x78));
excessive.flush();
check(
  excessive.errors.join() === 'ERR_IPC_CHANNEL_DATA' && excessive.metrics.closed === 1,
  'Unterminated frame limit closes once'
);

for (const invalid of [
  Uint8Array.of(0x22, 0xc0, 0xaf, 0x22, 0x0a),
  Uint8Array.of(0xef, 0xbb, 0xbf, 0x7b, 0x7d, 0x0a),
  Uint8Array.of(0x0a)
]) {
  const peer = component();
  for (const byte of invalid) {
    peer.channel.receive('data', Uint8Array.of(byte));
  }
  peer.flush();
  check(
    peer.errors.join() === 'ERR_IPC_CHANNEL_DATA' &&
      !peer.messages.length &&
      peer.metrics.closed === 1,
    'Invalid UTF-8/BOM/empty JSON rejected once'
  );
}
const partial = component();
partial.channel.receive('data', Buffer.from('{"partial":'));
partial.channel.receive('end');
partial.channel.receive('end');
partial.channel.receive('data', Buffer.from('true}\n'));
partial.flush();
check(
  partial.errors.join() === 'ERR_IPC_CHANNEL_DATA' &&
    !partial.messages.length &&
    partial.metrics.shutdown === 1,
  'Partial EOF closes once and rejects late bytes'
);

const reentry = component();

reentry.target.on('message', () => reentry.channel.disconnect());

reentry.channel.receive('data', Buffer.from('1\n2\n'));
reentry.flush();
check(
  reentry.messages.join() === '1,2' && reentry.metrics.shutdown === 1,
  'Disconnect during delivery preserves accepted FIFO'
);
const count = component();
count.channel.receive('data', Buffer.from('null\n'.repeat(65)));
count.flush();
check(
  count.messages.length === 64 && count.errors.join() === 'ERR_IPC_CHANNEL_LIMIT',
  'Message queue quota preserved'
);
const budget = component();
const unit = Buffer.from(JSON.stringify('z'.repeat(900000)) + '\n');
for (let index = 0; index < 5; index++) {
  budget.channel.receive('data', unit);
}
budget.flush();
check(
  budget.messages.length === 4 && budget.errors.join() === 'ERR_IPC_CHANNEL_LIMIT',
  'Four MiB delivery byte quota preserved'
);
console.log('PASS controlled IPC linear fragmented UTF8 framing budgets EOF and reentry');
