/** Each invocation is an isolated fatal path; it must never recursively recover. */
import { readFile } from 'node:fs';

const mode = process.argv[2];
const error = new Error(`controlled-fatal-${mode}`);

setTimeout(() => console.log('FORBIDDEN continuation'), 100);

if (mode === 'monitor-only') {
  process.on('uncaughtExceptionMonitor', (actual, origin) => {
    if (actual !== error || origin !== 'uncaughtException') {
      throw new Error('wrong-monitor-metadata');
    }
    console.log('MONITOR');
  });
} else if (mode === 'handler-throws' || mode === 'monitor-throws') {
  process.on('uncaughtExceptionMonitor', () => {
    console.log('MONITOR');
    if (mode === 'monitor-throws') {
      throw new Error('controlled-fatal-listener');
    }
  });

  process.on('uncaughtException', () => {
    console.log('HANDLER');
    throw new Error('controlled-fatal-listener');
  });
} else if (mode === 'rejection-handler-throws' || mode === 'handled-handler-throws') {
  process.on('uncaughtExceptionMonitor', (_error, origin) => console.log(`MONITOR:${origin}`));

  process.on('unhandledRejection', (actual, promise) => {
    if (actual !== error || promise !== original) {
      throw new Error('wrong-rejection-metadata');
    }
    console.log('REJECTION');
    if (mode === 'handled-handler-throws') {
      setTimeout(() => original.catch(() => {}), 0);

      return;
    }
    throw new Error('controlled-fatal-listener');
  });

  if (mode === 'handled-handler-throws') {
    process.on('rejectionHandled', (promise) => {
      if (promise !== original) {
        throw new Error('wrong-handled-promise');
      }
      console.log('HANDLED');
      throw new Error('controlled-fatal-listener');
    });
  }
} else if (mode === 'rejection-default-monitor') {
  process.on('uncaughtExceptionMonitor', (actual, origin) => {
    if (actual !== error || origin !== 'unhandledRejection') {
      throw new Error('wrong-rejection-origin');
    }
    console.log(`MONITOR:${origin}`);
  });
}

let original;

setTimeout(() => {
  if (mode.startsWith('rejection-') || mode === 'handled-handler-throws') {
    original = Promise.reject(error);
  } else if (mode === 'microtask') {
    queueMicrotask(() => {
      throw error;
    });
  } else if (mode === 'nextTick') {
    process.nextTick(() => {
      throw error;
    });
  } else if (mode === 'io') {
    readFile(new URL(import.meta.url), () => {
      throw error;
    });
  } else {
    throw error;
  }
}, 0);
