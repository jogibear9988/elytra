/** Exercise the real global Console/native formatter without emitting large output. */
import assert from 'node:assert/strict';
import consoleModule from 'node:console';
import process from 'node:process';

assert.ok(process.versions.mite);
assert.equal(consoleModule, globalThis.console);
const stdout = process.stdout;
const stderr = process.stderr;
const originalOut = stdout.write;
const originalError = stderr.write;
const writes = [];

function capture(text, callback) {
  writes.push({ stream: this, text });
  if (typeof callback === 'function') {
    callback();
  }
  return true;
}

stdout.write = capture;
stderr.write = capture;
try {
  for (const name of ['log', 'info', 'debug', 'dir', 'warn', 'error']) {
    console[name]('owned-' + name);
  }
  console.assert(false, 'owned-assert');
  console.assert(true, 'must not write');
  assert.equal(writes.length, 7, 'All implemented global methods use actual stream hooks');
  const methods = ['log', 'info', 'debug', 'dir', 'warn', 'error', 'assert'];
  for (let index = 0; index < methods.length; index++) {
    assert.equal(writes[index].stream, index < 4 ? stdout : stderr);
    assert.equal(
      writes[index].text,
      (index === 6 ? 'Assertion failed: ' : '') + 'owned-' + methods[index] + '\n'
    );
  }

  const ascii = 'x'.repeat(1024 * 1024);
  console.log(ascii);
  assert.equal(writes.length, 8);
  assert.equal(writes[7].stream, stdout);
  assert.equal(writes[7].text.length, ascii.length + 1);
  assert.ok(
    writes[7].text === ascii + '\n',
    'Native 1 MiB argument text reaches writer with newline'
  );

  for (const args of [
    ['é'.repeat(512 * 1024 + 1)],
    ['x'.repeat(512 * 1024), 'y'.repeat(512 * 1024)]
  ]) {
    const count = writes.length;

    assert.throws(() => console.log(...args), { code: 'ERR_MITE_CONSOLE_LIMIT' });

    assert.equal(writes.length, count, 'Over-budget formatting must not perform any partial write');
  }
} finally {
  stdout.write = originalOut;
  stderr.write = originalError;
}
console.log('PASS native global Console stream hooks and UTF8/separator atomic bounds');
