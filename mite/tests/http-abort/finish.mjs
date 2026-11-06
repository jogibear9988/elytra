/** Upload finish is not request completion: waiting for response remains abortable. */
import { createServer, request } from 'node:http';
const controller = new AbortController();
const server = createServer((incoming) => {
  incoming.resume();

  incoming.once('end', () => controller.abort('after upload'));
});

server.listen(0, '127.0.0.1', () => {
  let finished = false;
  let correct = false;
  const pending = request({
    host: '127.0.0.1',
    port: server.address().port,
    method: 'POST',
    agent: false,
    signal: controller.signal
  });

  pending.once('finish', () => (finished = true));

  pending.on(
    'error',
    (error) => (correct = error.code === 'ABORT_ERR' && error.cause === 'after upload')
  );

  pending.once('close', () =>
    server.close(() => console.log('after finish', finished, correct, pending.destroyed))
  );

  pending.end('upload complete');
});
