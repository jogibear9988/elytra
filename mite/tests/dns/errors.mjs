import { Socket, createServer } from 'node:net';
import { once } from 'node:events';
for (const lookup of [
  (host, opts, cb) =>
    cb(Object.assign(new Error('custom lookup failed'), { code: 'CUSTOM_LOOKUP' })),
  (host, opts, cb) => cb(null, 'not an address', 4),
  (host, opts, cb) => cb(null, '127.0.0.1', 3)
]) {
  const socket = new Socket();
  socket.connect({ port: 80, host: 'custom.example', autoSelectFamily: false, lookup });
  await new Promise((resolve) => {
    socket.on('error', (e) => console.log(e.name, e.code));

    socket.on('close', resolve);
  });
}
const socket = new Socket();
socket.connect({ host: 'bad host', port: 80 });
await new Promise((resolve) => {
  socket.on('error', (e) => console.log(e.code, e.syscall, e.hostname));

  socket.on('close', resolve);
});
const server = createServer();
server.listen(0, 'bad host');
const [e] = await once(server, 'error');
console.log('server', e.code, e.syscall, e.hostname, server.listening);
const pending = createServer();
pending.listen(0, 'localhost');
await new Promise((resolve) =>
  pending.close((e) => {
    console.log('close while resolving', e?.code ?? null, pending.listening);
    resolve();
  })
);
