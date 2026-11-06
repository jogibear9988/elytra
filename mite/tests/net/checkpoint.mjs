import { createServer, connect } from 'node:net';
import { once } from 'node:events';
const order = [];
const server = createServer((socket) => {
  socket.on('data', () => {
    order.push('data');

    queueMicrotask(() => {
      order.push('microtask');

      process.nextTick(() => order.push('tick in microtask'));
    });

    process.nextTick(() => {
      order.push('tick');

      queueMicrotask(() => order.push('microtask in tick'));
    });

    Promise.resolve().then(() => order.push('promise'));

    setTimeout(() => socket.end('done'), 5);
  });
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
const client = connect(server.address().port, '127.0.0.1');
client.write('start');
for await (const _ of client) {
}
console.log(order.join(','));
await new Promise((resolve) => server.close(resolve));
