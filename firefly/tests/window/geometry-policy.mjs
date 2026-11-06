/** Real BrowserWindow geometry API against an isolated native-provider fixture; no GUI. */
import createApi from '../../core/api.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import * as urls from 'node:url';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

function equal(actual, expected, message) {
  const normalize = (value) =>
    Array.isArray(value) ? value : Object.fromEntries(Object.entries(value).sort());

  check(
    JSON.stringify(normalize(actual)) === JSON.stringify(normalize(expected)),
    message + ': ' + JSON.stringify(actual)
  );
}

function fails(action, expected) {
  let code;
  try {
    action();
  } catch (error) {
    code = error.code;
  }
  check(code === expected, 'expected ' + expected);
}

let handler;
let next = 0;
let writes = 0;
const windows = new Map();
const native = {
  appPath: process.cwd(),

  setHandler(value) {
    handler = value;
  },

  create(width, height, title, show, frame, preload, content, x, y) {
    const margins = frame
      ? { left: 4, top: 28, right: 4, bottom: 4 }
      : { left: 0, top: 0, right: 0, bottom: 0 };
    const rect = {
      x: x ?? 100,
      y: y ?? 100,
      width: width + (content ? margins.left + margins.right : 0),
      height: height + (content ? margins.top + margins.bottom : 0)
    };
    windows.set(++next, { rect, margins });
    return next;
  },

  windowBounds(id, content) {
    const { rect, margins: m } = windows.get(id);
    return JSON.stringify(
      content
        ? {
            x: rect.x + m.left,
            y: rect.y + m.top,
            width: rect.width - m.left - m.right,
            height: rect.height - m.top - m.bottom
          }
        : rect
    );
  },

  setWindowBounds(id, x, y, width, height, content) {
    const entry = windows.get(id);
    const m = entry.margins;
    entry.rect = content
      ? {
          x: x - m.left,
          y: y - m.top,
          width: width + m.left + m.right,
          height: height + m.top + m.bottom
        }
      : { x, y, width, height };
    writes++;
  },

  centerWindow(id) {
    // This fixture tests delegation; real AppKit positioning has a separate desktop gate.
    windows.get(id).rect.x = -700;
    windows.get(id).rect.y = 70;
    writes++;
  },

  destroy() {}
};
const { app, BrowserWindow } = createApi(native, EventEmitter, path, urls);
handler(0, 'ready', '');
await app.whenReady();
const window = new BrowserWindow({ x: -800, y: -100, width: 640, height: 480, show: false });
check(window instanceof EventEmitter, 'geometry base preserves emitter');
equal(window.getBounds(), { x: -800, y: -100, width: 640, height: 480 }, 'outer constructor');
equal(
  window.getContentBounds(),
  { x: -796, y: -72, width: 632, height: 448 },
  'native frame margins'
);
equal(window.getSize(), [640, 480], 'outer size');
equal(window.getContentSize(), [632, 448], 'content size');
equal(window.getPosition(), [-800, -100], 'negative DIP position');
const record = window.getBounds();
record.width = 1;
check(window.getSize()[0] === 640, 'native snapshot ownership');
window.setBounds({ x: -700, width: 600 });
equal(
  window.getBounds(),
  { x: -700, y: -100, width: 600, height: 480 },
  'partial bounds preserve fields'
);
window.setContentSize(500, 300);
equal(window.getContentSize(), [500, 300], 'content size setter');
equal(window.getSize(), [508, 332], 'outer includes frame');
window.setContentBounds({ x: -600, y: 0, width: 500, height: 300 });
equal(window.getPosition(), [-604, -28], 'content origin to frame origin');
window.setSize(400, 240);
window.setPosition(-900, -100);
window.center();
equal(window.getPosition(), [-700, 70], 'native center delegation');
console.log('PASS geometry outer/content snapshots partial setters and native center delegation');

const content = new BrowserWindow({ useContentSize: true, width: 400, height: 300 });
equal(content.getContentSize(), [400, 300], 'content-sized constructor');
const frameless = new BrowserWindow({ frame: false, width: 400, height: 300 });
equal(frameless.getBounds(), frameless.getContentBounds(), 'frameless content equals bounds');
let moved = 0;

window.on('move', () => {
  moved++;
});

handler(window.id, 'move', '');
check(moved === 1, 'queued native move event');
console.log('PASS geometry content-sized frameless constructors and move events');

const count = writes;
for (const action of [
  () => window.setPosition(1.2, 1),
  () => window.setBounds(null),
  () => window.setBounds({ x: NaN }),
  () => new BrowserWindow({ useContentSize: 'yes' }),
  () => new BrowserWindow({ x: 2147483648 })
]) {
  fails(action, 'ERR_INVALID_ARG_TYPE');
}

fails(() => window.setBounds({ width: 0 }), 'ERR_INVALID_ARG_VALUE');

fails(() => window.setContentBounds({ width: 500 }), 'ERR_INVALID_ARG_VALUE');

fails(() => window.setBounds({ x: 2147483647, width: 2 }), 'ERR_OUT_OF_RANGE');

fails(() => new BrowserWindow({ x: 2147483647, width: 2 }), 'ERR_OUT_OF_RANGE');

fails(() => window.setBounds({ unknown: true }), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => window.setPosition(1, 2, true), 'ERR_FIREFLY_UNSUPPORTED_API');

check(writes === count && next === 3, 'invalid requests do not reach native mutation');
window.destroy();
handler(window.id, 'closed', '');
for (const action of [
  () => window.getBounds(),
  () => window.getContentSize(),
  () => window.setPosition(1, 2),
  () => window.center()
]) {
  fails(action, 'ERR_FIREFLY_OBJECT_DESTROYED');
}
console.log('PASS geometry validation overflow unsupported animation and destroyed guards');
console.log('DONE geometry policy');
