/** Run the published ESM package and its dependency against the actual native BigInt clock. */
import timeSpan from 'time-span';
import { setTimeout as delay } from 'node:timers/promises';

const original = process.hrtime.bigint;
let calls = 0;

process.hrtime.bigint = () => {
  calls++;
  return original();
};

try {
  const end = timeSpan();
  await delay(15);
  const milliseconds = end();
  const rounded = end.rounded();
  const seconds = end.seconds();
  const nanos = end.nanoseconds();
  console.log(
    'span',
    milliseconds >= 5 && milliseconds < 10000,
    Number.isInteger(rounded),
    seconds >= milliseconds / 1000,
    typeof nanos === 'bigint',
    nanos >= BigInt(Math.floor(milliseconds * 1e6)),
    calls
  );
} finally {
  process.hrtime.bigint = original;
}
