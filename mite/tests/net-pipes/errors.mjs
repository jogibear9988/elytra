/** Failed binds/connects never remove a foreign listener or ordinary file. */
import net from 'node:net';
import fs from 'node:fs';
import { once } from 'node:events';
const endpoint = process.env.MITE_TEST_PIPE;
if (!endpoint) {
  throw Error('Require isolated OS pipe endpoint');
}
const original = net.createServer((socket) => socket.end('original-owner'));
original.listen({ path: endpoint });
await once(original, 'listening');
const second = net.createServer();
second.listen(endpoint);
const [conflict] = await once(second, 'error');
if (
  conflict.code !== 'EADDRINUSE' ||
  second.listening ||
  second.address() !== endpoint ||
  !original.listening
) {
  throw Error('Existing pipe listener collision');
}
const checkOriginal = net.connect(endpoint);
let answer = '';
for await (const chunk of checkOriginal) {
  answer += chunk;
}
if (answer !== 'original-owner') {
  throw Error('Failed bind damaged original listener');
}
console.log('existing listener preserved', conflict.code);
await new Promise((resolve) => original.close(resolve));
const missing = net.createConnection({ path: endpoint + '-missing' });
const closed = new Promise((resolve) => missing.on('close', resolve));
const [failure] = await once(missing, 'error');
await closed;
if (
  failure.code !== 'ENOENT' ||
  failure.syscall !== 'connect' ||
  !missing.destroyed ||
  missing.connecting
) {
  throw Error('Missing OS pipe endpoint error/lifetime');
}
console.log('missing endpoint', failure.code, failure.syscall);
if (process.platform !== 'win32') {
  const file = endpoint + '-ordinary';
  fs.writeFileSync(file, 'foreign regular file must survive');
  const rejected = net.createServer();
  rejected.listen({ path: file });
  const [error] = await once(rejected, 'error');
  if (
    error.code !== 'EADDRINUSE' ||
    fs.readFileSync(file, 'utf8') !== 'foreign regular file must survive' ||
    !fs.statSync(file).isFile()
  ) {
    throw Error('Pipe bind overwrote ordinary file');
  }
  console.log('ordinary file preserved', error.code);
  fs.unlinkSync(file);
}
