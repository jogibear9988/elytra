/** A completed request must release cancellation without changing its result later. */
import { createServer, get } from 'node:http';
const controller = new AbortController();
const server = createServer((request, response) => response.end('done'));

server.listen(0, '127.0.0.1', () => {
  let errors = 0;
  let ended = false;
  let text = '';
  const pending = get(
    { host: '127.0.0.1', port: server.address().port, agent: false, signal: controller.signal },
    (response) => {
      response.on('data', (data) => (text += data));

      response.once('end', () => (ended = response.complete));
    }
  );

  pending.on('error', () => errors++);

  pending.once('close', () => {
    controller.abort('too late');

    setImmediate(() =>
      server.close(() => console.log('settled', text, ended, errors, pending.aborted))
    );
  });
});
