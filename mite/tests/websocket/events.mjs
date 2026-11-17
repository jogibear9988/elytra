/** Pure event constructors and pre-I/O WebSocket validation use actual owner-realm brands. */
import assert from 'node:assert/strict';
import process from 'node:process';
for (const Type of [MessageEvent, CloseEvent]) {
  assert.throws(() => new Type(), TypeError);

  const event = new Type('owned');
  assert.equal(event instanceof Event, true);
  assert.equal(event.type, 'owned');
  assert.equal(event.bubbles, false);
  assert.equal(event.cancelable, false);
}
const message = new MessageEvent('message');
assert.equal(message.data, null);
assert.equal(message.origin, '');
assert.equal(message.lastEventId, '');
assert.equal(message.source, null);
assert.deepEqual(message.ports, []);
const close = new CloseEvent('close');
assert.equal(close.wasClean, false);
assert.equal(close.code, 0);
assert.equal(close.reason, '');
for (const [Type, names] of [
  [MessageEvent, ['data', 'origin', 'lastEventId', 'source', 'ports']],
  [CloseEvent, ['wasClean', 'code', 'reason']]
]) {
  for (const name of names) {
    const descriptor = Object.getOwnPropertyDescriptor(Type.prototype, name);
    assert.equal(typeof descriptor.get, 'function');
    assert.equal(descriptor.set, undefined);

    assert.throws(() => Reflect.apply(descriptor.get, {}, []), TypeError);
  }
}
console.log('PASS WebSocket event inheritance, defaults and readonly receiver brands');

const reads = Object.create(null);

function once(name, value) {
  return () => {
    reads[name] = (reads[name] ?? 0) + 1;
    return value;
  };
}

const data = { owned: 1 };
const init = {};
for (const [name, value] of [
  ['data', data],
  ['origin', 'owned-origin'],
  ['lastEventId', 'owned-id'],
  ['source', null],
  ['ports', []]
]) {
  Object.defineProperty(init, name, { get: once(name, value) });
}
const populated = new MessageEvent('message', init);
assert.equal(populated.data, data);
assert.equal(populated.origin, 'owned-origin');
assert.equal(populated.lastEventId, 'owned-id');
for (const name of ['data', 'origin', 'lastEventId', 'source', 'ports']) {
  assert.equal(reads[name], 1);
}
const closeReads = Object.create(null);
const closeInit = {};
for (const [name, value] of [
  ['wasClean', true],
  ['code', 65537],
  ['reason', 'owned-reason']
]) {
  Object.defineProperty(closeInit, name, {
    get() {
      closeReads[name] = (closeReads[name] ?? 0) + 1;
      return value;
    }
  });
}
const populatedClose = new CloseEvent('close', closeInit);
assert.equal(populatedClose.wasClean, true);
assert.equal(populatedClose.code, 1);
assert.equal(populatedClose.reason, 'owned-reason');
for (const name of ['wasClean', 'code', 'reason']) {
  assert.equal(closeReads[name], 1);
}
if (process.versions.mite) {
  assert.throws(
    () => new MessageEvent('message', { source: { owned: true } }),
    (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
  );

  assert.throws(
    () => new MessageEvent('message', { ports: [{}] }),
    (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
  );
}
console.log('PASS event dictionary getters once, payload identity and explicit capability limits');

const sentinel = new Error('owned URL conversion');

assert.throws(
  () =>
    new WebSocket({
      toString() {
        throw sentinel;
      }
    }),
  (error) => error === sentinel
);

let protocolConversions = 0;

assert.throws(
  () =>
    new WebSocket('ws://127.0.0.1:1/', [
      'owned',
      {
        toString() {
          protocolConversions++;
          return 'owned';
        }
      }
    ]),
  (error) => error.name === 'SyntaxError'
);

assert.equal(protocolConversions, 1);
const protocolError = new Error('owned protocol conversion');

assert.throws(
  () =>
    new WebSocket('ws://127.0.0.1:1/', [
      {
        toString() {
          throw protocolError;
        }
      }
    ]),
  (error) => error === protocolError
);

assert.throws(
  () => new WebSocket('ws://127.0.0.1:1/', ['bad token']),
  (error) => error.name === 'SyntaxError'
);

if (process.versions.mite) {
  let steps = 0;
  let returns = 0;
  let conversions = 0;
  const protocols = {
    [Symbol.iterator]() {
      return {
        next() {
          const index = ++steps;
          return {
            done: false,
            value: {
              toString() {
                conversions++;
                return 'protocol' + index;
              }
            }
          };
        },

        return() {
          returns++;
          return { done: true };
        }
      };
    }
  };

  assert.throws(
    () => new WebSocket('ws://127.0.0.1:1/', { protocols }),
    (error) => error.code === 'ERR_MITE_WEBSOCKET_LIMIT'
  );

  assert.equal(steps, 65);
  assert.equal(returns, 1);
  assert.equal(conversions, 64);
}
console.log(
  'PASS pre-I/O URL/protocol conversions, duplicate refusal and bounded iterator cleanup'
);
