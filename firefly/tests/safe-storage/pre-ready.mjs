/** Actual desktop main, but no windows or post-ready keychain access. */
import { app, safeStorage } from 'firefly';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const limited = process.versions.electron === undefined;
check(!app.isReady(), 'Entrypoint runs before ready');
check(safeStorage.isEncryptionAvailable() === false, 'Pre-ready encryption unavailable');
for (const action of [
  () => safeStorage.encryptString('text'),
  () => safeStorage.decryptString(Buffer.alloc(0))
]) {
  let failure;
  try {
    action();
  } catch (error) {
    failure = error;
  }
  check(failure instanceof Error, 'Pre-ready operation must fail');
  if (limited) {
    check(failure.code === 'ERR_FIREFLY_NOT_READY', 'Explicit Firefly readiness error');
  }
}

Promise.all([
  safeStorage
    .isAsyncEncryptionAvailable()
    .then((value) => check(value === false, 'No async encryption before ready')),
  safeStorage.encryptStringAsync('text').then(
    () => {
      throw Error('Unexpected async encryption');
    },
    (error) => check(error instanceof Error, 'Async encryption rejection')
  ),
  safeStorage.decryptStringAsync(Buffer.alloc(0)).then(
    () => {
      throw Error('Unexpected async decryption');
    },
    (error) => check(error instanceof Error, 'Async decryption rejection')
  )
])
  .then(() => {
    console.log('PASS actual main safeStorage pre-ready guard; no post-ready keychain access');
    app.exit(0);
  })
  .catch((error) => {
    console.error(error.stack);
    app.exit(1);
  });

setTimeout(() => app.exit(1), 3000);
