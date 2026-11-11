/** Real subscriptions, tracing order and ALS contexts; no network or GUI. */
import assert from 'node:assert/strict';
import dc from 'node:diagnostics_channel';
import { AsyncLocalStorage } from 'node:async_hooks';

const calls = [];
const channel = dc.channel('owned-test');
assert.equal(channel, dc.channel('owned-test'));
assert.equal(channel instanceof dc.Channel, true);
assert.equal(dc.hasSubscribers('owned-test'), false);

const second = (value, name) => calls.push([2, value, name]);

const first = (value, name) => {
  calls.push([1, value, name]);
  channel.unsubscribe(second);
};

channel.subscribe(first);
dc.subscribe('owned-test', second);
channel.publish(7);
channel.publish(8);
assert.deepEqual(calls, [
  [1, 7, 'owned-test'],
  [2, 7, 'owned-test'],
  [1, 8, 'owned-test']
]);
assert.equal(dc.unsubscribe('owned-test', first), true);
assert.equal(channel.hasSubscribers, false);
const storage = new AsyncLocalStorage();

channel.bindStore(storage, (value) => ({ value }));

assert.equal(channel.hasSubscribers, true);
assert.equal(
  channel.runStores(
    41,
    function (add) {
      return storage.getStore().value + this.n + add;
    },
    { n: 1 },
    1
  ),
  43
);
assert.equal(storage.getStore(), undefined);
assert.equal(channel.unbindStore(storage), true);
assert.equal(channel.unbindStore(storage), false);
assert.equal(channel.hasSubscribers, false);

const trace = dc.tracingChannel('owned-trace');
const events = [];
const handlers = Object.fromEntries(
  ['start', 'end', 'asyncStart', 'asyncEnd', 'error'].map((name) => [
    name,
    (context) => events.push([name, context.result, context.error?.message])
  ])
);
trace.subscribe(handlers);
assert.equal(
  trace.traceSync(
    function (n) {
      return this.n + n;
    },
    {},
    { n: 2 },
    3
  ),
  5
);
assert.deepEqual(
  events.map((value) => value[0]),
  ['start', 'end']
);
events.length = 0;
assert.equal(await trace.tracePromise(async (n) => n + 1, {}, null, 6), 7);
assert.deepEqual(
  events.map((value) => value[0]),
  ['start', 'end', 'asyncStart', 'asyncEnd']
);
events.length = 0;
const failure = new Error('original');

assert.throws(
  () =>
    trace.traceSync(() => {
      throw failure;
    }),
  (error) => error === failure
);

assert.deepEqual(
  events.map((value) => value[0]),
  ['start', 'error', 'end']
);
assert.equal(trace.unsubscribe(handlers), true);
assert.equal(trace.hasSubscribers, false);
console.log('PASS diagnostics channels, tracing and real ALS stores');
