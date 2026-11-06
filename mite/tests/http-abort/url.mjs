/** URL overloads must retain option-provided signals and their original reason identity. */
import { get } from 'node:http';
const reason = { url: true };
const signal = AbortSignal.abort(reason);
let correct = false;
const pending = get(new URL('http://127.0.0.1:9/never?sent=true'), { signal, agent: false });

pending.on('error', (error) => (correct = error.code === 'ABORT_ERR' && error.cause === reason));

pending.once('close', () => console.log('url', correct, pending.socket === null));
