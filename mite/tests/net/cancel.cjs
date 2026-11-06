const net = require('node:net');
const server = net.createServer((socket) => socket.resume());

server.listen(0, '127.0.0.1', () => {
  const c = net.connect(server.address().port, '127.0.0.1');

  c.on('error', (e) => console.log('error', e.code));

  c.write('hello', (err) => console.log('write', err?.code ?? null));

  c.on('close', (had) => {
    console.log('close', had);

    server.close(() => console.log('server closed'));
  });

  c.destroy();
});
