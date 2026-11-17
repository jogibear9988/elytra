/** Compare clock invariants, never scheduler-specific absolute values. */
import hooks, { performance } from 'node:perf_hooks';
import bare from 'perf_hooks';
import { createRequire, builtinModules, isBuiltin } from 'node:module';
import { setTimeout as delay } from 'node:timers/promises';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

const require = createRequire(import.meta.url);
check(
  performance === globalThis.performance && performance === hooks.performance,
  'global identity'
);
check(hooks === bare && hooks === require('node:perf_hooks'), 'module identity');
check(isBuiltin('perf_hooks') && builtinModules.includes('perf_hooks'), 'available inventory');
const origin = performance.timeOrigin;
check(Number.isFinite(origin) && origin > 1e12, 'native epoch milliseconds');
check(Math.abs(origin + performance.now() - Date.now()) < 2000, 'epoch/relative clock relation');
const before = process.uptime() * 1000;
const now = performance.now();
const after = process.uptime() * 1000;
check(now >= 0 && Number.isFinite(now), 'native relative milliseconds');
check(Number.isFinite(before) && before >= 0 && after >= before, 'native process uptime');
// Node samples performance_process_start during static initialization and
// node_start_time later in process initialization. Their absolute offsets have
// no five-millisecond guarantee. Mite deliberately shares one native origin.
if (typeof process.versions.mite === 'string') {
  check(before - 5 <= now && now <= after + 5, 'Mite clocks share their native runtime origin');
}
let last = now;
for (let i = 0; i < 1000; i++) {
  const next = performance.now();
  check(next >= last, 'monotonic clock');
  last = next;
}
console.log('PASS performance global/ESM/CommonJS identity and native clock origin');

const oldDate = Date.now;

Date.now = () => {
  throw new Error('JavaScript wall clock consulted');
};

const start = performance.now();
const uptimeStart = process.uptime() * 1000;
await delay(25);
const uptimeElapsed = process.uptime() * 1000 - uptimeStart;
const elapsed = performance.now() - start;
check(elapsed >= 10 && elapsed < 10000 && performance.timeOrigin === origin, 'elapsed native time');
// Nested samples compare elapsed intervals without assuming equal origins or
// scheduler latency. Both clocks must advance while the JS wall clock throws.
check(uptimeElapsed >= 10 && uptimeElapsed <= elapsed + 1, 'native uptime interval');
Date.now = oldDate;
console.log('PASS monotonic native milliseconds advance across timers without Date.now');

const retained = performance.now;
const collect =
  typeof collectHostGarbage === 'function'
    ? collectHostGarbage
    : typeof gc === 'function'
      ? gc
      : undefined;
if (collect) {
  collect();
}
check(retained.call(performance) >= last && performance.timeOrigin === origin, 'clock survives GC');
if (collect) {
  console.log('PASS performance clock survives actual GC');
}

globalThis.performance = {};
check(
  hooks.performance === performance && performance.now() >= last,
  'global replacement cannot change exports'
);
globalThis.performance = performance;
console.log('PASS global replacement leaves cached clock exports intact');
