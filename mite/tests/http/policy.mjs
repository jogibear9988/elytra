import { createServer } from 'node:http';
import { connect } from 'node:net';
import { once } from 'node:events';

async function exchange(request, options = {}) {
  const server = createServer(options, (req, res) => {
    req.on('error', () => {});
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const socket = connect(server.address().port, '127.0.0.1');
  socket.write(request);
  let text = '';
  for await (const bytes of socket) {
    text += bytes.toString();
  }
  await new Promise((resolve) => server.close(resolve));
  return text.split('\r\n')[0];
}

console.log(
  'headers timeout',
  await exchange('GET / HTTP/1.1\r\nHost: test\r\n', { headersTimeout: 15, requestTimeout: 30 })
);
console.log(
  'body timeout',
  await exchange('POST / HTTP/1.1\r\nHost: test\r\nContent-Length: 20\r\n\r\nx', {
    headersTimeout: 15,
    requestTimeout: 30
  })
);
console.log(
  'upgrade',
  await exchange('GET / HTTP/1.1\r\nHost: test\r\nConnection: Upgrade\r\nUpgrade: custom\r\n\r\n')
);
console.log('tunnel', await exchange('CONNECT test:443 HTTP/1.1\r\nHost: test\r\n\r\n'));
for (const options of [
  { insecureHTTPParser: true },
  { uniqueHeaders: ['x'] },
  { connectionsCheckingInterval: 1 },
  { rejectNonStandardBodyWrites: true }
]) {
  try {
    createServer(options);
    console.log('unexpected success');
  } catch (e) {
    console.log(e.code);
  }
}
