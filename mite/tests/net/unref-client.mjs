import { createServer, connect } from 'node:net';
import { once } from 'node:events';
let peer;
const server = createServer((socket) => {
  peer = socket;
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
const incoming = once(server, 'connection');
const client = connect(server.address().port, '127.0.0.1');
await once(client, 'connect');
await incoming;
console.log(client.unref() === client, client.ref() === client, peer.unref() === peer);
client.unref();
server.unref();
console.log('exit with live sockets');
