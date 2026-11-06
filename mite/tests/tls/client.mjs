/** Real TLS client assertions are shared verbatim by Node and Mite. */
import tls, { TLSSocket, connect } from 'node:tls';
import { Socket } from 'node:net';
import { readFileSync } from 'node:fs';
const [port, directory, mode, version] = process.argv.slice(2);
const controller = new AbortController();
const ca = readFileSync(directory + '/ca.pem');
const config = {
  host: '127.0.0.1',
  keepAlive: true,
  keepAliveInitialDelay: 1000,
  port: Number(port),
  servername:
    mode === 'wildcard'
      ? 'app.elytra.test'
      : mode === 'wildcard-mismatch'
        ? 'deep.app.elytra.test'
        : ['mismatch', 'permissive-host'].includes(mode)
          ? 'wrong.example'
          : mode === 'ip'
            ? ''
            : 'localhost',
  ...(mode === 'default-untrusted'
    ? {}
    : mode === 'untrusted'
      ? { ca: [] }
      : { ca: mode === 'array' ? [ca] : ca }),
  minVersion: version,
  maxVersion: version,
  ALPNProtocols: mode === 'no-alpn' ? [] : ['http/1.1'],
  ...(mode === 'permissive'
    ? { ca: [], rejectUnauthorized: false }
    : mode === 'permissive-host'
      ? { rejectUnauthorized: false }
      : {}),
  signal: controller.signal
};
let secure = false;
let valid = false;
let errors = 0;
let code = '';
let payload = 0;
const target = mode === 'large' ? 1024 * 1024 : 5;
const create =
  mode === 'array'
    ? (options, callback) => connect(Number(port), '127.0.0.1', options, callback)
    : connect;
const socket = create(config, () => {
  secure = true;
  valid =
    socket instanceof TLSSocket &&
    socket instanceof Socket &&
    socket.encrypted &&
    socket.getProtocol() === version &&
    socket.alpnProtocol === (mode === 'no-alpn' ? false : 'http/1.1') &&
    socket.authorized === !['permissive', 'permissive-host'].includes(mode) &&
    tls.connect === connect;
  if (['permissive', 'permissive-host'].includes(mode)) {
    valid &&= !!socket.authorizationError;
  }
  if (mode === 'gc') {
    (typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc)();
  }
  if (!['abort', 'early'].includes(mode)) {
    socket.write(mode === 'large' ? Buffer.alloc(target, 97) : Buffer.from('hello'));
  }
});
if (mode === 'early') {
  socket.write('hello');
}
if (mode === 'gc') {
  (typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc)();
}

socket.on('data', (data) => {
  if (mode === 'abort') {
    controller.abort('TLS cancelled');
    return;
  }
  for (const byte of data) {
    if (mode === 'large' && byte !== 97) {
      throw Error('Decrypted bytes were corrupted');
    }
  }
  payload += data.length;
  if (mode === 'large') {
    socket.pause();

    setImmediate(() => socket.resume());
  }
  if (payload === target) {
    socket.end();
  }
});

socket.on('error', (error) => {
  errors++;
  code = error.code;
  if (mode === 'abort' && (error.name !== 'AbortError' || error.cause !== 'TLS cancelled')) {
    throw error;
  }
});

socket.once('close', () => {
  const expectedFailures = {
    mismatch: ['ERR_TLS_CERT_ALTNAME_INVALID'],
    'wildcard-mismatch': ['ERR_TLS_CERT_ALTNAME_INVALID'],
    untrusted: ['UNABLE_TO_VERIFY_LEAF_SIGNATURE', 'UNABLE_TO_GET_ISSUER_CERT_LOCALLY'],
    expired: ['CERT_HAS_EXPIRED'],
    selfsigned: ['DEPTH_ZERO_SELF_SIGNED_CERT'],
    'default-untrusted': ['DEPTH_ZERO_SELF_SIGNED_CERT']
  };
  if (expectedFailures[mode]) {
    console.log('rejected', mode, !secure, errors === 1, expectedFailures[mode].includes(code));
  } else if (mode === 'abort') {
    console.log('cancelled', secure, valid, errors === 1 && code === 'ABORT_ERR');
  } else {
    console.log('TLS', mode, secure, valid, payload === target, errors === 0);
  }
});
