/** Deliberate Mite boundaries: no synthetic lifecycle support or foreign handlers. */
import { constants } from 'node:os';
const mode = process.argv[2];

function denied(type, code) {
  try {
    process.on(type, () => {});
  } catch (error) {
    if (error.code === code && process.listenerCount(type) === 0) {
      return;
    }
    throw error;
  }
  throw new Error(`Accepted ${type}`);
}

if (mode === 'host') {
  denied('SIGINT', 'ERR_MITE_SIGNAL_HOST_POLICY');
} else if (mode === 'protected') {
  denied('SIGUSR2', 'ERR_MITE_SIGNAL_OWNED');
} else if (mode === 'ignore') {
  // Leave a live subscription for native shutdown to restore the ignored disposition.
  process.on('SIGUSR2', () => {
    throw new Error('Ignored signal delivered');
  });

  process.removeAllListeners('SIGUSR2');

  process.once('SIGUSR2', () => {});
} else {
  const marker = new Error('newListener failed');

  function reject(type) {
    if (type === 'SIGINT') {
      throw marker;
    }
  }

  process.on('newListener', reject);
  try {
    process.on('SIGINT', () => {});

    throw new Error('Missing meta exception');
  } catch (error) {
    if (error !== marker) {
      throw error;
    }
  }
  process.off('newListener', reject);
  if (process.listenerCount('SIGINT') !== 0) {
    throw new Error('Failed subscription retained');
  }
  for (const type of ['beforeExit', 'warning', 'message']) {
    denied(type, 'ERR_MITE_UNSUPPORTED_PROCESS_EVENT');
  }
  for (const type of ['SIGKILL', 'SIGCHLD', 'SIGSEGV']) {
    if (Object.hasOwn(constants.signals, type)) {
      denied(type, 'ERR_MITE_UNSUPPORTED_SIGNAL');
    }
  }

  const listener = () => {};
  // The failed meta-listener subscription also owns one deferred close slot.
  for (let i = 0; i < 31; ++i) {
    process.on('SIGINT', listener);
    process.off('SIGINT', listener);
  }
  denied('SIGINT', 'ERR_MITE_SIGNAL_LIMIT');

  setTimeout(() => {
    process.on('SIGINT', listener);
    process.off('SIGINT', listener);
    console.log('PASS pending signal handle credit recovered');
  }, 20);
}
console.log(`PASS process event policy ${mode}`);
