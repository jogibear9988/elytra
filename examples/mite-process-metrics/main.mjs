/** Measure native work without confusing elapsed time with process-wide CPU usage. */
import { hrtime, uptime, cpuUsage } from 'node:process';
import { createHash } from 'node:crypto';

const start = hrtime.bigint();
const before = cpuUsage();
const digest = createHash('sha256').update(new Uint8Array(65536)).digest('hex');
console.log(
  JSON.stringify({
    digest,
    elapsedNanoseconds: (hrtime.bigint() - start).toString(),
    cpuMicroseconds: cpuUsage(before),
    uptimeSeconds: uptime()
  })
);
