/** Optional manual Internet probe via the genuine native host; never a default CI dependency.
 * Run: firefly-net-native-probe firefly/tests/net/public-https.mjs [https://example.com/]
 * Owned loopback tests independently prove rejection of an untrusted certificate.
 */
import native from 'net-test';
import createNet from '../../core/net/index.mjs';

const failure = (code, message) => Object.assign(Error(message), { code });

const component = createNet(
  native,
  () => true,
  () => false,
  failure,
  (name) => {
    throw failure('ERR_FIREFLY_UNSUPPORTED_API', name);
  },
  native.transport
);
const endpoint = new URL(process.argv[2] || 'https://example.com/');
if (endpoint.protocol !== 'https:') {
  throw Error('Public positive TLS probe requires HTTPS');
}
const controller = new AbortController();
const deadline = setTimeout(() => controller.abort(), 15000);
try {
  const response = await component.net.fetch(endpoint.href, { signal: controller.signal });
  const body = await response.text();
  if (!response.ok || !body.length) {
    throw Error('Verified public HTTPS did not return successful content');
  }
  console.log(
    'PASS optional actual net default-trust HTTPS status=' +
      response.status +
      ' decodedCharacters=' +
      body.length
  );
} finally {
  clearTimeout(deadline);
  component.stop();
}
