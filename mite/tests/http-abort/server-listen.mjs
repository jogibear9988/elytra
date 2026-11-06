/** Inherited listen cancellation must allow an accepted HTTP response to finish. */
import { createServer, get } from 'node:http';
const controller = new AbortController();
let closed = false;
let ended = false;
let complete = false;
let text = '';

function report() {
  if (closed && ended) {
    console.log('server listen', text, complete, !server.listening);
  }
}

const server = createServer((request, response) => {
  controller.abort('stop accepting');
  response.end('graceful');
});

server.once('close', () => {
  closed = true;
  report();
});

server.listen({ port: 0, host: '127.0.0.1', signal: controller.signal }, () => {
  get({ host: '127.0.0.1', port: server.address().port, agent: false }, (response) => {
    response.on('data', (data) => (text += data));

    response.once('end', () => {
      ended = true;
      complete = response.complete;
      report();
    });
  });
});
