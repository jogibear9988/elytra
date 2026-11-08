/** Genuine watch AbortSignal closure, preserving ordinary close event semantics. */
import fs from 'node:fs';
import { once } from 'node:events';
fs.writeFileSync('abort.txt', 'baseline');
const controller = new AbortController();
let errors = 0;
let calls = 0;
let closes = 0;
const watcher = fs.watch('abort.txt', { signal: controller.signal }, () => calls++);

watcher.on('error', () => errors++);

watcher.on('close', () => closes++);

const closed = once(watcher, 'close');
controller.abort();
await closed;
controller.abort();
watcher.close();
fs.appendFileSync('abort.txt', '-after-abort');
await new Promise((resolve) => setTimeout(resolve, 40));
if (errors || calls || closes !== 1) {
  throw Error('Abort must close once without error or later change');
}
const aborted = new AbortController();
aborted.abort();
let missing;
try {
  fs.watch('missing-preaborted.txt', { signal: aborted.signal });
} catch (error) {
  missing = error.code;
}
if (missing !== 'ENOENT') {
  throw Error('Native admission precedes pre-aborted close');
}
const already = fs.watch('abort.txt', { signal: aborted.signal });
await once(already, 'close');
console.log('PASS actual watcher abort/close, pre-aborted admission and later-event revocation');
