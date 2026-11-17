import { createServer } from 'node:http';
import { connect } from 'node:net';
import { once } from 'node:events';
let observed;
const server = createServer(async (req, res) => {
  const chunks = [];
  for await (const bytes of req) {
    chunks.push(bytes);
  }
  observed = [
    req.method,
    req.url,
    req.complete,
    Buffer.concat(chunks).toString(),
    req.rawTrailers,
    req.trailers,
    req.headers.cookie,
    req.headers['x-repeat'],
    req.headersDistinct['x-repeat']
  ];
  res.sendDate = false;
  res.writeHead(201, { 'Content-Type': 'text/plain' });
  res.write('');
  res.write('one');
  res.write('');
  res.addTrailers({ 'X-End': 'done' });
  res.end('two');
});
server.listen(0, '127.0.0.1');
await once(server, 'listening');
const socket = connect(server.address().port, '127.0.0.1');
socket.write(
  'POST /upload HTTP/1.1\r\nHost: test\r\nTransfer-Encoding: chunked\r\nConnection: close\r\nCookie: a=1\r\nCookie: b=2\r\nX-Repeat: a\r\nX-Repeat: b\r\n\r\n'
);
for (const part of ['4\r\nw', 'iki\r\n', '5\r\npedia\r\n', '0\r\nX-Trailer: yes\r\n\r\n']) {
  socket.write(part);
  await new Promise((resolve) => setTimeout(resolve, 2));
}
let text = '';
for await (const chunk of socket) {
  text += chunk.toString();
}
console.log(JSON.stringify(observed));
// HTTP chunk boundaries may change when Node corks adjacent writes. Decode the
// actual framing independently, while still requiring exact headers/body/trailers.
const boundary = text.indexOf('\r\n\r\n');
if (boundary < 0) {
  throw Error('Missing response headers');
}
const headers = text.slice(0, boundary);
let offset = boundary + 4;
let body = '';
let trailers;
while (offset < text.length) {
  const end = text.indexOf('\r\n', offset);
  const sizeText = text.slice(offset, end);
  if (end < 0 || !/^[0-9a-f]+$/i.test(sizeText)) {
    throw Error('Invalid response chunk size');
  }
  const size = parseInt(sizeText, 16);
  offset = end + 2;
  if (!Number.isSafeInteger(size) || size > text.length - offset) {
    throw Error('Truncated chunk');
  }
  if (size === 0) {
    const last = text.indexOf('\r\n\r\n', offset);
    if (last < 0 || last + 4 !== text.length) {
      throw Error('Invalid response trailer termination');
    }
    trailers = text.slice(offset, last);
    offset = text.length;
    break;
  }
  body += text.slice(offset, offset + size);
  offset += size;
  if (text.slice(offset, offset + 2) !== '\r\n') {
    throw Error('Invalid response chunk terminator');
  }
  offset += 2;
}
if (body !== 'onetwo' || trailers !== 'X-End: done') {
  throw Error('Response body/trailers changed');
}
console.log(JSON.stringify({ headers, body, trailers }));
await new Promise((resolve) => server.close(resolve));
