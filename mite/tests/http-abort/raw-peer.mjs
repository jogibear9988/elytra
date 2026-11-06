/** Cancellation is also verified against an independent OS TCP peer. */
import { request } from 'node:http';
const [port, mode] = process.argv.slice(2);
const controller = new AbortController();
let correct = false;
let errors = 0;
let bodyAborted = false;
const pending = request(
  {
    host: '127.0.0.1',
    port: Number(port),
    path: '/' + mode,
    method: mode === 'upload' ? 'POST' : 'GET',
    agent: false,
    signal: controller.signal,
    ...(mode === 'upload' ? { headers: { 'content-length': 8 * 1024 * 1024 } } : {})
  },
  (response) => {
    response.on('error', (error) => (bodyAborted = error.code === 'ECONNRESET'));

    response.once('data', () => controller.abort(mode));
  }
);

pending.on('error', (error) => {
  errors++;
  correct = error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === mode;
});

pending.once('continue', () => controller.abort(mode));

pending.once('close', () =>
  setImmediate(() =>
    console.log('peer', mode, errors, correct, mode === 'upload' ? !pending.finished : bodyAborted)
  )
);

if (mode === 'upload') {
  // Leave the upload open; the peer triggers cancellation while the request owns
  // a large native write snapshot. Native completion still drains safely.
  pending.write(Buffer.alloc(8 * 1024 * 1024, 97));
} else {
  pending.end();
}
