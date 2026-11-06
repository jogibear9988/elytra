import { createServer } from 'node:http';
import { connect } from 'node:net';
import { once } from 'node:events';
const server = createServer((req, res) => {
  res.sendDate = false;
  if (req.url === '/empty') {
    res.statusCode = 204;
  }
  res.end('invisible');
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
for (const [method, url] of [
  ['HEAD', '/'],
  ['GET', '/empty']
]) {
  const socket = connect(server.address().port, '127.0.0.1');
  socket.write(`${method} ${url} HTTP/1.1\r\nHost: test\r\nConnection: close\r\n\r\n`);
  let text = '';
  for await (const chunk of socket) {
    text += chunk.toString();
  }
  console.log(JSON.stringify(text));
}
await new Promise((resolve) => server.close(resolve));
