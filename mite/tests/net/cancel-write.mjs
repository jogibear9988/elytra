import net from 'node:net';
import { once } from 'node:events';
const server = net.createServer((socket) => {
  socket.on('error', () => {});

  socket.pause();

  setTimeout(() => socket.destroy(), 10);
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
const c = net.connect(server.address().port, '127.0.0.1');
await once(c, 'connect');

c.on('error', (e) => console.log('error', e.code));

c.write(Buffer.alloc(8 * 1024 * 1024), (err) => console.log('write', err?.code ?? null));

c.on('close', (had) => {
  console.log('close', had);

  server.close(() => console.log('server closed'));
});

c.destroy();
