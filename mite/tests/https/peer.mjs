/** Independent Node HTTPS server; report whether application requests passed the TLS boundary. */
import { createServer } from 'node:https';
import { readFileSync } from 'node:fs';
const [directory, mode] = process.argv.slice(2);
let requests = 0;
let bytes = 0;
const sockets = new Set();
const server = createServer(
  {
    key: readFileSync(directory + '/server-key.pem'),
    cert: readFileSync(directory + (mode === 'expired' ? '/expired.pem' : '/server.pem')),
    ALPNProtocols: ['http/1.1'],
    minVersion: 'TLSv1.2',
    maxVersion: mode === 'v12' ? 'TLSv1.2' : 'TLSv1.3'
  },
  (request, response) => {
    requests++;
    let body = '';

    request.on('data', (data) => {
      body += data;
      bytes += data.length;
    });

    request.on('error', () => {});

    response.on('error', () => {});

    request.on('end', () => {
      if (mode === 'cancel') {
        response.writeHead(200, { 'content-length': 100 });
        response.write('partial');
      } else if (mode === 'large') {
        response.writeHead(200, { 'content-length': 1024 * 1024 });
        response.end(Buffer.alloc(1024 * 1024, 97));
      } else if (mode === 'chunked') {
        response.writeHead(200, { 'transfer-encoding': 'chunked', trailer: 'X-End' });
        response.write('one');
        response.write('two');
        response.addTrailers({ 'X-End': 'done' });
        response.end();
      } else if (mode === 'head') {
        response.writeHead(200, { 'content-length': 50 });
        response.end();
      } else if (mode === 'empty') {
        response.writeHead(204);
        response.end();
      } else {
        response.setHeader('Content-Type', 'application/json');
        response.end(
          JSON.stringify({
            method: request.method,
            path: request.url,
            body,
            host: request.headers.host,
            sni: request.socket.servername || '',
            encrypted: request.socket.encrypted,
            auth: request.headers.authorization ?? null
          })
        );
      }
    });
  }
);

server.on('connection', (socket) => {
  sockets.add(socket);

  socket.once('close', () => sockets.delete(socket));
});

server.on('tlsClientError', (error, socket) => socket.destroy());

server.listen(0, mode === 'ipv6' ? '::1' : '127.0.0.1', () => console.log(server.address().port));

process.stdin.resume();

process.stdin.once('data', () => {
  for (const socket of sockets) {
    socket.destroy();
  }

  server.close(() => {
    console.log(JSON.stringify({ requests, bytes }));
    process.stdin.pause();
  });
});
