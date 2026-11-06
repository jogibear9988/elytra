/** HTTP native consumers must be independent of public propagation stops. */
import { get, createServer } from 'node:http';
const controller = new AbortController();

controller.signal.addEventListener('abort', (event) => event.stopImmediatePropagation());

const server = createServer(() => controller.abort('unblocked'));

server.listen(0, '127.0.0.1', () => {
  let correct = false;
  const pending = get({
    host: '127.0.0.1',
    port: server.address().port,
    agent: false,
    signal: controller.signal
  });

  pending.on(
    'error',
    (error) => (correct = error.code === 'ABORT_ERR' && error.cause === 'unblocked')
  );

  pending.once('close', () => server.close(() => console.log('propagation', correct)));
});
