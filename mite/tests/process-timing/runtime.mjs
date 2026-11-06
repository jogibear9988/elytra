/** Measure actual native clocks/CPU without depending on exact scheduling or wall-clock values. */
import current, { hrtime, uptime, cpuUsage } from 'node:process';
import { setTimeout as delay } from 'node:timers/promises';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

check(
  current === process &&
    hrtime === process.hrtime &&
    uptime === process.uptime &&
    cpuUsage === process.cpuUsage,
  'builtin and global identities'
);

const ns = (tuple) => BigInt(tuple[0]) * 1000000000n + BigInt(tuple[1]);

for (let i = 0; i < 100; i++) {
  const before = hrtime.bigint();
  const tuple = hrtime();
  const after = hrtime.bigint();
  check(
    Number.isInteger(tuple[0]) &&
      Number.isInteger(tuple[1]) &&
      tuple[0] >= 0 &&
      tuple[1] >= 0 &&
      tuple[1] < 1e9,
    'normalized native tuple'
  );
  check(before <= ns(tuple) && ns(tuple) <= after, 'exact nanosecond sample between BigInts');
}
const mutated = hrtime();
mutated[0] = -1;
mutated[1] = -1;
check(hrtime()[0] >= 0, 'fresh clock samples');
console.log('PASS native monotonic tuple BigInt precision and process export identities');

const oldDate = Date.now;

Date.now = () => {
  throw new Error('wall clock must not be used');
};

const start = hrtime();
const up = uptime();
check(Number.isFinite(up) && up > 0, 'native process uptime');
await delay(30);
const elapsed = ns(hrtime(start));
const upElapsed = uptime() - up;
check(elapsed >= 10000000n && elapsed < 10000000000n, 'native clock advances across timer');
check(
  upElapsed > 0 && Math.abs(upElapsed - Number(elapsed) / 1e9) < 0.5,
  'uptime and monotonic clock advance together'
);
Date.now = oldDate;
console.log('PASS native elapsed time fractional uptime and wall-clock independence');

const initial = cpuUsage();
check(
  Number.isInteger(initial.user) &&
    initial.user >= 0 &&
    Number.isInteger(initial.system) &&
    initial.system >= 0,
  'native CPU microseconds'
);
const until = hrtime.bigint() + 50000000n;
let checksum = 0;
do {
  for (let i = 0; i < 1000; i++) {
    checksum = (checksum + i) | 0;
  }
} while (hrtime.bigint() < until);
const used = cpuUsage(initial);
check(
  used.user >= 0 && used.system >= 0 && used.user + used.system > 0,
  'actual CPU work is accounted'
);
check(Number.isInteger(checksum), 'CPU work completed');
const future = cpuUsage({ user: Number.MAX_SAFE_INTEGER, system: Number.MAX_SAFE_INTEGER });
check(future.user < 0 && future.system < 0, 'valid future snapshots subtract without clamping');
const zero = cpuUsage({ user: 0, system: 0 });
check(
  zero.user >= initial.user && zero.system >= initial.system,
  'process CPU totals do not reset'
);
console.log('PASS native process CPU accounting microsecond deltas and snapshot ownership');

const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
if (collect) {
  const tuple = hrtime();
  const big = hrtime.bigint();
  const cpu = cpuUsage();
  collect();
  check(
    ns(hrtime(tuple)) >= 0n && hrtime.bigint() >= big && cpuUsage(cpu).user >= 0,
    'samples remain valid across actual collection'
  );
  console.log('PASS process timing samples survive actual GC');
}
console.log('DONE process timing runtime');
