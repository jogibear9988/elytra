import { createServer, connect } from 'node:net';
import { once } from 'node:events';
const size = 3 * 1024 * 1024 + 29;

async function exchange(corked) {
  let resolveReceived;
  const received = new Promise((resolve) => (resolveReceived = resolve));
  const server = createServer({ pauseOnConnect: true }, (socket) => {
    let count = 0;
    let sum = 0;

    setTimeout(() => {
      socket.on('data', (bytes) => {
        count += bytes.length;
        for (const b of bytes) {
          sum = (sum + b) >>> 0;
        }
      });

      socket.on('end', () => {
        socket.end(String(count));
        resolveReceived([count, sum]);
      });

      socket.resume();
    }, 10);
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const socket = connect({ port: server.address().port, host: '127.0.0.1', highWaterMark: 1024 });
  await once(socket, 'connect');
  const payload = Buffer.alloc(size);
  for (let i = 0; i < size; i++) {
    payload[i] = i % 251;
  }
  let complete;
  const written = new Promise(
    (resolve, reject) => (complete = (error) => (error ? reject(error) : resolve()))
  );
  if (corked) {
    socket.cork();
  }
  const accepted = socket.write(payload, complete);
  // The OS may accept a direct write immediately, including large Windows writes.
  // Only false promises a future drain. Corking forces the buffered branch without
  // depending on kernel send-buffer sizing or completion timing.
  const drained = accepted ? null : once(socket, 'drain');
  if (corked && accepted) {
    throw Error('Corked write above highWaterMark must apply backpressure');
  }
  if (corked) {
    socket.uncork();
  }
  if (drained) {
    await drained;
  }
  await written;
  if (socket.writableLength !== 0) {
    throw Error('Write buffer did not drain');
  }
  console.log(corked ? 'corked' : 'direct', 'drained', true);
  socket.end();
  let answer = '';
  for await (const chunk of socket) {
    answer += chunk;
  }
  const result = await received;
  let expectedSum = 0;
  for (const byte of payload) {
    expectedSum = (expectedSum + byte) >>> 0;
  }
  if (
    answer !== String(size) ||
    result[0] !== size ||
    result[1] !== expectedSum ||
    socket.bytesWritten !== size
  ) {
    throw Error('Native transfer integrity');
  }
  console.log('received', answer, ...result, socket.bytesWritten);
  await new Promise((resolve) => server.close(resolve));
}

await exchange(false);
await exchange(true);
