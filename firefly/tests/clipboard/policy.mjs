/** Test the real API factory without accessing any operating-system clipboard. */
import createApi from '../../core/api.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import * as urls from 'node:url';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

async function rejects(action, code) {
  let found;
  try {
    await action();
  } catch (error) {
    found = error.code;
  }
  check(found === code, 'expected rejection ' + code);
}

let board = [];
let writes = 0;
let forcedFailure = false;
const native = {
  appPath: process.cwd(),

  setHandler() {},

  clipboardRequest(wire) {
    if (forcedFailure) {
      return JSON.stringify({
        error: { code: 'ERR_FIREFLY_CLIPBOARD_UNAVAILABLE', message: 'fixture' }
      });
    }
    const value = JSON.parse(wire);
    let result = {};
    if (value.op === 'write') {
      board = value.items;
      writes++;
    } else if (value.op === 'clear') {
      board = [];
      writes++;
    } else if (value.op === 'read') {
      result = { items: board };
    } else if (value.op === 'has') {
      result = { has: board.some((item) => item.some((entry) => entry.format === value.format)) };
    } else if (value.op === 'readText') {
      const text = board[0]?.find((entry) => entry.format === 'text/plain');
      result = { text: text ? Buffer.from(text.data, 'base64').toString('utf8') : '' };
    } else {
      throw new Error('unknown native operation');
    }
    return JSON.stringify(result);
  }
};
const { app, clipboard, ClipboardItem } = createApi(native, EventEmitter, path, urls);
check(!app.isReady() && clipboard.selection === undefined, 'macOS surface without readiness guard');
const empty = clipboard.readText();
check(empty instanceof Promise && (await empty) === '', 'asynchronous empty readText');
check(JSON.stringify(await clipboard.read()) === '[]', 'empty read');
await clipboard.writeText('Grüße 🪲\nline\0tail');
check((await clipboard.readText()) === 'Grüße 🪲\nline\0tail', 'unicode and NUL preservation');
check((await clipboard.has('text/plain')) && !(await clipboard.has('text/html')), 'MIME metadata');
console.log('PASS clipboard async text and main-process factory boundary');

const bytes = new Uint8Array([0, 255, 128, 42]);
const source = {
  'text/plain': 'plain',
  'text/html': Promise.resolve('<b>plain</b>'),
  'text/rtf': new Blob([bytes], { type: 'text/rtf' })
};
const item = new ClipboardItem(source);
source['text/plain'] = 'mutated';
item.types.pop();
check(item.types.length === 3, 'types cannot mutate backing record');
bytes.fill(0);
await clipboard.write([item]);
const [snapshot] = await clipboard.read();
check(snapshot instanceof ClipboardItem, 'read ClipboardItem identity');
clipboard.clear();
check((await (await snapshot.getType('text/plain')).text()) === 'plain', 'snapshot capture');
check((await (await snapshot.getType('text/html')).text()) === '<b>plain</b>', 'promised HTML');
check(
  JSON.stringify([...new Uint8Array(await (await snapshot.getType('text/rtf')).arrayBuffer())]) ===
    '[0,255,128,42]',
  'binary blob preservation'
);
check((await snapshot.getType('text/html')).type === 'text/html', 'Blob MIME metadata');
await rejects(() => snapshot.getType('missing'), 'ERR_INVALID_ARG_VALUE');
console.log('PASS clipboard MIME promises Blob bytes and immutable read snapshots');

await clipboard.writeText('keep');
let resolve;
const pending = new Promise((done) => {
  resolve = done;
});
const before = writes;
const prepared = clipboard.write([
  new ClipboardItem({ 'text/plain': pending, 'text/html': '<b>next</b>' })
]);
check(
  writes === before && (await clipboard.readText()) === 'keep',
  'pending data does not publish'
);
resolve('next');
await prepared;
check(writes === before + 1 && (await clipboard.readText()) === 'next', 'one native commit');

const count = writes;
for (const action of [
  () => clipboard.writeText(1),
  () => clipboard.write([{}]),
  () => clipboard.write([new ClipboardItem({ 'text/plain': 1 })]),
  () => clipboard.write([new ClipboardItem({ 'text/plain': 'prepared', 'text/html': {} })])
]) {
  await rejects(action, 'ERR_INVALID_ARG_TYPE');
}
await rejects(() => clipboard.write([item, item]), 'ERR_INVALID_ARG_VALUE');
await rejects(() => clipboard.has('image/png'), 'ERR_FIREFLY_UNSUPPORTED_API');
await rejects(
  () => clipboard.write([new ClipboardItem({ 'image/png': 'x' })]),
  'ERR_FIREFLY_UNSUPPORTED_API'
);
await rejects(
  () =>
    clipboard.write([
      new ClipboardItem({ 'text/plain': new Blob([new Uint8Array(8 * 1024 * 1024 + 1)]) })
    ]),
  'ERR_FIREFLY_CLIPBOARD_LIMIT'
);
check(
  writes === count && (await clipboard.readText()) === 'next',
  'failed preparation never publishes'
);
const reason = new Error('promised data rejected');
let caught;
try {
  await clipboard.write([new ClipboardItem({ 'text/plain': Promise.reject(reason) })]);
} catch (error) {
  caught = error;
}
check(caught === reason && writes === count, 'payload rejection identity and no native mutation');
console.log('PASS clipboard prepare-before-publish validation limits and rejected payloads');

forcedFailure = true;
await rejects(() => clipboard.readText(), 'ERR_FIREFLY_CLIPBOARD_UNAVAILABLE');
forcedFailure = false;
await clipboard.write([]);
check(
  (await clipboard.readText()) === '' && !(await clipboard.has('text/plain')),
  'empty write clears'
);
check(clipboard.clear() === undefined, 'synchronous clear result');
console.log('PASS clipboard native failure propagation and explicit clear');
console.log('DONE clipboard policy');
