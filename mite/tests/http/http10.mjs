import { createServer } from 'node:http';
import { connect } from 'node:net';
import { once } from 'node:events';
const server = createServer((req, res) => {
  res.sendDate = false;
  if (req.url === '/stream') {
    res.write('one');
    res.end('two');
  } else {
    res.end('known');
  }
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
for (const path of ['/known', '/stream']) {
  const socket = connect(server.address().port, '127.0.0.1');
  socket.write(`GET ${path} HTTP/1.0\r\n\r\n`);
  let text = '';
  for await (const chunk of socket) {
    text += chunk.toString();
  }
  console.log(JSON.stringify(text));
}
await new Promise((resolve) => server.close(resolve));
