/** Genuine filesystem polling, shared registrations and deletion/recreation history. */
import fs from 'node:fs';
import { EventEmitter } from 'node:events';
import { pathToFileURL } from 'node:url';
import path from 'node:path';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const turn = (ms = 5) => new Promise((resolve) => setTimeout(resolve, ms));

async function wait(test, message) {
  const deadline = Date.now() + 2500;
  while (!test() && Date.now() < deadline) {
    await turn();
  }
  check(test(), message);
}

const file = path.resolve('watched.txt');
fs.writeFileSync(file, 'alpha');
const events = [];
let others = 0;

const primary = (curr, prev) => events.push({ curr, prev });

const secondary = () => others++;

const watcher = fs.watchFile(pathToFileURL(file), { interval: 20, persistent: false }, primary);
const shared = fs.watchFile(file, { interval: 5007, persistent: true, bigint: true }, secondary);
check(
  shared === watcher &&
    watcher instanceof EventEmitter &&
    watcher.constructor.name === 'StatWatcher',
  'One real StatWatcher per normalized path'
);
check(watcher.ref() === watcher && watcher.unref() === watcher, 'Chainable watcher references');
await turn(100);
check(
  events.length === 0 && others === 0,
  'Existing file initial stat creates baseline without event'
);
fs.writeFileSync(file, 'beta-XYZ');
await wait(() => events.length > 0 && others > 0, 'First interval wins over later interval');
const changed = events.at(-1);
check(
  changed.curr instanceof fs.Stats && typeof changed.curr.size === 'number',
  'First bigint:false determines shared Stats type'
);
check(
  changed.curr.isFile() && changed.curr.size === 8 && changed.prev.size === 5,
  'Real changed stats'
);
fs.unlinkSync(file);
await wait(() => events.at(-1).curr.ino === 0, 'Deletion reports zero snapshot');
const deleted = events.at(-1);
check(
  deleted.curr.ctimeMs === 0 && deleted.curr.mtime.getTime() === 0 && !deleted.curr.isFile(),
  'Missing stats use zero fields/epoch dates'
);
check(deleted.prev.size === 8, 'Deletion retains latest real previous snapshot');
fs.writeFileSync(file, 'new');
await wait(
  () => events.at(-1).curr.ino !== 0 && events.at(-1).curr.size === 3,
  'Recreation observed'
);
const recreated = events.at(-1);
check(
  recreated.prev.ino === deleted.prev.ino &&
    recreated.prev.size === deleted.prev.size &&
    recreated.prev.mtimeMs === deleted.prev.mtimeMs,
  'Recreation previous equals last real snapshot before deletion'
);
const beforeUnknown = watcher.listenerCount('change');

fs.unwatchFile(file, () => {});

check(watcher.listenerCount('change') === beforeUnknown, 'Unknown callback removal is a no-op');
fs.unwatchFile(file, primary);
check(watcher.listenerCount('change') === 1, 'Specific unsubscribe preserves other observer');
const firstCount = events.length;
const secondCount = others;
fs.writeFileSync(file, 'longer-data');
await wait(() => others > secondCount, 'Remaining callback stays active');
check(events.length === firstCount, 'Removed callback receives no new polling changes');
let stops = 0;

watcher.on('stop', () => stops++);

fs.unwatchFile(file);
check(
  stops === 0 && watcher.listenerCount('change') === 0,
  'Last unsubscribe clears callbacks and defers stop'
);
await turn();
check(stops === 1, 'One asynchronous stop notification');
fs.unwatchFile(file);
fs.unwatchFile('unknown-watch-file');
check(stops === 1, 'Repeated/unknown unwatch is a no-op');
console.log(
  JSON.stringify({
    existingBaseline: true,
    firstOptions: true,
    changed: true,
    missing: true,
    recreationHistory: true,
    removed: true,
    shared: true,
    stop: stops
  })
);
