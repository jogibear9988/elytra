/** Exercise real global/named/CommonJS output streams through OS pipes. */
import assert from 'node:assert/strict';
import processDefault, { stdout, stderr } from 'node:process';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);
assert.equal(processDefault, process);
assert.equal(require('node:process').stdout, stdout);
assert.equal(process.stdout, stdout);
assert.equal(process.stderr, stderr);
assert.equal(stdout.fd, 1);
assert.equal(stderr.fd, 2);
const bytes = Buffer.alloc(65536);
for (let index = 0; index < bytes.length; index++) {
  bytes[index] = index % 256;
}
await new Promise((resolve, reject) =>
  stdout.write(bytes, (error) => (error ? reject(error) : resolve()))
);
await new Promise((resolve, reject) =>
  stdout.write('Grüße 🦋\n', (error) => (error ? reject(error) : resolve()))
);
await new Promise((resolve, reject) =>
  stderr.write('native stderr 🦋\n', (error) => (error ? reject(error) : resolve()))
);
