import { createServer } from 'node:net';

const port = Number(process.argv[2] ?? 3000);
const server = createServer(socket => {
  socket.on('error', error => console.error(error.code));
  socket.pipe(socket);
});
server.on('error', error => {
  console.error(error.code);
  process.exitCode = 1;
});
server.listen(port, '127.0.0.1', () => {
  console.log(`TCP echo listening on 127.0.0.1:${server.address().port}`);
});
