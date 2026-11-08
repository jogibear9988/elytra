/** Real public facade plus native OS provider, hosted in an explicit private test keychain. */
import { safeStorage } from 'firefly';
import * as native from 'safe-storage-test';
import fs from 'node:fs';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const rejects = (action, code) => {
  let error;
  try {
    action();
  } catch (value) {
    error = value;
  }
  check(error instanceof Error && error.code === code, 'Expected explicit ' + code);
};

const invalid = 'ERR_FIREFLY_SAFE_STORAGE_INVALID_DATA';
const phase = process.argv[2];
const file = process.argv[3];
const originals = ['', 'Grüße 🦋 €', 'embedded\0NUL', '\ud800', '\udc00', 'a\ud800b'];

const expected = ['', 'Grüße 🦋 €', 'embedded\0NUL', '\ufffd', '\ufffd', 'a\ufffdb'];

check(safeStorage.isEncryptionAvailable() === true, 'Real OS encryption must be available');

if (phase === 'read' || phase === 'wrong-key') {
  const records = JSON.parse(fs.readFileSync(file, 'utf8'));
  if (phase === 'wrong-key') {
    // Make the other profile own an actual different OS key, rather than test a missing key.
    safeStorage.encryptString('independent profile key');
  }
  for (const record of records) {
    check(record.encrypted.type === 'Buffer', 'Persisted ciphertext follows Buffer JSON protocol');
    const encrypted = Buffer.from(record.encrypted.data);
    if (phase === 'wrong-key') {
      rejects(() => safeStorage.decryptString(encrypted), invalid);
    } else {
      check(
        safeStorage.decryptString(encrypted) === record.expected,
        'Decrypt after native restart'
      );
    }
  }
  console.log('PASS public safeStorage ' + phase);
} else {
  check(
    phase === 'write' && typeof file === 'string',
    'Require write/read/wrong-key and state file'
  );
  const records = originals.map((plain, index) => {
    const encrypted = safeStorage.encryptString(plain);
    check(Buffer.isBuffer(encrypted), 'Public encrypt returns a Buffer');
    check(safeStorage.decryptString(encrypted) === expected[index], 'UTF-8 string roundtrip');
    return { encrypted, expected: expected[index] };
  });
  fs.writeFileSync(file, JSON.stringify(records));
  check(safeStorage.decryptString(Buffer.alloc(0)) === '', 'Electron empty Buffer convention');
  const cipher = safeStorage.encryptString('authenticated payload');
  const shadowed = Buffer.from(cipher);
  Object.defineProperty(shadowed, 'length', { value: 0 });
  check(
    safeStorage.decryptString(shadowed) === 'authenticated payload',
    'Own length properties cannot bypass ciphertext validation'
  );
  check(!cipher.equals(safeStorage.encryptString('authenticated payload')), 'Fresh random nonce');
  // Version prefix, nonce, ciphertext and tag are all protected; no partial plaintext escapes.
  for (const position of [0, 4, 8, 20, cipher.length - 1]) {
    const corrupted = Buffer.from(cipher);
    corrupted[position] ^= 1;

    rejects(() => safeStorage.decryptString(corrupted), invalid);
  }
  for (const malformed of [
    cipher.subarray(0, 7),
    cipher.subarray(0, cipher.length - 1),
    Buffer.concat([cipher, Buffer.from([0])]),
    Buffer.from('v10foreign-electron-ciphertext'),
    Buffer.from('plaintext')
  ]) {
    rejects(() => safeStorage.decryptString(malformed), invalid);
  }
  for (const value of [undefined, null, 1, {}, [], new String('boxed')]) {
    rejects(() => safeStorage.encryptString(value), 'ERR_INVALID_ARG_TYPE');
  }
  for (const value of [undefined, null, 'text', {}, new Uint8Array(0), new ArrayBuffer(0)]) {
    rejects(() => safeStorage.decryptString(value), 'ERR_INVALID_ARG_TYPE');
  }

  rejects(() => safeStorage.encryptString('a'.repeat(1024 * 1024 + 1)), 'ERR_OUT_OF_RANGE');

  rejects(() => safeStorage.encryptString('ä'.repeat(600000)), 'ERR_OUT_OF_RANGE');

  rejects(() => safeStorage.decryptString(Buffer.alloc(1024 * 1024 + 65)), 'ERR_OUT_OF_RANGE');

  check(safeStorage.setUsePlainTextEncryption(false) === undefined, 'OS-only storage policy');

  rejects(() => safeStorage.setUsePlainTextEncryption(true), 'ERR_FIREFLY_UNSUPPORTED_API');

  rejects(() => safeStorage.setUsePlainTextEncryption('false'), 'ERR_INVALID_ARG_TYPE');

  check((await safeStorage.isAsyncEncryptionAvailable()) === false, 'Async backend unavailable');
  for (const promise of [
    safeStorage.encryptStringAsync('text'),
    safeStorage.decryptStringAsync(cipher)
  ]) {
    let failure;
    try {
      await promise;
    } catch (error) {
      failure = error;
    }
    check(failure?.code === 'ERR_FIREFLY_UNSUPPORTED_API', 'Async methods reject explicitly');
  }
  // The native boundary must enforce readiness and byte types even if callers bypass the facade.
  native.markReady(false);
  check(native.safeStorageAvailable() === false, 'Native availability respects readiness');

  rejects(() => native.safeStorageEncrypt(new Uint8Array(0)), 'ERR_FIREFLY_NOT_READY');

  rejects(() => native.safeStorageDecrypt(cipher), 'ERR_FIREFLY_NOT_READY');

  native.markReady(true);
  check(native.safeStorageAvailable() === true, 'Native readiness restored');
  for (const value of [undefined, null, 'bytes', {}, new DataView(new ArrayBuffer(0))]) {
    rejects(() => native.safeStorageEncrypt(value), 'ERR_INVALID_ARG_TYPE');

    rejects(() => native.safeStorageDecrypt(value), 'ERR_INVALID_ARG_TYPE');
  }

  rejects(() => native.safeStorageEncrypt(new Uint8Array(1024 * 1024 + 1)), 'ERR_OUT_OF_RANGE');

  rejects(() => native.safeStorageDecrypt(new Uint8Array([0xff])), invalid);

  const bytes = new Uint8Array([0xff, 0xc0, 0, 0x41]);
  const encryptedBytes = native.safeStorageEncrypt(bytes);
  check(
    Buffer.from(native.safeStorageDecrypt(encryptedBytes)).equals(Buffer.from(bytes)),
    'Native hook preserves arbitrary bytes without interpreting UTF-8'
  );
  check(
    safeStorage.decryptString(Buffer.from(encryptedBytes)) === '\ufffd\ufffd\0A',
    'Public text decoding handles malformed UTF-8 with replacement'
  );
  console.log(
    'PASS public safeStorage write: native OS key, Buffer JSON, UTF-8, nonce/authentication, limits and guards'
  );
}
