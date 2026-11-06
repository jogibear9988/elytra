import { Socket, createServer } from 'node:net';
import { once } from 'node:events';
let accepts = 0;
const server = createServer((socket) => {
  accepts++;
  socket.destroy();
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
let done;
const completed = new Promise((resolve) => (done = resolve));
const socket = new Socket();
socket.connect({
  port: server.address().port,
  host: 'later.example',
  autoSelectFamily: false,

  lookup(host, opts, cb) {
    setTimeout(() => {
      cb(null, '127.0.0.1', 4);
      done();
    }, 10);
  }
});

socket.on('error', (err) => console.log('unexpected', err.code));

socket.write('queued', (err) => console.log('write', err?.code));

socket.destroy();
await once(socket, 'close');
await completed;
console.log('cancelled', socket.destroyed, socket.connecting, accepts);
await new Promise((resolve) => server.close(resolve));
