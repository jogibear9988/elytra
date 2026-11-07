/** Public JS policy against a fixture provider; native/OS enforcement has separate gates. */
import createApi from '../../core/api.mjs';
import { EventEmitter } from 'node:events';
import path from 'node:path';
import * as urls from 'node:url';

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

function fails(action, code) {
  let actual;
  try {
    action();
  } catch (error) {
    actual = error.code;
  }
  check(actual === code, `Expected ${code}, got ${actual}`);
}

function fixture(refuse = false) {
  let handler;
  let creations = 0;
  let calls = 0;
  const api = createApi(
    {
      appPath: process.cwd(),

      setHandler(value) {
        handler = value;
      },

      enableSandbox() {
        ++calls;
        if (refuse) {
          throw Object.assign(Error('native refused'), { code: 'ERR_FIREFLY_SANDBOX_DISABLED' });
        }
      },

      create() {
        return ++creations;
      }
    },
    EventEmitter,
    path,
    urls
  );
  return { api, ready: () => handler(0, 'ready', ''), count: () => creations, calls: () => calls };
}

const defaults = fixture();
defaults.ready();

fails(
  () => new defaults.api.BrowserWindow({ webPreferences: { sandbox: false } }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);

check(defaults.count() === 0, 'default rejection reached native window');
const enforced = fixture();
check(
  enforced.api.app.enableSandbox() === undefined && enforced.api.app.enableSandbox() === undefined,
  'early return'
);
enforced.ready();
const prefs = { sandbox: false };
new enforced.api.BrowserWindow({ webPreferences: prefs });
check(
  enforced.calls() === 2 && enforced.count() === 1 && prefs.sandbox === false,
  'global override or caller preference mutation'
);

fails(() => enforced.api.app.enableSandbox(), 'ERR_FIREFLY_SANDBOX_TOO_LATE');

fails(
  () => new enforced.api.BrowserWindow({ webPreferences: { sandbox: 'false' } }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);

fails(
  () => new enforced.api.BrowserWindow({ webPreferences: { nodeIntegration: true } }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);

fails(
  () => new enforced.api.BrowserWindow({ webPreferences: { contextIsolation: false } }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);

const refused = fixture(true);

fails(() => refused.api.app.enableSandbox(), 'ERR_FIREFLY_SANDBOX_DISABLED');

refused.ready();

fails(
  () => new refused.api.BrowserWindow({ webPreferences: { sandbox: false } }),
  'ERR_FIREFLY_UNSUPPORTED_API'
);

console.log(
  'PASS app sandbox policy/failed-enable rollback, global override, unchanged preferences and existing isolation limits'
);
