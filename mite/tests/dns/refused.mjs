import { createServer, connect } from 'node:net';
import { once } from 'node:events';
const server = createServer();
server.listen(0, '127.0.0.1');
await once(server, 'listening');
const port = server.address().port;
await new Promise((resolve) => server.close(resolve));
const socket = connect({
  port,
  host: 'two.example',

  lookup(host, opts, cb) {
    cb(null, [
      { address: '127.0.0.1', family: 4 },
      { address: '::1', family: 6 }
    ]);
  }
});

socket.on('error', (e) =>
  console.log(
    e.constructor.name,
    e.code,
    JSON.stringify(e.errors.map((e) => [e.code, e.address, e.port === port]))
  )
);

socket.on('close', (had) => console.log('close', had));
