/** Exercise the real API factory using deterministic native-provider records, without GUI. */
import createApi from '../../core/api.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import * as urls from 'node:url';
import { readFileSync } from 'node:fs';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

function fails(action, code) {
  let found;
  try {
    action();
  } catch (error) {
    found = error.code;
  }
  check(found === code, 'error ' + code);
}

function display(id, x, y, width, height, scaleFactor = 1) {
  return {
    id,
    bounds: { x, y, width, height },
    workArea: { x, y: y + 20, width, height: height - 20 },
    size: { width, height },
    workAreaSize: { width, height: height - 20 },
    scaleFactor,
    rotation: 0,
    internal: false,
    label: 'fixture'
  };
}

let data = {
  primaryId: 7,
  displays: [
    display(8, -800, 200, 800, 600),
    display(7, 0, 0, 1000, 1000, 2),
    display(9, 0, -600, 800, 600)
  ]
};
let handler;
let reads = 0;
const native = {
  appPath: process.cwd(),

  setHandler(value) {
    handler = value;
  },

  screenSnapshot() {
    reads++;
    return JSON.stringify(data);
  },

  cursorScreenPoint() {
    return JSON.stringify({ x: -200, y: 250 });
  }
};
const { app, screen } = createApi(native, EventEmitter, path, urls);
check(screen instanceof EventEmitter, 'screen EventEmitter');
for (const call of [
  () => screen.getAllDisplays(),
  () => screen.getPrimaryDisplay(),
  () => screen.getCursorScreenPoint(),
  () => screen.getDisplayNearestPoint({ x: 0, y: 0 }),
  () => screen.getDisplayMatching({ x: 0, y: 0, width: 1, height: 1 })
]) {
  fails(call, 'ERR_FIREFLY_NOT_READY');
}
check(reads === 0, 'pre-ready never queries OS');
const events = [];

screen.on('display-removed', (event, display) => {
  events.push(['removed', display.id]);
  display.bounds.x = 99999;
});

screen.on('display-added', (event, display) => events.push(['added', display.id]));

screen.on('display-metrics-changed', (event, display, fields) => {
  events.push(['changed', display.id, fields]);
  screen.getAllDisplays();
  display.bounds.width = 1;
});

handler(0, 'ready', '');
await app.whenReady();
check(
  app.isReady() && screen.getPrimaryDisplay().id === 7 && screen.getAllDisplays().length === 3,
  'ready snapshots'
);
const changed = screen.getAllDisplays();
changed[0].bounds.x = 99999;
check(screen.getAllDisplays()[0].bounds.x === -800, 'snapshot isolation');
check(JSON.stringify(screen.getCursorScreenPoint()) === '{"x":-200,"y":250}', 'cursor DIP');
console.log('PASS screen readiness ownership and native boundary');

for (const [x, y, expected] of [
  [10, 10, 7],
  [-200, 250, 8],
  [100, -200, 9],
  [-10000, 400, 8],
  [5000, 500, 7],
  [0, 400, 7]
]) {
  check(screen.getDisplayNearestPoint({ x, y }).id === expected, 'nearest display');
}
for (const [rect, expected] of [
  [{ x: -50, y: 300, width: 200, height: 100 }, 7],
  [{ x: -500, y: 300, width: 400, height: 100 }, 8],
  [{ x: 10, y: -200, width: 50, height: 250 }, 9],
  [{ x: 5000, y: 300, width: 10, height: 10 }, 7],
  [{ x: -100, y: 300, width: 0, height: 0 }, 8],
  [{ x: -50, y: 300, width: 100, height: 100 }, 7]
]) {
  check(screen.getDisplayMatching(rect).id === expected, 'matching display');
}
console.log('PASS screen negative-origin geometry intersection and ties');

for (const value of [undefined, {}, { x: NaN, y: 1 }, { x: 1.2, y: 2 }, { x: 2147483648, y: 2 }]) {
  fails(() => screen.getDisplayNearestPoint(value), 'ERR_INVALID_ARG_TYPE');
}

fails(() => screen.getDisplayMatching({ x: 0, y: 0, width: -1, height: 2 }), 'ERR_OUT_OF_RANGE');

fails(() => screen.screenToDipPoint({ x: 0, y: 0 }), 'ERR_FIREFLY_UNSUPPORTED_API');

console.log('PASS screen explicit validation and unavailable conversions');

data = {
  primaryId: 7,
  displays: [
    display(7, 0, 0, 1100, 1000, 1),
    display(9, 0, -600, 800, 600),
    display(10, 1100, 0, 1000, 800)
  ]
};
data.displays[0].rotation = 90;
handler(0, 'screen-changed', '');
check(
  JSON.stringify(events) ===
    '[["removed",8],["changed",7,["bounds","workArea","scaleFactor","rotation"]],["added",10]]',
  'display event sequence'
);
check(screen.getPrimaryDisplay().bounds.width === 1100, 'event argument isolation');
handler(0, 'screen-changed', '');
check(events.length === 3, 'unchanged event suppression');
console.log('PASS screen event snapshots reentry and unchanged suppression');

data = { primaryId: -1, displays: [] };
check(screen.getAllDisplays().length === 0, 'empty native layout');

fails(() => screen.getPrimaryDisplay(), 'ERR_FIREFLY_DISPLAY_UNAVAILABLE');

fails(() => screen.getCursorScreenPoint(), 'ERR_FIREFLY_DISPLAY_UNAVAILABLE');

console.log('PASS screen unavailable desktop is explicit');

if (process.argv[2]) {
  data = JSON.parse(readFileSync(process.argv[2], 'utf8'));
  const displays = screen.getAllDisplays();
  check(
    displays.length > 0 && screen.getPrimaryDisplay().id === data.primaryId,
    'actual OS snapshot through API'
  );
  for (const d of displays) {
    check(
      screen.getDisplayNearestPoint({
        x: d.bounds.x + Math.floor(d.bounds.width / 2),
        y: d.bounds.y + Math.floor(d.bounds.height / 2)
      }).id === d.id,
      'native display center'
    );
  }
  console.log('PASS native macOS display snapshots through Node/Mite API factory');
}
console.log('DONE screen policy');
