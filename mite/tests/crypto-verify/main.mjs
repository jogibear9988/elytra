/** Owned public P-256 vector, actual cryptographic validation and snapshot isolation. */
import { createPublicKey, verify } from 'node:crypto';
import { readFileSync } from 'node:fs';
import path from 'node:path';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const vector = JSON.parse(readFileSync(path.join(import.meta.dirname, 'vector.json'), 'utf8'));
const source = { ...vector.jwk, kid: 'owned-test', alg: 'ES256', use: 'sig' };
const key = createPublicKey({ key: source, format: 'jwk' });
const wrong = createPublicKey({ key: vector.wrong, format: 'jwk' });
const options = { key, dsaEncoding: 'ieee-p1363' };
const message = Buffer.from(vector.message);
const signature = Buffer.from(vector.signature, 'base64url');
check(
  key.type === 'public' &&
    key.asymmetricKeyType === 'ec' &&
    key.asymmetricKeyDetails.namedCurve === 'prime256v1',
  'Real public EC key identity'
);
check(verify('sha256', message, options, signature), 'Genuine valid ES256 signature');
source.x = 'A'.repeat(43);
source.y = 'A'.repeat(43);
check(verify('sha256', message, options, signature), 'Key snapshot independent of JWK mutation');
const altered = Buffer.from(signature);
altered[0] ^= 1;
check(!verify('sha256', message, options, altered), 'Tampered signature fails');
check(
  !verify('sha256', Buffer.from('different message'), options, signature),
  'Tampered signed data fails'
);
check(
  !verify('sha256', message, { key: wrong, dsaEncoding: 'ieee-p1363' }, signature),
  'Wrong real public key fails'
);
check(!verify('sha256', message, options, Buffer.alloc(64)), 'Zero r/s is invalid');
check(
  !verify('sha256', message, options, signature.subarray(0, 63)),
  'Wrong signature size is invalid'
);
const backing = Buffer.alloc(message.length + 16, 88);
message.copy(backing, 8);
check(
  verify(
    'sha256',
    new DataView(backing.buffer, backing.byteOffset + 8, message.length),
    options,
    signature
  ),
  'Exact data view offset/range'
);
let rejected = false;
try {
  createPublicKey({ key: { ...vector.jwk, x: 'A'.repeat(43), y: 'A'.repeat(43) }, format: 'jwk' });
} catch {
  rejected = true;
}
check(rejected, 'Actual off-curve key rejected by provider');
if (process.versions.mite) {
  const fails = (action, code) => {
    try {
      action();
    } catch (error) {
      check(error.code === code, 'Expected explicit error code');
      return;
    }
    throw Error('Expected rejection');
  };

  fails(() => verify('sha384', message, options, signature), 'ERR_MITE_UNSUPPORTED_API');

  fails(
    () => verify('sha256', message, { key: {}, dsaEncoding: 'ieee-p1363' }, signature),
    'ERR_INVALID_ARG_TYPE'
  );

  fails(
    () => verify('sha256', message, { key, dsaEncoding: 'der' }, signature),
    'ERR_MITE_UNSUPPORTED_API'
  );

  fails(
    () => verify('sha256', Buffer.alloc(1024 * 1024 + 1), options, signature),
    'ERR_OUT_OF_RANGE'
  );

  fails(() => key.export({ format: 'der', type: 'spki' }), 'ERR_MITE_UNSUPPORTED_API');
}
console.log(
  'PASS real P-256 JWK/ES256 verification, invalid points, tampering, wrong key and immutable key ownership'
);
