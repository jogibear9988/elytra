/** Cancellation must close a paused response without waiting for readable demand. */
import { createServer, get } from 'node:http';
const controller = new AbortController();
const server = createServer((request, response) => {
  response.writeHead(200, { 'content-length': 100000 });
  response.write('paused body');
});

server.listen(0, '127.0.0.1', () => {
  let correct = false;
  const pending = get(
    { host: '127.0.0.1', port: server.address().port, agent: false, signal: controller.signal },
    (response) => {
      response.pause();

      response.on('error', (error) => {
        if (error.code !== 'ECONNRESET') {
          throw error;
        }
      });

      response.once('close', () =>
        server.close(() =>
          console.log('paused', correct, response.aborted, response.destroyed, !response.complete)
        )
      );

      setImmediate(() => controller.abort('paused'));
    }
  );

  pending.on(
    'error',
    (error) => (correct = error.code === 'ABORT_ERR' && error.cause === 'paused')
  );
});
