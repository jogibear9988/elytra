/** Real stream writes, callback ownership and explicit partial Console surface. */
import { Console } from 'node:console';
import { Writable } from 'node:stream';
import createConsoleModule from '../../modules/core/console.mjs';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const out = [];
const err = [];

const stream = (result) =>
  new Writable({
    write(chunk, encoding, callback) {
      result.push(chunk.toString());
      callback();
    }
  });

const stdout = stream(out);
const stderr = stream(err);
const logger = new Console({ stdout, stderr, ignoreErrors: false });
const detached = logger.log;
detached('hello %s %d', 'stream', 7);
logger.info('info');
logger.debug('debug');
logger.warn('warn');
logger.error('error');
logger.assert(true, 'must not print');
logger.assert(false, 'bad %s', 'argument');
check(
  out.join('') === 'hello stream 7\ninfo\ndebug\n',
  'Real stdout stream/format/detached binding'
);
check(
  err.join('') === 'warn\nerror\nAssertion failed: bad argument\n',
  'Real stderr/assert streams'
);
check(stdout.listenerCount('error') === 0, 'No owned listener leak after successful writes');
const failing = stream([]);

failing.write = () => {
  throw Error('owned write failed');
};

const ignored = new Console({ stdout: failing });
ignored.log('ignored transport failure');
if (process.versions.mite) {
  let unsupported = false;
  try {
    logger.table([{ actual: 1 }]);
  } catch (error) {
    unsupported = error.code === 'ERR_MITE_UNSUPPORTED_API';
  }
  check(unsupported, 'No fabricated table implementation');
}

const originalLog = () => {};

const originalError = () => {};

let getterReads = 0;
const captured = { log: originalLog };

const wrapper = () => {
  getterReads++;
  return originalError;
};

Object.defineProperty(captured, 'error', { configurable: true, get: wrapper });
const facade = createConsoleModule({ process: { stdout, stderr }, console: captured });
check(
  facade === captured && facade.log === originalLog,
  'Global/module console identity and existing pipe log preserved'
);
check(
  getterReads === 0 && Object.getOwnPropertyDescriptor(captured, 'error').get === wrapper,
  'Existing property wrapper preserved without evaluation'
);
check(
  typeof facade.Console === 'function' &&
    captured.Console === facade.Console &&
    facade.Console.name === 'Console',
  'Actual Console constructor attached to shared object'
);
console.log(
  'PASS Console actual stdout/stderr, formatting, detached methods, assert and stream write failure policy'
);
