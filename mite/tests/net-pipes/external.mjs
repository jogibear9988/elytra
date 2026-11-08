/** Independent Python OS peer supplies actual bytes and verifies the native listener. */
import net from 'node:net';
const expected = 262173;
const server = net.createServer((socket) => {
  const chunks = [];
  let length = 0;

  socket.on('data', (chunk) => {
    chunks.push(chunk);
    length += chunk.length;
    if (length > expected) {
      throw Error('Too many independent peer bytes');
    }
    if (length === expected) {
      socket.end(Buffer.concat(chunks));
    }
  });

  socket.on('close', () => server.close(() => console.log('SERVED')));
});

server.listen(process.env.MITE_TEST_PIPE, () => console.log('READY'));
