import { createServer, connect } from 'node:net';
import { once } from 'node:events';
const server = createServer((socket) => socket.end('custom'));
server.listen(0, '127.0.0.1');
await once(server, 'listening');
for (const multiple of [false, true]) {
  const events = [];
  const client = connect({
    port: server.address().port,
    host: 'mapped.example',
    autoSelectFamily: multiple,

    lookup(hostname, opts, cb) {
      events.push(['lookup function', hostname, opts]);
      if (multiple) {
        cb(null, [
          { address: '127.0.0.2', family: 4 },
          { address: '127.0.0.1', family: 4 }
        ]);
      } else {
        cb(null, '127.0.0.1', 4);
      }
    }
  });

  client.on('lookup', (err, address, family, hostname) =>
    events.push(['lookup event', err, address, family, hostname])
  );

  client.on('connectionAttempt', (address, port, family) =>
    events.push(['attempt', address, family])
  );

  client.on('connectionAttemptTimeout', (address, port, family) =>
    events.push(['timeout', address, family])
  );

  client.on('connectionAttemptFailed', (address, port, family, error) =>
    events.push(['failed', address, family, error.code])
  );

  let text = '';
  for await (const bytes of client) {
    text += bytes;
  }
  console.log(JSON.stringify(events), text);
}
await new Promise((resolve) => server.close(resolve));
