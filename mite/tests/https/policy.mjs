/** Unsupported TLS policies must reach validation through HTTPS rather than disappear. */
import https from 'node:https';
const cases = [
  () => new https.Agent(),
  () => new https.Server(),
  () => https.createServer(),
  ...[
    { ALPNProtocols: ['h2'] },
    { secureOptions: 0 },
    { sigalgs: 'rsa_pss_rsae_sha256' },
    { ecdhCurve: 'X25519' },
    { requestOCSP: true },
    { cert: '' },
    { checkServerIdentity() {} },
    { secureContext: {} },
    { createConnection() {} }
  ].map((options) => () => https.request({ host: '127.0.0.1', port: 9, ...options }))
];
for (const invoke of cases) {
  try {
    invoke();
    throw Error('Unsupported HTTPS option ignored');
  } catch (error) {
    if (error.code !== 'ERR_MITE_UNSUPPORTED_API') {
      throw error;
    }
  }
}
const authorities = [
  { host: '127.1' },
  { host: '127.0.0.1', headers: { host: '127.1' } },
  { host: '127.0.0.1', servername: '0x7f.0.0.1' }
];
for (const options of authorities) {
  try {
    https.request({ port: 9, ...options });
    throw Error('Ambiguous TLS authority accepted');
  } catch (error) {
    if (error.code !== 'ERR_INVALID_ARG_VALUE') {
      throw error;
    }
  }
}
console.log('HTTPS policy', cases.length + authorities.length);
