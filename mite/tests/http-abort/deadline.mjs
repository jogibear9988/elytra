/** A real HTTP deadline must abort the native connection with the TimeoutError cause. */
import { createServer, get } from 'node:http';
const server = createServer(() => {});

server.listen(0, '127.0.0.1', () => {
  let correct = false;
  const pending = get({
    host: '127.0.0.1',
    port: server.address().port,
    agent: false,
    signal: AbortSignal.timeout(50)
  });

  pending.on(
    'error',
    (error) =>
      (correct =
        error.code === 'ABORT_ERR' &&
        error.cause.name === 'TimeoutError' &&
        error.cause.code === 23)
  );

  pending.once('close', () => server.close(() => console.log('deadline', correct)));
});
