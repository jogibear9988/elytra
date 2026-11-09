/** Intentional CommonJS control: synchronous entry errors are not Promise failures. */
const error = new Error('controlled-commonjs-entry');
let monitored = false;

process.on('uncaughtExceptionMonitor', (actual, origin) => {
  if (actual !== error || origin !== 'uncaughtException') {
    throw new Error('wrong-commonjs-monitor');
  }
  monitored = true;
});

process.once('uncaughtException', (actual, origin) => {
  if (!monitored || actual !== error || origin !== 'uncaughtException') {
    throw new Error('wrong-commonjs-handler');
  }

  setTimeout(
    () => console.log('PASS process CommonJS entry recovery retains synchronous origin'),
    0
  );
});

throw error;
