/**
 * Assemble the realm-local node:crypto facade from entropy and digest services.
 * Native handles remain in the injected host; ESM and CommonJS share this result.
 */
import createRandomApi from './random.mjs';
import createDigestApi from './digest.mjs';
import createWebCrypto from './web-crypto.mjs';
import createX509 from './x509.mjs';

export default function createCryptoModule(host) {
  return {
    ...createRandomApi(host),
    ...createDigestApi(host),
    ...createWebCrypto(host),
    ...createX509(host)
  };
}
