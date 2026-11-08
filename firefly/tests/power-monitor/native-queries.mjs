/** Minimal ready component context; actual OS provider, no injected power transitions. */
import native from 'power-test';
import { EventEmitter } from 'node:events';
import createPowerMonitor from '../../core/power-monitor/index.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const failure = (code, message) => Object.assign(new Error(message), { code });

const component = createPowerMonitor(
  native,
  EventEmitter,
  () => true,
  () => false,
  failure,
  (name) => {
    throw failure('ERR_FIREFLY_UNSUPPORTED_API', name);
  }
);

native.setHandler((data) => component.dispatch(JSON.parse(data)));

const monitor = component.powerMonitor;
const capabilities = JSON.parse(native.powerMonitorCapabilities());
check(Array.isArray(capabilities), 'Actual host reports native capabilities');
const idle = monitor.getSystemIdleTime();
check(Number.isSafeInteger(idle) && idle >= 0, 'Real whole-second idle observation');
check(
  ['active', 'idle', 'locked', 'unknown'].includes(monitor.getSystemIdleState(60)),
  'Real session/idle observation'
);
check(
  ['unknown', 'nominal', 'fair', 'serious', 'critical'].includes(monitor.getCurrentThermalState()),
  'Real native thermal observation'
);
if (capabilities.includes('on-ac')) {
  check(typeof monitor.isOnBatteryPower() === 'boolean', 'Real native power source observation');
  check(typeof monitor.onBatteryPower === 'boolean', 'Actual battery getter path');
} else {
  let code;
  try {
    monitor.isOnBatteryPower();
  } catch (error) {
    code = error.code;
  }
  check(code === 'ERR_FIREFLY_POWER_UNAVAILABLE', 'Unavailable power source is explicit');
}

const listener = () => {};

for (const name of capabilities) {
  monitor.on(name, listener);
}
native.collect();
for (const name of capabilities) {
  check(monitor.listenerCount(name) === 1, 'Real passive observer listener retained');
}
monitor.removeAllListeners();
for (const name of capabilities) {
  check(monitor.listenerCount(name) === 0, 'Passive native generation retired');
}
// Reconcile a new epoch through the exact native Host/Provider, then revoke it.
if (capabilities.length) {
  monitor.once(capabilities[0], listener);
  native.collect();
  monitor.removeAllListeners(capabilities[0]);
}
component.stop();
let stopped;
try {
  monitor.getSystemIdleTime();
} catch (error) {
  stopped = error.code;
}
check(stopped === 'ERR_FIREFLY_APP_EXITED', 'Component teardown rejects future queries');
console.log(
  'PASS actual power factory/Host/OS queries and passive subscription epochs; physical transitions not exercised'
);
