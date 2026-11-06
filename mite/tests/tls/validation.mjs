/** Unsupported modes and malformed internal inputs must reject without opening transports. */
import * as tls from 'node:tls';
const cases = [
  [() => tls.createServer(), 'ERR_MITE_UNSUPPORTED_API'],
  [() => tls.createSecureContext(), 'ERR_MITE_UNSUPPORTED_API'],
  [() => new tls.TLSSocket(), 'ERR_MITE_UNSUPPORTED_API'],
  ...[
    [{ minVersion: '__proto__' }, 'ERR_TLS_INVALID_PROTOCOL_VERSION'],
    [{ maxVersion: 'TLSv1' }, 'ERR_TLS_INVALID_PROTOCOL_VERSION'],
    [{ minVersion: 'TLSv1.3', maxVersion: 'TLSv1.2' }, 'ERR_TLS_INVALID_PROTOCOL_VERSION'],
    [{ ca: 42 }, 'ERR_INVALID_ARG_TYPE'],
    [{ ca: 'malformed' }, 'ERR_TLS_INVALID_CA'],
    [{ ca: '\ud800' }, 'ERR_INVALID_ARG_VALUE'],
    [{ signal: {} }, 'ERR_INVALID_ARG_TYPE'],
    [{ ALPNProtocols: ['a'.repeat(256)] }, 'ERR_INVALID_ARG_VALUE'],
    [{ ALPNProtocols: Buffer.from([1, 97]) }, 'ERR_INVALID_ARG_TYPE'],
    [{ servername: '127.0.0.1' }, 'ERR_INVALID_ARG_VALUE'],
    [{ host: '127.1' }, 'ERR_INVALID_ARG_VALUE'],
    [{ host: '0177.0.0.1' }, 'ERR_INVALID_ARG_VALUE'],
    [{ host: '0x7f.0.0.1' }, 'ERR_INVALID_ARG_VALUE'],
    [{ host: '2130706433' }, 'ERR_INVALID_ARG_VALUE'],
    [{ servername: '127.1' }, 'ERR_INVALID_ARG_VALUE'],
    [{ timeout: '1' }, 'ERR_INVALID_ARG_TYPE'],
    [{ cert: '' }, 'ERR_MITE_UNSUPPORTED_API'],
    [{ secureOptions: 0 }, 'ERR_MITE_UNSUPPORTED_API'],
    [{ sigalgs: 'rsa_pss_rsae_sha256' }, 'ERR_MITE_UNSUPPORTED_API'],
    [{ ecdhCurve: 'X25519' }, 'ERR_MITE_UNSUPPORTED_API'],
    [{ requestOCSP: true }, 'ERR_MITE_UNSUPPORTED_API'],
    [{ allowPartialTrustChain: false }, 'ERR_MITE_UNSUPPORTED_API'],
    [{ localAddress: '127.0.0.1' }, 'ERR_MITE_UNSUPPORTED_API'],
    [{ path: '/tmp/tls-socket' }, 'ERR_MITE_UNSUPPORTED_API'],
    [{ handshakeTimeout: 1 }, 'ERR_MITE_UNSUPPORTED_API']
  ].map(([options, code]) => [() => tls.connect({ host: '127.0.0.1', port: 9, ...options }), code])
];
for (const [invoke, code] of cases) {
  try {
    invoke();
    throw Error('Invalid TLS option was accepted');
  } catch (error) {
    if (error.code !== code) {
      throw error;
    }
  }
}
console.log('TLS policy', cases.length);
