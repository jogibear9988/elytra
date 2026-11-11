/** Actual certificate bytes; parser/canonicalization only, never trust validation. */
import { X509Certificate, createHash } from 'node:crypto';
import { readFileSync } from 'node:fs';
import path from 'node:path';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const failures = (action, Type = Error) => {
  try {
    action();
  } catch (error) {
    check(error instanceof Type, 'Expected actual typed error');
    return;
  }
  throw Error('Expected rejection');
};

const input = readFileSync(path.join(import.meta.dirname, '../tls/fixtures/ca.pem'));
const certificate = new X509Certificate(input);
const pem = certificate.toString();
check(
  pem.startsWith('-----BEGIN CERTIFICATE-----\n') && pem.endsWith('-----END CERTIFICATE-----\n'),
  'Canonical PEM'
);
check(
  certificate.toJSON() === pem && JSON.stringify(certificate) === JSON.stringify(pem),
  'Genuine canonical JSON representation'
);
check(
  Buffer.isBuffer(certificate.raw) && certificate.raw === certificate.raw,
  'Cached DER Buffer identity'
);
check(new X509Certificate(certificate.raw).toString() === pem, 'Real DER parsing');
const backing = Buffer.alloc(input.length + 16, 42);
input.copy(backing, 8);
for (const view of [
  new Uint8Array(backing.buffer, backing.byteOffset + 8, input.length),
  new DataView(backing.buffer, backing.byteOffset + 8, input.length)
]) {
  check(new X509Certificate(view).toString() === pem, 'Exact view offset/range');
}
const copy = Buffer.from(input);
const independent = new X509Certificate(copy);
copy.fill(0);
check(independent.toString() === pem, 'Input snapshot survives mutation');
const raw = Buffer.from(certificate.raw);
certificate.raw.fill(0);
check(
  certificate.toString() === pem && new X509Certificate(raw).toString() === pem,
  'Mutable public raw does not mutate parsed certificate'
);
check(
  new X509Certificate(pem.replaceAll('\n', '\r\n')).toString() === pem,
  'PEM newline normalization'
);

failures(() => new X509Certificate('not a certificate'));

failures(() => new X509Certificate(raw.subarray(0, raw.length - 1)));

for (const value of [undefined, null, 1, {}, [], raw.buffer]) {
  failures(() => new X509Certificate(value), TypeError);
}

failures(() => X509Certificate.prototype.toString.call({}), TypeError);

if (process.versions.mite) {
  // Mite deliberately requires the actual branded receiver, including toJSON.
  failures(() => X509Certificate.prototype.toJSON.call(new Proxy(certificate, {})), TypeError);

  failures(() => new X509Certificate(Buffer.alloc(1024 * 1024 + 1)), RangeError);

  try {
    independent.verify({});
    throw Error('Unsupported operation unexpectedly succeeds');
  } catch (error) {
    check(
      error.code === 'ERR_MITE_UNSUPPORTED_API',
      'Explicit unsupported certificate verification'
    );
  }
}
console.log(
  'PASS X509 native PEM/DER canonicalization, binary ownership, JSON and validation ' +
    createHash('sha256').update(pem).digest('hex')
);
