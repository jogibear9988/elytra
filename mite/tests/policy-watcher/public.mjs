/** Real extension and OS observation. No preference mutation or synthetic initial snapshot. */
import watcher, { createWatcher } from 'mite:policy-watcher';
import { createRequire } from 'node:module';
import { randomUUID } from 'node:crypto';

const require = createRequire(import.meta.url);
if (watcher !== require('mite:policy-watcher') || createWatcher !== watcher.createWatcher) {
  throw Error('Policy extension ESM/CommonJS singleton mismatch');
}

const domain = 'com.elytra.policy.probe.' + randomUUID().replaceAll('-', '');
if (process.platform !== 'darwin') {
  let failure;
  try {
    createWatcher(domain, { UnconfiguredProbe: { type: 'boolean' } }, () => {
      throw Error('Unsupported provider returned invented policy data');
    });
  } catch (error) {
    failure = error;
  }
  if (failure?.code !== 'ERR_MITE_UNSUPPORTED_API') {
    throw Error('Unsupported native policy provider must fail explicitly');
  }
  console.log('PASS public policy extension singleton and explicitly unavailable native platform');
} else {
  let handle;
  let synchronous = true;
  let calls = 0;
  const timeout = setTimeout(() => {
    console.error('Public native policy snapshot timed out');
    process.exit(1);
  }, 10000);
  await new Promise((resolve, reject) => {
    handle = createWatcher(domain, { UnconfiguredProbe: { type: 'boolean' } }, (values) => {
      try {
        if (synchronous || ++calls !== 1 || Object.keys(values).length !== 0) {
          throw Error('Actual unique-domain observation must be asynchronous and initially empty');
        }
        handle.dispose();
        handle.dispose();
        clearTimeout(timeout);
        resolve();
      } catch (error) {
        reject(error);
      }
    });
    synchronous = false;
  });
  console.log(
    'PASS public policy extension singleton and real asynchronous read-only macOS observation'
  );
}
