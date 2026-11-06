/** Independent Node/OpenSSL peer for TLS versions unavailable in the host Python build. */
import { createServer } from 'node:tls';
import { readFileSync } from 'node:fs';
const [directory, version, mode] = process.argv.slice(2);
const server = createServer(
  {
    key: readFileSync(directory + '/server-key.pem'),
    cert: readFileSync(
      directory +
        (mode === 'expired'
          ? '/expired.pem'
          : ['selfsigned', 'default-untrusted'].includes(mode)
            ? '/selfsigned.pem'
            : '/server.pem')
    ),
    minVersion: version,
    maxVersion: version,
    ALPNProtocols: ['http/1.1'],
    allowHalfOpen: true
  },
  (socket) => {
    const expectedName =
      mode === 'ip'
        ? false
        : mode === 'wildcard'
          ? 'app.elytra.test'
          : mode === 'wildcard-mismatch'
            ? 'deep.app.elytra.test'
            : ['mismatch', 'permissive-host'].includes(mode)
              ? 'wrong.example'
              : 'localhost';
    if (socket.servername !== expectedName) {
      throw Error('SNI was not preserved');
    }

    socket.on('error', () => {});

    if (mode === 'abort') {
      socket.write('ready');
    } else {
      socket.on('data', (data) => socket.write(data));
    }

    socket.on('end', () => socket.end());

    socket.on('close', () => server.close());
  }
);

server.on('tlsClientError', (error, socket) => {
  socket.destroy();
  server.close();
});

const accepted = new Set();

server.on('connection', (socket) => {
  accepted.add(socket);

  socket.once('close', () => {
    accepted.delete(socket);
    server.close();
  });
});
// A failed client may finish without the peer completing a TLS half-close. Give
// the test driver explicit peer teardown after it has checked the client result.
process.stdin.resume();

process.stdin.once('data', () => {
  for (const socket of accepted) {
    socket.destroy();
  }
  server.close();
  process.stdin.pause();
});

const deadline = setTimeout(() => {
  server.close();
  process.exitCode = 1;
}, 10000);

server.on('close', () => clearTimeout(deadline));

server.listen(0, '127.0.0.1', () => console.log(server.address().port));
