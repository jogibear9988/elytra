import { createServer } from 'node:net';
import { once } from 'node:events';
const original = createServer();
original.listen(0, '127.0.0.1');
await once(original, 'listening');
const second = createServer();
second.listen(original.address().port, '127.0.0.1');
const [err] = await once(second, 'error');
console.log(
  'conflict',
  err.code,
  err.syscall,
  err.address,
  err.port === original.address().port,
  second.listening,
  second.address()
);
second.listen(0, '127.0.0.1');
await once(second, 'listening');
try {
  second.listen(0, '127.0.0.1');
} catch (e) {
  console.log('duplicate', e.code);
}
await new Promise((resolve) => second.close(resolve));
await new Promise((resolve) => original.close(resolve));
await new Promise((resolve) =>
  second.close((e) => {
    console.log('not running', e.code);
    resolve();
  })
);
