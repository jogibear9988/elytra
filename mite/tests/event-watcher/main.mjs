/** Genuine OS event watching; assert eventual behavior without inventing backend event counts. */
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';
import { EventEmitter } from 'node:events';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const sleep = (ms = 5) => new Promise((resolve) => setTimeout(resolve, ms));

async function wait(test, message, diagnostics) {
  const deadline = Date.now() + 3500;
  while (!test() && Date.now() < deadline) {
    await sleep();
  }
  check(
    test(),
    diagnostics ? `${message}; native events=${JSON.stringify(diagnostics())}` : message
  );
}

const directory = path.resolve('events');
fs.mkdirSync(directory);
const initial = path.join(directory, 'initial.txt');
fs.writeFileSync(initial, 'baseline');
const events = [];
const watcher = fs.watch(pathToFileURL(directory), { persistent: false }, (kind, name) => {
  check(kind === 'change' || kind === 'rename', 'Native event type');
  check(name === null || typeof name === 'string', 'Default native filename encoding');
  events.push({ kind, name });
});

watcher.on('error', (error) => {
  throw error;
});

check(
  watcher instanceof EventEmitter && watcher.constructor.name === 'FSWatcher',
  'Genuine FSWatcher'
);
check(watcher.ref() === watcher && watcher.unref() === watcher, 'Chainable native references');
// Let the newly registered OS directory stream enter its owner loop before mutation.
await sleep(50);
fs.appendFileSync(initial, '-changed');
await wait(
  () => events.some((event) => event.name === null || event.name === 'initial.txt'),
  'Real directory append produces a native target event',
  () => events
);
fs.writeFileSync(path.join(directory, 'created.txt'), 'created');
await wait(
  () =>
    events.some(
      (event) => event.kind === 'rename' && (event.name === null || event.name === 'created.txt')
    ),
  'Real creation produces rename-style event'
);
const beforeRename = events.length;
fs.renameSync(path.join(directory, 'created.txt'), path.join(directory, 'renamed.txt'));
await wait(
  () => events.slice(beforeRename).some((event) => event.kind === 'rename'),
  'Real rename observed'
);
let closed = 0;

watcher.on('close', () => closed++);

check(watcher.close() === undefined && closed === 0, 'Close is asynchronous');
watcher.close();
await wait(() => closed === 1, 'Close emitted once');
const afterClose = events.length;
fs.appendFileSync(initial, '-after-close');
await sleep(60);
check(events.length === afterClose && closed === 1, 'Closed watcher receives no later events');

// File and directory registrations both deliver backend-defined change/rename masks.
let fileObserved = false;
const direct = fs.watch(initial, { persistent: false }, (kind) => {
  check(kind === 'change' || kind === 'rename', 'Direct native file event type');
  fileObserved = true;
});

direct.on('error', (error) => {
  throw error;
});

// Give the newly registered OS stream an owner-loop turn before mutation.
await sleep(50);
fs.appendFileSync(initial, '-direct-change');
await wait(() => fileObserved, 'Direct file append produces actual native event');
direct.close();

let binaryNames = 0;
const binary = fs.watch(
  Buffer.from(directory),
  { encoding: 'buffer', persistent: false },
  (kind, name) => {
    check(kind === 'change' || kind === 'rename', 'Buffer watcher native event type');
    if (name !== null) {
      check(Buffer.isBuffer(name), 'Owned byte filename');
      if (name.toString() === 'initial.txt') {
        binaryNames++;
      }
      name.fill(0); // Later native events must own fresh filename bytes.
    }
  }
);

binary.on('error', (error) => {
  throw error;
});

await sleep(50);
fs.appendFileSync(initial, '-buffer-one');
await wait(() => binaryNames > 0, 'Buffer path/encoding actual target event');
const beforeSecond = binaryNames;
fs.appendFileSync(initial, '-buffer-two');
await wait(() => binaryNames > beforeSecond, 'Subsequent native target filename snapshot');
binary.close();
check(binaryNames >= 2, 'Ordinary file changes exercise fresh byte filename snapshots');
console.log(
  JSON.stringify({
    append: true,
    create: true,
    rename: true,
    typedNames: true,
    bufferSnapshots: true,
    close: true
  })
);
