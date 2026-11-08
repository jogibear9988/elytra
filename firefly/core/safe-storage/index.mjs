/** Main-only OS-key-backed authenticated encryption. Ciphertext is Firefly-specific. */
export default function createSafeStorage(native, app, error, unsupported) {
  const BufferClass = globalThis.Buffer;
  const isBuffer = BufferClass.isBuffer.bind(BufferClass);
  const byteLength = BufferClass.byteLength.bind(BufferClass);
  const from = BufferClass.from.bind(BufferClass);
  const limit = 1024 * 1024;
  const apply = Reflect.apply;
  const viewLength = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Uint8Array.prototype),
    'byteLength'
  ).get;

  function invalid(message) {
    return Object.assign(new TypeError(message), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  function ready() {
    if (!app.isReady()) {
      throw error('ERR_FIREFLY_NOT_READY', 'safeStorage requires app readiness');
    }
  }

  function available() {
    return (
      app.isReady() &&
      typeof native.safeStorageAvailable === 'function' &&
      native.safeStorageAvailable()
    );
  }

  function requireEncryption() {
    ready();
    if (!available()) {
      throw error('ERR_FIREFLY_SAFE_STORAGE_UNAVAILABLE', 'OS-backed encryption is unavailable');
    }
  }

  const storage = {
    isEncryptionAvailable: available,

    encryptString(plainText) {
      if (typeof plainText !== 'string') {
        throw invalid('safeStorage.encryptString requires a string');
      }
      if (plainText.length > limit || byteLength(plainText, 'utf8') > limit) {
        throw error('ERR_OUT_OF_RANGE', 'safeStorage plaintext exceeds its byte budget');
      }
      requireEncryption();
      return from(native.safeStorageEncrypt(from(plainText, 'utf8')));
    },

    decryptString(encrypted) {
      requireEncryption();
      if (!isBuffer(encrypted)) {
        throw invalid('safeStorage.decryptString requires a Buffer');
      }
      const length = apply(viewLength, encrypted, []);
      if (length > limit + 64) {
        throw error('ERR_OUT_OF_RANGE', 'safeStorage ciphertext exceeds its byte budget');
      }
      // Electron permits an empty Buffer after checking encryption availability.
      if (length === 0) {
        return '';
      }
      return from(native.safeStorageDecrypt(encrypted)).toString('utf8');
    },

    setUsePlainTextEncryption(value) {
      if (typeof value !== 'boolean') {
        throw invalid('Plaintext policy must be a boolean');
      }
      if (value) {
        unsupported('Plaintext key storage');
      }
      // Disabling plaintext is the only policy: there is no fallback to turn off.
    },

    isAsyncEncryptionAvailable() {
      return Promise.resolve(false);
    },

    encryptStringAsync() {
      return Promise.reject(
        error('ERR_FIREFLY_UNSUPPORTED_API', 'Async OS encryption is not implemented')
      );
    },

    decryptStringAsync() {
      return Promise.reject(
        error('ERR_FIREFLY_UNSUPPORTED_API', 'Async OS decryption is not implemented')
      );
    }
  };

  if (process.platform === 'linux') {
    storage.getSelectedStorageBackend = () => 'unknown';
  }
  return storage;
}
