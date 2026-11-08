/** Real non-Apple provider: verify explicit unavailability without a plaintext fallback. */
import { safeStorage } from 'firefly';
import * as native from 'safe-storage-test';

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

check(process.platform !== 'darwin', 'Run only on actual non-Apple hosts');
check(safeStorage.isEncryptionAvailable() === false, 'Public OS provider unavailable');
check(native.safeStorageAvailable() === false, 'Native OS provider unavailable');
if (process.platform === 'linux') {
  check(safeStorage.getSelectedStorageBackend() === 'unknown', 'No basic_text fallback');
} else {
  check(safeStorage.getSelectedStorageBackend === undefined, 'Backend selector is Linux-only');
}
for (const action of [
  () => safeStorage.encryptString('secret'),
  () => safeStorage.decryptString(Buffer.alloc(0)),
  () => native.safeStorageEncrypt(new Uint8Array([1]))
]) {
  rejects(action, 'ERR_FIREFLY_SAFE_STORAGE_UNAVAILABLE');
}
check(
  safeStorage.setUsePlainTextEncryption(false) === undefined,
  'Disabling plaintext is harmless'
);

rejects(() => safeStorage.setUsePlainTextEncryption(true), 'ERR_FIREFLY_UNSUPPORTED_API');

check(safeStorage.isEncryptionAvailable() === false, 'Policy cannot enable an insecure fallback');
check((await safeStorage.isAsyncEncryptionAvailable()) === false, 'No asynchronous OS provider');
for (const operation of [
  () => safeStorage.encryptStringAsync('secret'),
  () => safeStorage.decryptStringAsync(Buffer.alloc(0))
]) {
  let error;
  try {
    await operation();
  } catch (value) {
    error = value;
  }
  check(error?.code === 'ERR_FIREFLY_UNSUPPORTED_API', 'Unsupported async crypto rejects');
}
console.log(
  'PASS public safeStorage unavailable: actual non-Apple provider, explicit errors, no plaintext fallback'
);
