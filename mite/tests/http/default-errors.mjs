import { createServer, ServerResponse } from 'node:http';
import { connect } from 'node:net';
import { once } from 'node:events';
const server = createServer(
  {
    maxHeaderSize: 1024,
    ServerResponse: class extends ServerResponse {
      constructor(req) {
        super(req);
        this.sendDate = false;
      }
    }
  },
  (req, res) => res.end('unexpected request')
);
server.listen(0, '127.0.0.1');
await once(server, 'listening');
for (const request of [
  'GET / HTTP/1.1\r\n\r\n',
  'GET / HTTP/1.1\r\nHost: test\r\nX: ' + 'x'.repeat(2048) + '\r\n\r\n'
]) {
  const socket = connect(server.address().port, '127.0.0.1');
  socket.write(request);
  let text = '';
  for await (const bytes of socket) {
    text += bytes.toString();
  }
  console.log(JSON.stringify(text));
}
await new Promise((resolve) => server.close(resolve));
