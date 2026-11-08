/** The same Firefly binary must enter plain Mite before any Qt application/plugin startup. */
import { createRequire } from 'node:module';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const require = createRequire(import.meta.url);
let fireflyUnavailable = false;
try {
  require('firefly');
} catch (error) {
  fireflyUnavailable = typeof error.code === 'string';
}
check(fireflyUnavailable, 'Fork child must not expose Firefly desktop API');
check(process.parentPort === undefined, 'Fork child is not a Firefly utility process');
check(
  typeof process.versions.mite === 'string' && process.versions.v8 === undefined,
  'Child executes native Mite/LibJS'
);
check(
  process.env.QT_QPA_PLATFORM === 'elytra-no-gui',
  'Invalid Qt plugin negative control reaches child'
);
check(
  process.argv[2] === 'fork-boundary',
  'Internal fork role flags do not leak into application argv'
);

process.on('message', (value) => {
  check(
    value?.text === 'plain Mite child' && value?.nested?.count === 7,
    'Parent IPC snapshot reaches child'
  );

  process.send({ role: 'plain-mite', fireflyUnavailable, parentPort: false, value }, () =>
    process.disconnect()
  );
});

process.send({ ready: true });
const watchdog = setTimeout(() => {
  throw Error('Firefly fork child timed out');
}, 5000);
watchdog.unref();
