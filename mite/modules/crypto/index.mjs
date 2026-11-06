/**
 * Assemble the realm-local node:crypto facade from entropy and digest services.
 * Native handles remain in the injected host; ESM and CommonJS share this result.
 */
import createRandomApi from './random.mjs';
import createDigestApi from './digest.mjs';

export default function createCryptoModule(host) {
  return { ...createRandomApi(host), ...createDigestApi(host) };
}
