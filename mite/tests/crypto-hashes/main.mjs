/** Compare public discovery contracts; exact catalogs legitimately depend on OpenSSL providers. */
import crypto, { createHash, getHashes } from 'node:crypto';
import { createRequire } from 'node:module';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const require = createRequire(import.meta.url);
check(
  crypto.getHashes === getHashes && require('crypto').getHashes === getHashes,
  'Shared ESM/CommonJS crypto function identity'
);
const names = getHashes();
check(Array.isArray(names) && names.length > 0, 'Actual provider names are available');
check(
  names.every((name) => typeof name === 'string' && name.length > 0 && name.length <= 256),
  'Bounded actual algorithm names'
);
check(
  new Set(names).size === names.length &&
    JSON.stringify(names) === JSON.stringify([...names].sort()),
  'Unique sorted native registry'
);
const stable = [...names];
names[0] = 'not-an-actual-digest';
names.push('must-not-be-cached');
check(
  JSON.stringify(getHashes()) === JSON.stringify(stable),
  'Mutation never changes the provider catalog'
);
let coercions = 0;
check(
  JSON.stringify(
    getHashes({
      toString() {
        coercions++;
        throw Error('Argument coercion');
      }
    })
  ) === JSON.stringify(stable) && coercions === 0,
  'Node-compatible ignored arguments'
);
const expected = {
  sha256: 'ba7816bf8f01cfea414140de5dae2223b00361a396177a9cb410ff61f20015ad',
  sha384:
    'cb00753f45a35e8bb5a03d699ac65007272c32ab0eded1631a8b605a43ff5bed8086072ba1e7cc2358baeca134c825a7',
  sha512:
    'ddaf35a193617abacc417349ae20413112e6fa4e89a97ea20a9eeee64b55d39a2192992a274fc1a836ba3c23a3feebbd454d4423643ce80e2a9ac94fa54ca49f'
};
for (const [algorithm, digest] of Object.entries(expected)) {
  check(stable.includes(algorithm), 'SRI algorithm actually supported: ' + algorithm);
  check(
    createHash(algorithm).update('abc').digest('hex') === digest,
    'Actual standard SHA-2 digest: ' + algorithm
  );
}
for (const name of stable) {
  const digest = createHash(name).update('owned provider enumeration').digest();
  check(Buffer.isBuffer(digest), 'Every advertised alias creates a real usable digest: ' + name);
}
console.log(
  'PASS getHashes real provider algorithms, sorted independent snapshots, SRI digests and ignored arguments'
);
