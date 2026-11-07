/** Real main handlers and hidden native window; no test-only transport or path provider. */
import { app, BrowserWindow, protocol, session } from 'firefly';
import path from 'node:path';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function fails(action, code) {
  try {
    action();
  } catch (error) {
    check(error.code === code, `Expected ${code}, got ${error}`);
    return;
  }
  throw Error(`Expected ${code}`);
}

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

async function until(test, message) {
  for (let count = 0; count < 500; ++count) {
    if (test()) {
      return;
    }
    await delay(20);
  }
  throw Error(message);
}

async function rejected(promise, message) {
  try {
    await promise;
  } catch (error) {
    check(error.code === 'ERR_FAILED', `${message}: ${error}`);
    return;
  }
  throw Error(message);
}

try {
  fails(() => session.defaultSession, 'ERR_FIREFLY_NOT_READY');

  protocol.registerSchemesAsPrivileged([
    { scheme: 'app', privileges: { standard: true, secure: true, supportFetchAPI: true } },
    { scheme: 'no-fetch', privileges: { standard: true, secure: true } }
  ]);

  fails(() => protocol.registerFileProtocol('app', () => {}), 'ERR_FIREFLY_NOT_READY');

  await app.whenReady();
  check(session.defaultSession.protocol === protocol, 'default-session singleton');

  fails(() => protocol.registerFileProtocol('no-fetch', () => {}), 'ERR_FIREFLY_UNSUPPORTED_API');

  check(!protocol.isProtocolHandled('app'), 'unexpected initial route');
  const root = process.argv[2];
  const calls = [];
  let late;
  const routes = new Map([
    ['app://bundle/index.html', 'index.html'],
    ['app://bundle/main.mjs', 'main.mjs'],
    ['app://bundle/math.mjs', 'math.mjs'],
    ['app://bundle/theme.css', 'theme.css'],
    ['app://bundle/payload.bin', 'payload.bin'],
    ['app://bundle/pending.html', 'pending.html'],
    ['app://bundle/pending.mjs', 'pending.mjs']
  ]);

  function handler(request, callback) {
    calls.push({ url: request.url, method: request.method });
    check(
      Object.isFrozen(request) && request.headers === undefined,
      'bounded truthful request fields'
    );
    if (request.url === 'app://bundle/late.html' || request.url === 'app://bundle/late.bin') {
      late = callback;
      return;
    }
    if (request.url === 'app://bundle/bad-header.html') {
      callback({ path: path.join(root, 'index.html'), headers: { 'Content-Length': '99' } });
      return;
    }
    if (request.url === 'app://bundle/missing.html') {
      callback(path.join(root, 'missing.html'));
      return;
    }
    const file = routes.get(request.url);

    setTimeout(() => {
      if (!file) {
        callback({ error: -10 });
        return;
      }
      const result = callback({
        path: path.join(root, file),
        headers: {
          'Cache-Control': 'no-store',
          'Set-Cookie': 'private=1',
          'Content-Security-Policy':
            "default-src 'none'; script-src 'self'; style-src 'self'; connect-src 'self'"
        }
      });
      check(result === undefined, 'callback must not expose native result status');
      check(callback(path.join(root, 'payload.bin')) === undefined, 'duplicate callback admitted');
    }, 10);
  }

  check(protocol.registerFileProtocol('app', handler), 'file registration');
  check(
    !protocol.registerFileProtocol('app', () => {
      throw Error('replacement handler');
    }),
    'duplicate registration replaced route'
  );
  check(protocol.isProtocolHandled('app'), 'registered route missing');
  const window = new BrowserWindow({ show: false });
  await window.loadURL('app://bundle/index.html');
  await until(
    () => window.webContents.getTitle() === 'PUBLIC_PROTOCOL_OK',
    'module/CSS/binary document failed'
  );
  check(
    calls.length === 7 && calls.filter((r) => r.method === 'HEAD').length === 1,
    `Unexpected main file requests: ${JSON.stringify(calls)}`
  );
  check(window.webContents.getURL() === 'app://bundle/index.html', 'actual app URL');
  console.log(
    'PASS public default-session file handler: real HTML/ESM/CSS/binary/HEAD/refusal and duplicate callbacks'
  );

  await rejected(
    window.loadURL('app://bundle/denied.html'),
    'explicit denial did not reject loadURL'
  );
  await rejected(
    window.loadURL('app://bundle/missing.html'),
    'native open failure did not reject loadURL'
  );
  await rejected(
    window.loadURL('app://bundle/bad-header.html'),
    'invalid metadata did not reject loadURL'
  );
  const waiting = window.loadURL('app://bundle/late.html');
  // Attach rejection before revocation can settle it.
  const waitingResult = rejected(waiting, 'unregistration did not reject pending navigation');
  await until(() => late, 'late navigation handler not reached');
  check(protocol.unregisterProtocol('app'), 'unregister failed');
  await waitingResult;
  check(
    late(path.join(root, 'index.html')) === undefined,
    'cancelled navigation callback revived a route'
  );
  check(
    !protocol.unregisterProtocol('app') && !protocol.isProtocolHandled('app'),
    'removed route retained'
  );
  check(protocol.registerFileProtocol('app', handler), 're-registration failed');
  check(late(path.join(root, 'index.html')) === undefined, 'old generation affected replacement');
  late = null;
  await window.loadURL('app://bundle/pending.html');
  await until(() => late, 'document fetch handler not reached');
  check(protocol.unregisterProtocol('app'), 'document route removal');
  await until(
    () => window.webContents.getTitle() === 'PUBLIC_PROTOCOL_CANCELLED',
    'pending renderer fetch did not reject'
  );
  check(
    late(path.join(root, 'payload.bin')) === undefined,
    'cancelled document callback reopened a file'
  );
  check(protocol.registerFileProtocol('app', handler), 'final route registration');
  await window.loadURL('app://bundle/index.html');
  await until(
    () => window.webContents.getTitle() === 'PUBLIC_PROTOCOL_OK',
    'replacement route no longer loads'
  );
  window.destroy();
  protocol.unregisterProtocol('app');
  console.log(
    'PASS public file handler: load failures, pending navigation/document revocation and generation replacement'
  );
  app.exit(0);
} catch (error) {
  console.error(error.stack ?? error);
  app.exit(1);
}
