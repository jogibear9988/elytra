/** Power-monitor policy unit with a controlled query/event peer, never OS transitions. */
import createApi from '../../core/api.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import * as urls from 'node:url';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  let failure;
  try {
    action();
  } catch (error) {
    failure = error;
  }
  check(
    failure instanceof Error &&
      (Array.isArray(code) ? code.includes(failure.code) : failure.code === code),
    'Expected ' + code + ', got ' + failure?.code
  );
  return failure;
}

const eventNames = [
  'suspend',
  'resume',
  'on-ac',
  'on-battery',
  'thermal-state-change',
  'speed-limit-change',
  'shutdown',
  'lock-screen',
  'unlock-screen',
  'user-did-become-active',
  'user-did-resign-active'
];
const dataError = 'ERR_FIREFLY_POWER_MONITOR_DATA';

function fixture() {
  const state = {
    handler: null,
    starts: [],
    stops: [],
    queries: [],
    capabilities: [...eventNames],
    values: {
      'idle-time': 7,
      'idle-state': 'idle',
      'thermal-state': 'nominal',
      'battery-power': false
    },
    onQuery: null,
    onStart: null,
    onStop: null,
    queryError: null,
    queryRaw: null,
    capsError: null,
    exits: []
  };
  const native = {
    appPath: process.cwd(),

    setHandler(handler) {
      state.handler = handler;
    },

    powerMonitorQuery(kind, threshold) {
      state.queries.push({ kind, threshold });
      state.onQuery?.(kind, threshold);
      if (state.queryError) {
        throw state.queryError;
      }
      return state.queryRaw ?? JSON.stringify({ value: state.values[kind] });
    },

    powerMonitorCapabilities() {
      if (state.capsError) {
        throw state.capsError;
      }
      return JSON.stringify(state.capabilities);
    },

    powerMonitorStart(epoch) {
      state.starts.push(epoch);
      state.onStart?.(epoch);
    },

    powerMonitorStop(epoch) {
      state.stops.push(epoch);
      state.onStop?.(epoch);
    },

    exit(code) {
      state.exits.push(code);
    }
  };
  const api = createApi(native, EventEmitter, path, urls);
  return {
    api,
    monitor: api.powerMonitor,
    state,

    ready() {
      state.handler(0, 'ready', '');
    },

    event(epoch, type, extra = {}) {
      state.handler(0, 'power-monitor-event', JSON.stringify({ epoch, type, ...extra }));
    }
  };
}

const first = fixture();
const monitor = first.monitor;
check(monitor instanceof EventEmitter, 'Public singleton EventEmitter');
for (const action of [
  () => monitor.getSystemIdleTime(),
  () => monitor.getSystemIdleState(5),
  () => monitor.getCurrentThermalState(),
  () => monitor.isOnBatteryPower(),
  () => monitor.onBatteryPower
]) {
  fails(action, 'ERR_FIREFLY_NOT_READY');
}
check(
  first.state.queries.length === 0 && first.state.starts.length === 0,
  'Pre-ready queries never reach native'
);
first.ready();
await first.api.app.whenReady();
check(
  monitor.getSystemIdleTime() === 7 && monitor.getSystemIdleState(5) === 'idle',
  'Controlled actual query values'
);
check(first.state.queries.at(-1).threshold === 5, 'Idle threshold forwarded exactly');
for (const value of ['active', 'idle', 'locked', 'unknown']) {
  first.state.values['idle-state'] = value;
  check(monitor.getSystemIdleState(1) === value, 'Idle state enum');
}
for (const value of ['unknown', 'nominal', 'fair', 'serious', 'critical']) {
  first.state.values['thermal-state'] = value;
  check(monitor.getCurrentThermalState() === value, 'Thermal state enum');
}
for (const value of [true, false]) {
  first.state.values['battery-power'] = value;
  check(
    monitor.isOnBatteryPower() === value && monitor.onBatteryPower === value,
    'Battery query and readonly projection'
  );
}
let assignment;
try {
  monitor.onBatteryPower = true;
} catch (error) {
  assignment = error.name;
}
check(assignment === 'TypeError', 'Battery property is readonly');
const beforeInvalidThreshold = first.state.queries.length;
for (const value of [undefined, null, '1', {}, 1n]) {
  fails(() => monitor.getSystemIdleState(value), 'ERR_INVALID_ARG_TYPE');
}
for (const value of [0, -1, 0.5, NaN, Infinity, 2147483648]) {
  fails(() => monitor.getSystemIdleState(value), ['ERR_INVALID_ARG_TYPE', 'ERR_OUT_OF_RANGE']);
}
check(
  first.state.queries.length === beforeInvalidThreshold,
  'Invalid idle thresholds never enter the native provider'
);
check(first.state.starts.length === 0, 'Queries do not register event observers');
console.log('PASS power queries, readiness, enum values and readonly battery property');

for (const [kind, call, invalidValues] of [
  [
    'idle-time',
    () => monitor.getSystemIdleTime(),
    [-1, 0.5, Infinity, Number.MAX_SAFE_INTEGER + 1, '7', null]
  ],
  ['idle-state', () => monitor.getSystemIdleState(1), ['invalid', 0, null]],
  ['thermal-state', () => monitor.getCurrentThermalState(), ['invalid', 100, null]],
  ['battery-power', () => monitor.isOnBatteryPower(), [null, 0, 1, 'false']]
]) {
  const previous = first.state.values[kind];
  for (const value of invalidValues) {
    first.state.values[kind] = value;
    fails(call, dataError);
  }
  first.state.values[kind] = previous;
}
first.state.queryRaw = '{invalid JSON';

fails(() => monitor.isOnBatteryPower(), dataError);

first.state.queryRaw = null;
first.state.queryError = Object.assign(Error('controlled OS query failure'), {
  code: 'ERR_TEST_POWER_QUERY'
});
for (const call of [
  () => monitor.getSystemIdleTime(),
  () => monitor.isOnBatteryPower(),
  () => monitor.getCurrentThermalState()
]) {
  fails(call, 'ERR_TEST_POWER_QUERY');
}
first.state.queryError = null;
const queryExit = fixture();
queryExit.ready();

queryExit.state.onQuery = () => queryExit.api.app.exit(0);

fails(() => queryExit.monitor.getSystemIdleTime(), 'ERR_FIREFLY_APP_EXITED');

console.log('PASS power query failures and malformed data never become fabricated fallback values');

const watching = fixture();
const pm = watching.monitor;
let meta = 0;

pm.on('newListener', () => ++meta);

pm.on('removeListener', () => ++meta);

pm.on('error', () => {});

check(watching.state.starts.length === 0, 'Meta listeners do not activate observer');
const resumed = [];

const resume = (...args) => resumed.push(args);

pm.on('resume', resume);
check(watching.state.starts.length === 0, 'Pre-ready subscription does not activate observer');

fails(() => pm.on('not-a-power-event', () => {}), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => pm.on('resume', 1), 'ERR_INVALID_ARG_TYPE');

watching.ready();
const epoch = watching.state.starts[0];
check(
  Number.isInteger(epoch) && epoch > 0 && watching.state.starts.length === 1,
  'Ready starts exactly one observer epoch'
);

const extra = () => {};

pm.prependListener('on-ac', extra);
pm.once('suspend', extra);
pm.prependOnceListener('unlock-screen', extra);
pm.addListener('lock-screen', extra);
check(watching.state.starts.length === 1, 'Listener aliases share one native observer');
watching.event(epoch, 'resume');
check(resumed.length === 1 && resumed[0].length === 0, 'Lifecycle event has no invented payload');
pm.removeAllListeners('on-ac');
pm.removeAllListeners('suspend');
pm.removeAllListeners('unlock-screen');
pm.removeAllListeners('lock-screen');
check(watching.state.stops.length === 0, 'Remaining native listener retains observer');
pm.removeListener('resume', resume);
check(
  watching.state.stops.length === 1 && watching.state.stops[0] === epoch,
  'Removing last native listener stops its epoch'
);
pm.on('resume', resume);
const nextEpoch = watching.state.starts.at(-1);
check(nextEpoch > epoch, 'New subscription has fresh epoch');
watching.event(epoch, 'resume');
watching.event(epoch, 'malformed-stale-event', { limit: 'invalid' });
watching.event(nextEpoch, 'resume');
check(resumed.length === 2, 'Old epoch events cannot reach new listeners');
check(meta > 0, 'Normal EventEmitter meta events remain available');
const limitedEvents = fixture();
limitedEvents.state.capabilities = ['resume'];

fails(() => limitedEvents.monitor.on('on-battery', () => {}), 'ERR_FIREFLY_UNSUPPORTED_API');

check(
  limitedEvents.state.starts.length === 0 &&
    limitedEvents.monitor.listenerCount('on-battery') === 0,
  'Unavailable event registration does not claim success'
);
const badCapabilities = fixture();
badCapabilities.state.capabilities = ['resume', 'invented-event'];

fails(() => badCapabilities.monitor.on('resume', () => {}), dataError);

check(
  badCapabilities.state.starts.length === 0,
  'Malformed capability data cannot start an observer'
);
const failedCapabilities = fixture();
failedCapabilities.state.capsError = Object.assign(Error('controlled capabilities failure'), {
  code: 'ERR_TEST_POWER_CAPS'
});

fails(() => failedCapabilities.monitor.on('resume', () => {}), 'ERR_TEST_POWER_CAPS');

pm.removeAllListeners();
const allRemovedEpoch = watching.state.starts.at(-1);
check(
  watching.state.stops.includes(allRemovedEpoch),
  'removeAllListeners disposes the current native observer'
);
pm.on('resume', resume);
check(
  watching.state.starts.at(-1) > allRemovedEpoch,
  'Native monitoring survives removal of all ordinary and meta listeners'
);
const onceOnly = fixture();
onceOnly.ready();
let onceCount = 0;

onceOnly.monitor.once('suspend', () => ++onceCount);

const onceEpoch = onceOnly.state.starts[0];
onceOnly.event(onceEpoch, 'suspend');
onceOnly.event(onceEpoch, 'suspend');
check(
  onceCount === 1 && onceOnly.state.stops.includes(onceEpoch),
  'Once-only listener fires and releases observer exactly once'
);
console.log('PASS power listener capabilities, aliases and native observer epoch lifetime');

const details = fixture();
details.ready();
const received = [];
for (const name of eventNames) {
  details.monitor.on(name, (...args) => {
    if (name === 'shutdown') {
      check(args.length === 1 && args[0].defaultPrevented === false, 'Passive shutdown event');

      fails(() => args[0].preventDefault(), 'ERR_FIREFLY_UNSUPPORTED_API');

      check(args[0].defaultPrevented === false, 'Unsupported prevention never claims OS delay');
      received.push([name]);
    } else {
      received.push([name, args]);
    }
  });
}
const detailEpoch = details.state.starts[0];
for (const name of eventNames) {
  details.event(
    detailEpoch,
    name,
    name === 'thermal-state-change'
      ? { state: 'serious' }
      : name === 'speed-limit-change'
        ? { limit: 73 }
        : {}
  );
}
check(received.length === eventNames.length, 'Supported packet names dispatch once');
check(
  JSON.stringify(received.find(([name]) => name === 'thermal-state-change')[1]) ===
    '[{"state":"serious"}]',
  'Thermal event detail shape'
);
check(
  JSON.stringify(received.find(([name]) => name === 'speed-limit-change')[1]) === '[{"limit":73}]',
  'Speed event detail shape'
);
for (const [type, extra] of [
  ['unknown-event', {}],
  ['thermal-state-change', { state: 'invalid' }],
  ['speed-limit-change', { limit: -1 }],
  ['speed-limit-change', { limit: 101 }],
  ['speed-limit-change', { limit: 0.5 }],
  ['speed-limit-change', { limit: '100' }]
]) {
  fails(() => details.event(detailEpoch, type, extra), dataError);
}
check(
  received.length === eventNames.length,
  'Malformed packets do not fabricate valid detail events'
);
console.log('PASS power packet validation, event details and explicit passive shutdown semantics');

const broken = fixture();

broken.monitor.on('newListener', () => {});

broken.monitor.on('resume', () => {});

broken.monitor.on('on-ac', () => {});

broken.state.onStart = () => {
  throw Object.assign(Error('controlled observer start failure'), { code: 'ERR_TEST_POWER_START' });
};

fails(() => broken.ready(), 'ERR_TEST_POWER_START');

check(
  broken.monitor.listenerCount('resume') === 0 && broken.monitor.listenerCount('on-ac') === 0,
  'Start failure rolls back all native listeners'
);
check(
  broken.monitor.listenerCount('newListener') === 1,
  'Native rollback preserves meta listeners'
);
check(
  broken.monitor.getSystemIdleTime() === 7,
  'Queries remain independent of failed observer startup'
);
const startExit = fixture();
startExit.ready();

startExit.state.onStart = () => startExit.api.app.exit(0);

fails(() => startExit.monitor.on('resume', () => {}), 'ERR_FIREFLY_APP_EXITED');

check(
  startExit.state.stops.includes(startExit.state.starts[0]),
  'Reentrant startup exit cancels admitted epoch'
);
const packetExit = fixture();
packetExit.ready();
let calls = 0;

packetExit.monitor.on('resume', () => {
  ++calls;
  packetExit.api.app.exit(0);
});

const lastEpoch = packetExit.state.starts[0];
packetExit.event(lastEpoch, 'resume');
packetExit.event(lastEpoch, 'resume');
check(
  calls === 1 && packetExit.state.stops.includes(lastEpoch),
  'Event listener shutdown disposes observer and ignores late packets'
);

fails(() => packetExit.monitor.getSystemIdleTime(), 'ERR_FIREFLY_APP_EXITED');

fails(() => packetExit.monitor.on('resume', () => {}), 'ERR_FIREFLY_APP_EXITED');

const stopReentry = fixture();
stopReentry.ready();

const originalListener = () => {};

let replacementCalls = 0;
stopReentry.monitor.on('resume', originalListener);
const retiredEpoch = stopReentry.state.starts[0];

stopReentry.state.onStop = () => {
  stopReentry.state.onStop = null;

  stopReentry.monitor.on('resume', () => ++replacementCalls);
};

stopReentry.monitor.removeListener('resume', originalListener);
const replacementEpoch = stopReentry.state.starts.at(-1);
check(
  replacementEpoch > retiredEpoch && stopReentry.state.stops.join(',') === String(retiredEpoch),
  'Old stop cannot revoke a reentrant new subscription'
);
stopReentry.event(retiredEpoch, 'resume');
stopReentry.event(replacementEpoch, 'resume');
check(replacementCalls === 1, 'Reentrant observer replacement routes only its fresh epoch');
console.log('PASS power start failure rollback, reentrant exit and disposed-epoch suppression');
const failingStop = fixture();
failingStop.ready();

failingStop.monitor.on('resume', () => {
  throw Error('Stale shutdown callback');
});

const failingEpoch = failingStop.state.starts[0];

failingStop.state.onStop = () => {
  throw Error('Controlled native cleanup failure');
};

failingStop.api.app.exit(7);
failingStop.event(failingEpoch, 'resume');
check(
  failingStop.state.exits.length === 1 &&
    failingStop.state.exits[0] === 7 &&
    failingStop.monitor.listenerCount('resume') === 0,
  'Native cleanup failure cannot prevent app exit or retain native listeners'
);
console.log('PASS power shutdown remains final after a native stop failure');
const cycling = fixture();
cycling.ready();

cycling.state.onStart = () => cycling.monitor.removeAllListeners('resume');

cycling.state.onStop = () => cycling.monitor.on('resume', () => {});

fails(() => cycling.monitor.on('resume', () => {}), 'ERR_FIREFLY_POWER_MONITOR_REENTRY');

check(
  cycling.state.starts.length === 16 &&
    cycling.state.stops.length === 16 &&
    cycling.monitor.listenerCount('resume') === 0,
  'Exhausted reentry retires the epoch and clears inert subscriptions'
);
cycling.state.onStart = null;
cycling.state.onStop = null;
let recoveredCalls = 0;

cycling.monitor.on('resume', () => ++recoveredCalls);

cycling.event(cycling.state.starts.at(-1), 'resume');
check(recoveredCalls === 1, 'A fresh subscription can recover after bounded reentry');
console.log('PASS power transition exhaustion cleans up and permits a fresh epoch');
console.log('DONE power monitor policy');
