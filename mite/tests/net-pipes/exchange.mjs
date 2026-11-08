/** Real OS stream pipes; no in-memory transport or IPC serialization. */
import net from 'node:net';
import { once } from 'node:events';

const base = process.env.MITE_TEST_PIPE;
if (!base) {
  throw Error('Require isolated OS pipe endpoint');
}
const payload = Buffer.alloc(3 * 1024 * 1024 + 29);
for (let index = 0; index < payload.length; index++) {
  payload[index] = index % 251;
}

for (const object of [false, true]) {
  const endpoint = base + (object ? '-object' : '-string');
  let ended = false;
  const server = net.createServer({ allowHalfOpen: true, pauseOnConnect: true }, (socket) => {
    const chunks = [];

    setTimeout(() => {
      socket.on('data', (chunk) => chunks.push(chunk));

      socket.on('end', () => {
        if (!socket.readableEnded || socket.writableEnded) {
          throw Error('Pipe half-close state');
        }
        ended = true;

        setTimeout(() => socket.end(Buffer.concat(chunks)), 5);
      });

      socket.resume();
    }, 5);
  });
  if (object) {
    server.listen({ path: endpoint, backlog: 4 });
  } else {
    server.listen(endpoint);
  }
  await once(server, 'listening');
  if (server.address() !== endpoint || !server.listening) {
    throw Error('Pipe listener address');
  }
  const socket = object
    ? net.createConnection({ path: endpoint, highWaterMark: 1024 })
    : new net.Socket({ highWaterMark: 1024 });
  if (!object) {
    socket.connect(endpoint);
  }
  await once(socket, 'connect');
  socket.cork();
  let complete;
  const written = new Promise((resolve, reject) => {
    complete = (failure) => (failure ? reject(failure) : resolve());
  });
  if (socket.write(payload, complete) !== false) {
    throw Error('Corked pipe write must apply stream backpressure');
  }
  const drained = once(socket, 'drain');
  socket.uncork();
  await drained;
  await written;
  if (socket.writableLength !== 0) {
    throw Error('Pipe write queue not drained');
  }
  socket.end();
  const chunks = [];
  for await (const chunk of socket) {
    chunks.push(chunk);
  }
  if (
    !ended ||
    !Buffer.concat(chunks).equals(payload) ||
    socket.bytesWritten !== payload.length ||
    socket.bytesRead !== payload.length
  ) {
    throw Error('Real pipe binary integrity');
  }
  await new Promise((resolve) => server.close(resolve));
  if (server.address() !== endpoint || server.listening) {
    throw Error('Closed pipe listener state');
  }
  console.log(object ? 'object' : 'string', 'binary half-close drain', payload.length);
}
