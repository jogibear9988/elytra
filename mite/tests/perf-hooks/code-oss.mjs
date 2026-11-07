/** Execute the actual type-stripped pinned Code-OSS module supplied by the test runner. */
import { performance } from 'node:perf_hooks';
import { pathToFileURL } from 'node:url';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

const source = pathToFileURL(process.argv[2]).href;
const upstream = await import(source);
const origin = upstream.getMarks();
check(origin.length === 1 && origin[0].name === 'code/timeOrigin', 'actual upstream origin mark');
check(origin[0].startTime === performance.timeOrigin, 'actual upstream uses native process origin');
upstream.mark('code/willLoadMainBundle', { startTime: Math.floor(performance.timeOrigin) });
const before = Date.now();
upstream.mark('code/didLoadMainBundle');
const after = Date.now();
const marks = upstream.getMarks();
check(marks.length === 3, 'actual upstream startup marks');
check(
  marks[1].startTime === Math.floor(performance.timeOrigin),
  'actual upstream explicit timestamp'
);
check(
  before <= marks[2].startTime && marks[2].startTime <= after,
  'actual upstream current timestamp'
);
upstream.clearMarks('code/willLoadMainBundle');
check(upstream.getMarks().length === 2, 'actual upstream named clearing');
upstream.clearMarks();
check(upstream.getMarks().length === 1, 'actual upstream retains origin on clear');
check(
  upstream.getMarks()[0].startTime === performance.timeOrigin,
  'actual upstream retained origin'
);
check((await import(source)) === upstream, 'actual module cache identity');
console.log('PASS pinned actual Code-OSS performance module origin/startup marks/clearing');
