/** Execute the unchanged pinned xterm headless terminal; no browser or canvas shims. */
import assert from 'node:assert/strict';
import { createRequire } from 'node:module';
import { resolve } from 'node:path';
import { readFileSync } from 'node:fs';
import { createHash } from 'node:crypto';
const root = resolve(process.argv[2], 'node_modules/@xterm/headless');
for (const [file, hash] of [
  ['package.json', '6105c670cb38c9e6d99c70a24ac5b7cc1cb13ddf6fb7c5d62abcfa21b90cf78e'],
  [
    'lib-headless/xterm-headless.js',
    '2e8e567454b499b052198fa2fa93efbcdb2ca404feeb4710a023f74c9b267bca'
  ]
]) {
  assert.equal(
    createHash('sha256')
      .update(readFileSync(resolve(root, file)))
      .digest('hex'),
    hash
  );
}
const require = createRequire(resolve(root, 'package.json'));
assert.equal(require('./package.json').version, '6.1.0-beta.303');
const { Terminal } = require('./lib-headless/xterm-headless.js');
const terminal = new Terminal({ cols: 20, rows: 4, allowProposedApi: true });
const deadline = setTimeout(() => {
  throw Error('xterm write deadline');
}, 10000);
try {
  await new Promise((resolve) => terminal.write('hello\r\n\x1b[31mred\x1b[0m', resolve));
  assert.equal(terminal.buffer.active.getLine(0).translateToString(true), 'hello');
  assert.equal(terminal.buffer.active.getLine(1).translateToString(true), 'red');
  assert.equal(terminal.buffer.active.cursorX, 3);
  assert.equal(terminal.buffer.active.cursorY, 1);
  terminal.reset();
  assert.equal(terminal.buffer.active.getLine(0).translateToString(true), '');
  assert.equal(terminal.buffer.active.cursorX, 0);
  assert.equal(terminal.buffer.active.cursorY, 0);
  console.log('PASS pinned xterm real asynchronous terminal parser, buffer and reset');
} finally {
  clearTimeout(deadline);
  terminal.dispose();
}
