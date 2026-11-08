/** Known unsupported terminal/buffer behavior fails before misleading success. */
import * as readline from 'node:readline';
import { PassThrough } from 'node:stream';

function refuses(action, code) {
  let failure;
  try {
    action();
  } catch (error) {
    failure = error;
  }
  if (failure?.code !== code) {
    throw Error('Expected ' + code + ': ' + failure);
  }
}

const input = new PassThrough();
for (const options of [{ terminal: true }, { completer: () => [] }, { historySize: 10 }]) {
  refuses(() => readline.createInterface({ input, ...options }), 'ERR_MITE_UNSUPPORTED_API');
}
if (input.listenerCount('data')) {
  throw Error('Invalid constructor changed input');
}
for (const name of [
  'emitKeypressEvents',
  'cursorTo',
  'moveCursor',
  'clearLine',
  'clearScreenDown'
]) {
  refuses(() => readline[name](), 'ERR_MITE_UNSUPPORTED_API');
}
const rl = readline.createInterface({ input });

refuses(() => rl.write('input'), 'ERR_MITE_UNSUPPORTED_API');

refuses(() => rl.getCursorPos(), 'ERR_MITE_UNSUPPORTED_API');

rl.close();

refuses(() => rl.question('closed?', () => {}), 'ERR_USE_AFTER_CLOSE');

const tooLongInput = new PassThrough();
const tooLong = readline.createInterface({ input: tooLongInput });
let error;

tooLong.on('error', (value) => {
  error = value;
});

tooLongInput.write('x'.repeat(1048576));
tooLongInput.write('x');
if (error?.code !== 'ERR_OUT_OF_RANGE' || !tooLong.closed) {
  throw Error('Unbounded line accepted');
}
const raw = new PassThrough();
const lines = readline.createInterface({ input: raw });
const pending = lines[Symbol.asyncIterator]().next();
raw.emit('error', 0);
let caught = false;
try {
  await pending;
} catch (value) {
  caught = value === 0;
}
if (!caught || !lines.closed) {
  throw Error('Falsy input error lost');
}
console.log('PASS explicit readline terminal/option/size limits and falsy-error retirement');
