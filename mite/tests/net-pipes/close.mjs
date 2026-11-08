/** Closing a pipe listener waits for its accepted peer, without blocking that connection. */
import net from 'node:net';
import { once } from 'node:events';
const endpoint = process.env.MITE_TEST_PIPE;
let peer;
const server = net.createServer({ allowHalfOpen: true }, (socket) => {
  peer = socket;
});
server.listen(endpoint);
await once(server, 'listening');
const socket = net.connect({ path: endpoint });
await once(socket, 'connect');
while (!peer) {
  await new Promise((resolve) => setTimeout(resolve, 1));
}
let ended = false;
const closing = new Promise((resolve) =>
  server.close(() => {
    ended = true;
    resolve();
  })
);
await new Promise((resolve) => setTimeout(resolve, 10));
if (ended || server.listening || server.address() !== endpoint) {
  throw Error('Listener close must wait for live accepted pipe');
}
peer.end('after-listener-close');
socket.end();
let reply = '';
for await (const chunk of socket) {
  reply += chunk;
}
await closing;
if (reply !== 'after-listener-close' || !ended) {
  throw Error('Accepted pipe lost during listener close');
}
console.log('close waited for accepted peer and preserved transfer');
