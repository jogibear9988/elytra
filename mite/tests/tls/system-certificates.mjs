/** Query actual macOS anchors, then independently parse every advertised PEM. */
import assert from 'node:assert/strict';
import { X509Certificate } from 'node:crypto';
import { getCACertificates } from 'node:tls';

const roots = getCACertificates('system');
assert.equal(Array.isArray(roots), true);
assert.equal(Object.isFrozen(roots), true);
assert.equal(getCACertificates('system'), roots);
assert.ok(roots.length > 0 && roots.length <= 4096);
for (const pem of roots) {
  assert.equal(new X509Certificate(pem).toString(), pem);
}

assert.throws(() => roots.push('untrusted replacement'), TypeError);

assert.equal(getCACertificates('system'), roots);
console.log('PASS real system CA snapshot, immutable identity and canonical certificates');
