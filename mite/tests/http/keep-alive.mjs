import { createServer } from 'node:http';
import { connect } from 'node:net';
import { once } from 'node:events';
let connections = 0;
let requests = 0;
const server = createServer((req, res) => {
  requests++;
  res.sendDate = false;
  res.end('ok');
});

server.on('connection', () => connections++);

server.listen(0, '127.0.0.1');
await once(server, 'listening');
const socket = connect(server.address().port, '127.0.0.1');
socket.write('POST /unread HTTP/1.1\r\nHost: test\r\nContent-Length: 4\r\n\r\nbody');
let text = '';
let sent = false;
for await (const bytes of socket) {
  text += bytes.toString();
  if (!sent && text.endsWith('ok')) {
    sent = true;
    socket.write('GET /next HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n');
  }
}
console.log(connections, requests, JSON.stringify(text));
await new Promise((resolve) => server.close(resolve));
