/** A synthetic public event must not replace actual HTTP cancellation state. */
import { createServer, get } from 'node:http';
const controller = new AbortController();
let syntheticDestroyed = false;
let correct = false;
const server = createServer(() => {
  controller.signal.dispatchEvent(new Event('abort'));
  syntheticDestroyed = pending.destroyed;
  controller.abort('actual');
});
let pending;

server.listen(0, '127.0.0.1', () => {
  pending = get({
    host: '127.0.0.1',
    port: server.address().port,
    agent: false,
    signal: controller.signal
  });

  pending.on('error', (error) => (correct = error.code === 'ABORT_ERR'));

  pending.once('close', () =>
    server.close(() => console.log('synthetic HTTP', syntheticDestroyed, correct))
  );
});
