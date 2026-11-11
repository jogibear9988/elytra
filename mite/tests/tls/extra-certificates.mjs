/** Spawn with unset/valid/invalid NODE_EXTRA_CA_CERTS to test the native startup snapshot. */
import assert from 'node:assert/strict';
import tls from 'node:tls';
import { readFileSync } from 'node:fs';
import { X509Certificate } from 'node:crypto';
const mode = process.argv[2] ?? 'unset';
if (mode === 'invalid') {
  assert.throws(() => tls.getCACertificates('extra'), { code: 'ERR_TLS_EXTRA_CA_CERTS' });

  console.log('PASS configured invalid extra certificates fail explicitly');
} else {
  const expected =
    mode === 'valid'
      ? [new X509Certificate(readFileSync(process.env.NODE_EXTRA_CA_CERTS)).toString()]
      : [];
  assert.deepEqual(tls.getCACertificates('extra'), expected);
  const copy = tls.getCACertificates('extra');
  assert.equal(Object.isFrozen(copy), true);

  assert.throws(() => copy.push('destination-only'), TypeError);

  assert.equal(tls.getCACertificates('extra'), copy);
  process.env.NODE_EXTRA_CA_CERTS = 'must-not-reload-after-startup';
  assert.deepEqual(tls.getCACertificates('extra'), expected);
  console.log('PASS actual extra certificate snapshot and immutable query cache');
}
