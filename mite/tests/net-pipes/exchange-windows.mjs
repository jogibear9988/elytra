/** Real Windows duplex pipes use a fixed-length frame; EOF is not a directional half-close. */
import net from 'node:net';
import { once } from 'node:events';

// The framing protocol can also be exercised over real Unix sockets locally;
// that does not establish native Windows transport acceptance.
if (!process.env.MITE_TEST_PIPE) {
  throw Error('Require an isolated native pipe endpoint');
}
const payload = Buffer.alloc(3 * 1024 * 1024 + 29);
for (let index = 0; index < payload.length; index++) {
  payload[index] = index % 251;
}

async function writeFrame(socket, frame) {
  socket.cork();
  const written = new Promise((resolve, reject) => {
    if (socket.write(frame, (error) => (error ? reject(error) : resolve())) !== false) {
      reject(Error('Real pipe frame must apply stream backpressure'));
    }
  });
  const drained = once(socket, 'drain');
  socket.uncork();
  await Promise.all([written, drained]);
  if (socket.writableLength !== 0 || socket.bytesWritten !== frame.length) {
    throw Error('Real pipe frame write queue/count');
  }
}

for (const object of [false, true]) {
  const endpoint = process.env.MITE_TEST_PIPE + (object ? '-object' : '-string');
  let accept;
  let rejectServer;
  const served = new Promise((resolve, reject) => {
    accept = resolve;
    rejectServer = reject;
  });
  const server = net.createServer({ pauseOnConnect: true }, (peer) => {
    const chunks = [];
    let received = 0;
    peer.on('error', rejectServer);

    peer.on('data', (chunk) => {
      chunks.push(chunk);
      received += chunk.length;
      if (received > payload.length) {
        rejectServer(Error('Request frame overflow'));
      } else if (received === payload.length) {
        // Frame completion, never an EOF/half-close, authorizes the reply.
        const frame = Buffer.concat(chunks);
        if (!frame.equals(payload) || peer.bytesRead !== payload.length) {
          rejectServer(Error('Real request frame binary integrity'));
          return;
        }

        writeFrame(peer, frame).then(() => {
          peer.end();
          accept();
        }, rejectServer);
      }
    });

    setTimeout(() => peer.resume(), 5);
  });
  server.on('error', rejectServer);
  if (object) {
    server.listen({ path: endpoint, backlog: 4 });
  } else {
    server.listen(endpoint);
  }
  await once(server, 'listening');
  if (server.address() !== endpoint || !server.listening) {
    throw Error('Native Windows pipe listener address');
  }
  const socket = object
    ? net.createConnection({ path: endpoint, highWaterMark: 1024 })
    : new net.Socket({ highWaterMark: 1024 });
  if (!object) {
    socket.connect(endpoint);
  }
  await once(socket, 'connect');
  // Install the reader before writing, preserving genuine concurrent duplex I/O.
  const reply = (async () => {
    const chunks = [];
    for await (const chunk of socket) {
      chunks.push(chunk);
    }
    return Buffer.concat(chunks);
  })();
  await writeFrame(socket, payload);
  const result = await reply;
  await served;
  if (!result.equals(payload) || socket.bytesRead !== payload.length) {
    throw Error('Real response frame binary integrity');
  }
  socket.end();
  await new Promise((resolve) => server.close(resolve));
  if (server.address() !== endpoint || server.listening) {
    throw Error('Closed native Windows pipe listener state');
  }
  console.log(
    object ? 'object' : 'string',
    'binary framed duplex drain both directions',
    payload.length
  );
}
