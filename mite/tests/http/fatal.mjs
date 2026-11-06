import { createServer } from 'node:http';
import { connect } from 'node:net';
import { once } from 'node:events';
const server = createServer(() => {
  throw Error('HTTP request callback failed');
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
connect(server.address().port, '127.0.0.1').write('GET / HTTP/1.1\r\nHost: test\r\n\r\n');
