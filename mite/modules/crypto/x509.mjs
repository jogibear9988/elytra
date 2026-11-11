/** Canonical certificate parsing only; policy/trust and private-key operations are not implied. */
import { fail, wrapBytes } from './bytes.mjs';

export default function createX509(host) {
  const records = new WeakMap();
  const Buffer = host.Buffer;
  const byteView = host.encoding?.byteView;
  const isView = ArrayBuffer.isView;
  const limit = 1024 * 1024;

  function state(value) {
    const record = records.get(value);
    if (!record) {
      fail('X509Certificate receiver', TypeError, 'ERR_INVALID_THIS');
    }
    return record;
  }

  function unsupported(value, operation) {
    state(value);
    const error = new Error('X509Certificate.' + operation + ' is not implemented');
    error.code = 'ERR_MITE_UNSUPPORTED_API';
    throw error;
  }

  class X509Certificate {
    constructor(input) {
      let bytes;
      if (typeof input === 'string') {
        if (input.length > limit || Buffer.byteLength(input, 'utf8') > limit) {
          fail('certificate size', RangeError, 'ERR_OUT_OF_RANGE');
        }
        bytes = Buffer.from(input, 'utf8');
      } else if (isView(input)) {
        bytes = Reflect.apply(byteView, host.encoding, [input]);
        if (bytes.byteLength > limit) {
          fail('certificate size', RangeError, 'ERR_OUT_OF_RANGE');
        }
      } else {
        fail('certificate', TypeError, 'ERR_INVALID_ARG_TYPE');
      }
      if (!host.native.x509 || typeof host.native.x509.parse !== 'function') {
        const error = new Error('Native X509 certificate parser is unavailable');
        error.code = 'ERR_MITE_UNSUPPORTED_API';
        throw error;
      }
      const parsed = host.native.x509.parse(bytes);
      records.set(this, { pem: parsed.pem, raw: wrapBytes(Buffer, parsed.raw) });
    }

    toString() {
      return state(this).pem;
    }

    toJSON() {
      return state(this).pem;
    }

    get raw() {
      return state(this).raw;
    }
  }
  for (const name of [
    'subject',
    'subjectAltName',
    'issuer',
    'issuerCertificate',
    'validFrom',
    'validTo',
    'validFromDate',
    'validToDate',
    'fingerprint',
    'fingerprint256',
    'fingerprint512',
    'keyUsage',
    'serialNumber',
    'publicKey',
    'ca',
    'infoAccess'
  ]) {
    Object.defineProperty(X509Certificate.prototype, name, {
      configurable: true,

      get() {
        return unsupported(this, name);
      }
    });
  }
  for (const name of [
    'checkHost',
    'checkEmail',
    'checkIP',
    'checkIssued',
    'checkPrivateKey',
    'verify',
    'toLegacyObject'
  ]) {
    Object.defineProperty(X509Certificate.prototype, name, {
      configurable: true,
      writable: true,

      value() {
        return unsupported(this, name);
      }
    });
  }
  return { X509Certificate };
}
