/** Independent Python peers check TLS wire selection and decrypted HTTP parsing. */
import { get } from 'node:https';
import { readFileSync } from 'node:fs';
const [port, directory, mode] = process.argv.slice(2);
let requestClosed = false;
let bodyClosed = false;
let valid = false;
let errors = 0;
let responses = 0;
let protocolCode = '';

function report() {
  if (requestClosed && mode === 'plain') {
    console.log('raw', mode, protocolCode, responses === 0, errors === 1);
  } else if (requestClosed && bodyClosed) {
    console.log('raw', mode, valid, responses === 1);
  }
}

const pending = get(
  { host: '127.0.0.1', port: Number(port), agent: false, ca: readFileSync(directory + '/ca.pem') },
  (response) => {
    responses++;
    let text = '';

    response.on('data', (data) => (text += data));

    response.on('error', (error) => {
      valid =
        mode === 'truncated' &&
        error.code === 'ECONNRESET' &&
        response.aborted &&
        !response.complete;
    });

    response.on('end', () => {
      valid =
        mode === 'chunked' &&
        text === 'onetwo' &&
        response.complete &&
        response.trailers['x-end'] === 'done';
    });

    response.once('close', () => {
      bodyClosed = true;
      report();
    });
  }
);

pending.on('error', (error) => {
  errors++;
  protocolCode = error.code;
  valid = mode === 'plain' && error.code.startsWith('ERR_SSL_') && errors === 1;
});

pending.once('close', () => {
  requestClosed = true;
  report();
});
