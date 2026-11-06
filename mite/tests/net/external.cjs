const { createServer } = require('node:net');
const server = createServer({ allowHalfOpen: true }, (socket) => {
  const chunks = [];

  socket.on('data', (chunk) => chunks.push(chunk));

  socket.on('end', () => socket.end(Buffer.concat(chunks)));

  socket.on('close', () => server.close());
});

server.listen(0, '127.0.0.1', () => console.log(server.address().port));
