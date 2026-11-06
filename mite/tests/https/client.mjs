/** Shared HTTPS client checks real HTTP framing after certificate verification. */
import https, { request, get } from 'node:https';
import { ClientRequest, IncomingMessage } from 'node:http';
import { TLSSocket } from 'node:tls';
import { readFileSync } from 'node:fs';
const [port, directory, mode] = process.argv.slice(2);
const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const controller = new AbortController();
const config = {
  host: mode === 'ipv6' ? '::1' : mode === 'dns' ? 'localhost' : '127.0.0.1',
  family: mode === 'ipv6' ? 6 : 4,
  port: Number(port),
  ...(mode === 'mismatch'
    ? { servername: 'wrong.example' }
    : ['ip', 'ipv6', 'custom-host', 'dns'].includes(mode)
      ? {}
      : { servername: 'localhost' }),
  ca: ['untrusted', 'permissive'].includes(mode) ? [] : readFileSync(directory + '/ca.pem'),
  agent: false,
  ALPNProtocols: mode === 'no-alpn' ? [] : ['http/1.1'],
  signal: controller.signal,
  ...(mode === 'post' ? { method: 'POST', path: '/posted?q=1', auth: 'user:pass' } : {}),
  ...(mode === 'head' ? { method: 'HEAD' } : {}),
  ...(mode === 'custom-host' ? { headers: { host: 'localhost' } } : {}),
  ...(mode === 'permissive' ? { rejectUnauthorized: false } : {})
};
if (mode === 'pre') {
  controller.abort('before HTTPS');
}
let valid = false;
let payload = '';
let length = 0;
let errors = 0;
let code = '';
let secure = false;
let responseEnded = false;
let requestClosed = false;

function report() {
  if (requestClosed && responseEnded) {
    if (mode === 'late-abort') {
      controller.abort('after HTTPS');
    }
    console.log('HTTPS', mode, valid, true, errors === 0);
  }
}

const onResponse = (response) => {
  valid &&=
    response instanceof IncomingMessage &&
    response.socket instanceof TLSSocket &&
    response.socket.encrypted &&
    response.socket.authorized === (mode !== 'permissive');
  if (mode === 'gc') {
    collect();
  }
  if (mode === 'permissive') {
    valid &&= !!response.socket.authorizationError;
  }
  if (mode === 'cancel') {
    response.on('error', (error) => {
      if (error.code !== 'ECONNRESET') {
        throw error;
      }
    });

    response.once('data', () => controller.abort('HTTPS response cancelled'));

    response.once('close', () =>
      console.log('cancel', valid, code === 'ABORT_ERR', response.aborted, !response.complete)
    );

    return;
  }

  response.on('data', (data) => {
    length += data.length;
    if (mode === 'large') {
      for (const byte of data) {
        if (byte !== 97) {
          throw Error('TLS response corruption');
        }
      }
      response.pause();

      setImmediate(() => response.resume());
    } else {
      payload += data;
    }
  });

  response.once('end', () => {
    responseEnded = true;
    if (['head', 'empty'].includes(mode)) {
      valid &&= length === 0 && response.statusCode === (mode === 'head' ? 200 : 204);
    } else if (mode === 'chunked') {
      valid &&= payload === 'onetwo' && response.trailers['x-end'] === 'done';
    } else if (mode === 'large') {
      valid &&= length === 1024 * 1024;
    } else {
      const reply = JSON.parse(payload);
      valid &&=
        reply.encrypted &&
        reply.host ===
          (mode === 'custom-host'
            ? 'localhost'
            : `${mode === 'ipv6' ? '[::1]' : mode === 'dns' ? 'localhost' : '127.0.0.1'}:${port}`) &&
        reply.sni === (['ip', 'ipv6'].includes(mode) ? '' : 'localhost') &&
        reply.method === (mode === 'post' ? 'POST' : 'GET') &&
        reply.path === (mode === 'post' ? '/posted?q=1' : mode === 'url' ? '/from-url?x=1' : '/') &&
        reply.body === (mode === 'post' ? 'upload body' : '') &&
        reply.auth === (mode === 'post' ? 'Basic dXNlcjpwYXNz' : null);
    }
    valid &&= response.complete;
    report();
  });
};

const pending =
  mode === 'url'
    ? get(new URL(`https://127.0.0.1:${port}/from-url?x=1`), config, onResponse)
    : mode === 'post'
      ? request(config, onResponse)
      : get(config, onResponse);
valid =
  pending instanceof ClientRequest && pending.protocol === 'https:' && https.request === request;

pending.on('socket', (socket) => {
  socket.once('secureConnect', () => (secure = socket.encrypted && socket.authorized));

  if (mode === 'socket-abort') {
    controller.abort('socket HTTPS');
  }
});

pending.on('error', (error) => {
  errors++;
  code = error.code;
  if (!['cancel', 'pre', 'socket-abort', 'mismatch', 'expired', 'untrusted'].includes(mode)) {
    // A failed positive case must report the actual TLS/socket error, not exit
    // quietly with no response while the peer only reports zero requests.
    throw error;
  }
  if (
    mode === 'cancel' &&
    (error.name !== 'AbortError' || error.cause !== 'HTTPS response cancelled')
  ) {
    throw error;
  }
});

pending.once('close', () => {
  if (['pre', 'socket-abort'].includes(mode)) {
    console.log('early abort', mode, !secure, errors === 1, code === 'ABORT_ERR');
  } else if (['mismatch', 'untrusted', 'expired'].includes(mode)) {
    console.log(
      'reject',
      mode,
      !secure,
      errors === 1,
      mode === 'mismatch'
        ? code === 'ERR_TLS_CERT_ALTNAME_INVALID'
        : mode === 'expired'
          ? code === 'CERT_HAS_EXPIRED'
          : ['UNABLE_TO_VERIFY_LEAF_SIGNATURE', 'UNABLE_TO_GET_ISSUER_CERT_LOCALLY'].includes(code)
    );
  } else if (mode !== 'cancel') {
    requestClosed = true;
    report();
  }
});

if (mode === 'post') {
  pending.write('upload ');
  pending.end('body');
}

if (mode === 'gc') {
  collect();
}
