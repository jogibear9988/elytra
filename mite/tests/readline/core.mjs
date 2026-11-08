/** Independent expectations for real stream framing and interface lifecycle. */
import { createInterface, Interface } from 'node:readline';
import { PassThrough, Readable, Writable } from 'node:stream';
import { createRequire } from 'node:module';
import { setTimeout as sleep } from 'node:timers/promises';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

check(
  createRequire(import.meta.url)('readline').createInterface === createInterface,
  'CJS/ESM identity'
);

async function lines(chunks, options = {}) {
  const input = new PassThrough();
  const rl = createInterface({ input, ...options });
  check(rl instanceof Interface, 'Interface class');
  const result = (async () => {
    const all = [];
    for await (const line of rl) {
      all.push(line);
    }
    return all;
  })();
  for (const chunk of chunks) {
    input.write(chunk);
  }
  input.end();
  return result;
}

const cases = [
  [[], []],
  [['a\nb\r\nc\rd'], ['a', 'b', 'c', 'd']],
  [['\n\r\n\r'], ['', '', '']],
  [
    ['a\r', '\n', 'b\n', 'tail'],
    ['a', 'b', 'tail']
  ],
  [
    [Buffer.from([0xc3]), Buffer.from([0xbc, 10, 0xf0, 0x9f]), Buffer.from([0x98, 0x80])],
    ['ü', '😀']
  ],
  [[Buffer.from([0xc3])], []]
];
for (const [chunks, expected] of cases) {
  const actual = await lines(chunks, { crlfDelay: Infinity });
  check(
    JSON.stringify(actual) === JSON.stringify(expected),
    'framed ' + JSON.stringify(chunks) + ' -> ' + JSON.stringify(actual)
  );
}
// CRLF delay is observable across separate input chunks.
const input = new PassThrough();
const rl = createInterface({ input, crlfDelay: 100 });
const delayed = [];

rl.on('line', (line) => delayed.push(line));

const closed = new Promise((resolve) => rl.once('close', resolve));
input.write('one\r');
await sleep(120);
input.end('\ntwo');
await closed;
check(JSON.stringify(delayed) === JSON.stringify(['one', '', 'two']), 'delayed CRLF');
const lifecycleInput = new PassThrough();
const lifecycle = createInterface({ input: lifecycleInput });
const events = [];

lifecycle.on('pause', () => events.push('pause'));

lifecycle.on('resume', () => events.push('resume'));

lifecycle.on('close', () => events.push('close'));

check(lifecycle.pause() === lifecycle && lifecycle.resume() === lifecycle, 'fluent lifecycle');
lifecycle.close();
lifecycle.close();
check(
  JSON.stringify(events) === JSON.stringify(['pause', 'resume', 'pause', 'close']),
  'lifecycle events ' + events
);
check(
  lifecycleInput.listenerCount('data') === 0 && lifecycleInput.listenerCount('end') === 0,
  'detached listeners'
);
const queryInput = new PassThrough();
let printed = '';
const output = new Writable({
  write(chunk, encoding, callback) {
    printed += chunk.toString();
    callback();
  }
});
const query = createInterface({ input: queryInput, output, terminal: false });
query.setPrompt('> ');
check(query.getPrompt() === '> ', 'prompt');
query.prompt();
const answer = new Promise((resolve) => query.question('Question? ', resolve));
queryInput.write('answer\n');
check((await answer) === 'answer', 'question answer');
query.close();
check(printed === '> Question? ', 'real prompt writes');
const controller = new AbortController();
const abortInput = new PassThrough();
const aborted = createInterface({ input: abortInput, signal: controller.signal });
const abortClosed = new Promise((resolve) => aborted.once('close', resolve));
controller.abort();
await abortClosed;
check(abortInput.listenerCount('data') === 0, 'abort cleanup');
// Demand drives a long single input chunk without accumulating every line.
const huge = 'x\n'.repeat(10000);
const pressureInput = Readable.from([Buffer.from(huge)]);
const pressure = createInterface({ input: pressureInput, crlfDelay: Infinity });
let count = 0;
for await (const line of pressure) {
  check(line === 'x', 'pressure text');
  count++;
}
check(count === 10000, 'long batch with iterator pressure');
const breakInput = new PassThrough();
const breaking = createInterface({ input: breakInput });
const broken = (async () => {
  for await (const line of breaking) {
    check(line === 'first', 'break first');
    break;
  }
})();
breakInput.write('first\nsecond\n');
await broken;
// Node 24.15 keeps the interface's input listener after iterator return;
// Mite retires it immediately. Keep the observed difference explicit.
if (process.versions.mite) {
  check(breakInput.listenerCount('data') === 0 && breaking.closed, 'iterator return cleanup');
} else {
  check(
    breakInput.listenerCount('data') === 1 && !breaking.closed,
    'Node iterator return reference'
  );
}
breaking.close();
check(breakInput.listenerCount('data') === 0, 'explicit close cleanup');
console.log(
  'PASS readline UTF-8/CRLF/EOF, delay, events, question, abort and bounded async line iteration'
);
