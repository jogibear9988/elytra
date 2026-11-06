/** Long-lived signals must not retain completed requests after native transport closure. */
import { createServer, get } from 'node:http';
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const controller = new AbortController();
const refs = [];
let closed = 0;
let turns = 0;
const server = createServer((request, response) => response.end('done'));

function check() {
  collect();
  if (++turns < 8) {
    setImmediate(check);
    return;
  }
  console.log(
    'HTTP released',
    refs.every((ref) => ref.deref() === undefined)
  );
  controller.abort('after collection');
}

server.listen(0, '127.0.0.1', () => {
  for (let index = 0; index < 8; index++) {
    const pending = get(
      { host: '127.0.0.1', port: server.address().port, agent: false, signal: controller.signal },
      (response) => response.resume()
    );
    refs.push(new WeakRef(pending));

    pending.on('error', (error) => {
      throw error;
    });

    pending.once('close', () => {
      if (++closed === 8) {
        server.close(() => setImmediate(check));
      }
    });
  }
});
