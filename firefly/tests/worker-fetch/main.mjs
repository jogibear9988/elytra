/** Same-origin custom Worker modules with real HTTP/session policy and stale-owner controls.
 * Cross-origin worker protocol requests remain explicitly outside this slice. */
import { app, BrowserWindow, protocol, session } from 'firefly';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { once } from 'node:events';
const directory = import.meta.dirname;
app.setPath('userData', path.join(directory, 'config'));

app.on('window-all-closed', () => {});

protocol.registerSchemesAsPrivileged([
  {
    scheme: 'workerfixture',
    privileges: {
      standard: true,
      secure: true,
      supportFetchAPI: true,
      corsEnabled: true
    }
  }
]);
const reports = [];
const hits = new Map();
const holds = new Map();
let command = null;
let nonce = 0;
let stage = 'startup';
let window;
let server;

const check = (value, label) => {
  if (!value) {
    throw Error(label);
  }
};

const count = (key) => hits.get(key) ?? 0;

const seen = (tag, phase) => reports.filter((value) => value.tag === tag && value.phase === phase);

async function until(predicate, label) {
  stage = label;
  for (let attempt = 0; attempt < 1000; ++attempt) {
    check(
      !reports.some((value) => value.phase.includes('worker-error')),
      'worker failed during ' + label
    );
    if (predicate()) {
      return;
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('Worker fetch deadline: ' + label);
}

function issue(tag, operation, extra = {}) {
  command = { tag, operation, nonce: ++nonce, ...extra };
}

function release(target) {
  const callback = holds.get(target);
  check(callback, 'held request missing');
  holds.delete(target);
  callback({ cancel: false });
}

async function start(tag) {
  const previous = seen(tag, 'worker-ready').length;
  issue(tag, 'start');
  await until(
    () => seen(tag, 'worker-ready').length === previous + 1,
    'real Blob module Worker ready'
  );
}

async function terminate(tag) {
  const previous = seen(tag, 'terminated').length;
  issue(tag, 'terminate');
  await until(() => seen(tag, 'terminated').length === previous + 1, 'actual worker terminated');
}

async function barrier(tag, target) {
  issue(tag, 'barrier', { target });
  await until(
    () => seen(tag, 'barrier').some((value) => value.target === target),
    'fresh worker HTTP barrier'
  );
}

try {
  await app.whenReady();
  const watchdog = setTimeout(() => {
    console.error('worker fetch deadline ' + stage);
    app.exit(1);
  }, 40000);
  server = http.createServer((request, response) => {
    const url = new URL(request.url, 'http://127.0.0.1');
    response.setHeader('Connection', 'close');
    response.setHeader('Cache-Control', 'no-store');
    response.setHeader('Access-Control-Allow-Origin', '*');
    if (url.pathname === '/commands' || url.pathname === '/report') {
      response.setHeader('Content-Type', 'application/json');
      if (url.pathname === '/report') {
        reports.push(JSON.parse(url.searchParams.get('value')));
        response.end('{}');
      } else {
        response.end(JSON.stringify(command));
      }
    } else {
      hits.set(url.pathname, count(url.pathname) + 1);
      response.end('actual:' + url.pathname);
    }
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  const origin = 'http://127.0.0.1:' + server.address().port;
  let moduleCalls = 0;
  let deniedModuleCalls = 0;

  protocol.handle('workerfixture', (request) => {
    const url = new URL(request.url);
    check(request.method === 'GET', 'custom module genuine GET');
    if (url.pathname === '/page.html' || url.pathname === '/page.mjs') {
      return new Response(fs.readFileSync(path.join(directory, url.pathname.slice(1))), {
        headers: { 'Content-Type': url.pathname.endsWith('.mjs') ? 'text/javascript' : 'text/html' }
      });
    }
    if (url.pathname !== '/module.mjs' && url.pathname !== '/denied.mjs') {
      return new Response('unavailable', { status: 404 });
    }
    if (url.pathname === '/denied.mjs') {
      ++deniedModuleCalls;
    } else {
      ++moduleCalls;
    }
    return new Response("export const fixtureToken = 'actual-custom-worker-module';", {
      headers: { 'Content-Type': 'text/javascript', 'Access-Control-Allow-Origin': '*' }
    });
  });

  const policy = session.defaultSession.webRequest;
  const beforeIds = new Map();
  let headerEdits = 0;
  let moduleHeaderChecks = 0;

  policy.onBeforeRequest((details, callback) => {
    const url = new URL(details.url);
    if (url.pathname.startsWith('/worker-') || url.protocol === 'workerfixture:') {
      check(
        details.webContents === window.webContents &&
          details.webContentsId === window.webContents.id,
        'worker policy must retain actual owning webContents/session'
      );
      beforeIds.set(details.url, details.id);
      check(
        Number.isInteger(details.id) && details.id > 0 && details.method === 'GET',
        'worker policy genuine request metadata'
      );
    }
    if (url.pathname === '/worker-denied' || url.pathname === '/denied.mjs') {
      callback({ cancel: true });
    } else if (
      url.pathname === '/worker-terminate' ||
      url.pathname === '/worker-navigation' ||
      url.pathname === '/worker-nested-terminate'
    ) {
      check(!holds.has(url.pathname), 'held request duplicated');
      holds.set(url.pathname, callback);
    } else {
      callback({ cancel: false });
    }
  });

  policy.onHeadersReceived((details, callback) => {
    const url = new URL(details.url);
    const edit = url.pathname === '/worker-allowed' || url.pathname === '/worker-nested-allowed';
    if (edit || url.pathname === '/module.mjs') {
      check(
        details.webContents === window.webContents &&
          details.webContentsId === window.webContents.id &&
          details.id === beforeIds.get(details.url) &&
          details.statusCode === 200,
        'worker header stage retains owner and logical request identity'
      );
      if (edit) {
        ++headerEdits;

        queueMicrotask(() =>
          callback({
            cancel: false,
            responseHeaders: {
              ...details.responseHeaders,
              'Access-Control-Expose-Headers': ['X-Worker-Edited'],
              'X-Worker-Edited': ['actual-worker-header-edit']
            }
          })
        );

        return;
      }
      ++moduleHeaderChecks;
    }
    callback({ cancel: false });
  });

  window = new BrowserWindow({ show: false, width: 180, height: 120 });
  await window.loadURL(
    'workerfixture://bundle/page.html?tag=original&control=' + encodeURIComponent(origin)
  );
  await until(() => seen('original', 'document-ready').length === 1, 'original document ready');
  await start('original');
  issue('original', 'basic', {
    module: 'workerfixture://bundle/module.mjs',
    allowed: origin + '/worker-allowed',
    denied: origin + '/worker-denied',
    deniedModule: 'workerfixture://bundle/denied.mjs'
  });
  await until(() => seen('original', 'basic').length === 1, 'worker import and policy requests');
  const basic = seen('original', 'basic')[0];
  check(
    basic.token === 'actual-custom-worker-module' &&
      basic.text === 'actual:/worker-allowed' &&
      basic.edited === 'actual-worker-header-edit' &&
      basic.deniedHTTP &&
      basic.deniedModule &&
      basic.bytes.join() === '0,128,255',
    'real worker module/fetch/postMessage result'
  );
  check(
    moduleCalls === 1 &&
      deniedModuleCalls === 0 &&
      count('/worker-allowed') === 1 &&
      count('/worker-denied') === 0,
    'denied worker requests reached actual transport'
  );
  console.log('PASS dedicated Worker custom module and HTTP policy cancellation');
  check(
    headerEdits === 1 && moduleHeaderChecks === 1,
    'actual worker HTTP and custom-module headers'
  );
  console.log('PASS dedicated Worker actual header decision and mutation');

  issue('original', 'nested-start');
  await until(
    () => seen('original', 'nested-worker-ready').length === 1,
    'real nested Blob module Worker ready'
  );
  issue('original', 'nested-message', {
    message: {
      operation: 'basic',
      module: 'workerfixture://bundle/module.mjs?kind=nested',
      allowed: origin + '/worker-nested-allowed',
      denied: origin + '/worker-denied',
      deniedModule: 'workerfixture://bundle/denied.mjs?kind=nested'
    }
  });
  await until(() => seen('original', 'nested-basic').length === 1, 'nested custom module and HTTP');
  const nested = seen('original', 'nested-basic')[0];
  check(
    nested.token === 'actual-custom-worker-module' &&
      nested.text === 'actual:/worker-nested-allowed' &&
      nested.edited === 'actual-worker-header-edit' &&
      nested.deniedHTTP &&
      nested.deniedModule &&
      nested.bytes.join() === '0,128,255',
    'nested native module/fetch/clone result'
  );
  check(
    moduleCalls === 2 &&
      deniedModuleCalls === 0 &&
      count('/worker-nested-allowed') === 1 &&
      count('/worker-denied') === 0 &&
      headerEdits === 2 &&
      moduleHeaderChecks === 2,
    'nested policy transport/header counts'
  );
  console.log('PASS nested dedicated Worker custom module HTTP and headers');
  issue('original', 'nested-message', {
    message: { operation: 'hold', target: origin + '/worker-nested-terminate' }
  });
  await until(
    () => holds.has('/worker-nested-terminate'),
    'held nested fetch before parent termination'
  );
  await terminate('original');
  release('/worker-nested-terminate');
  await start('original');
  await barrier('original', origin + '/worker-after-nested-terminate');
  check(
    count('/worker-nested-terminate') === 0 &&
      count('/worker-after-nested-terminate') === 1 &&
      seen('original', 'nested-stale-result').length === 0,
    'parent termination did not revoke nested fetch'
  );
  console.log('PASS parent Worker termination revokes nested pending fetch');

  issue('original', 'hold', { target: origin + '/worker-terminate' });
  await until(() => holds.has('/worker-terminate'), 'held worker request before terminate');
  await terminate('original');
  release('/worker-terminate');
  await start('original');
  await barrier('original', origin + '/worker-after-terminate');
  check(
    count('/worker-terminate') === 0 &&
      count('/worker-after-terminate') === 1 &&
      seen('original', 'stale-result').length === 0,
    'terminated worker stale policy completion escaped'
  );
  console.log('PASS dedicated Worker termination revokes pending fetch');

  issue('original', 'hold', { target: origin + '/worker-navigation' });
  await until(() => holds.has('/worker-navigation'), 'held worker request before navigation');
  command = null;
  await window.loadURL(
    'workerfixture://bundle/page.html?tag=replacement&control=' + encodeURIComponent(origin)
  );
  await until(
    () => seen('replacement', 'document-ready').length === 1,
    'replacement document ready'
  );
  release('/worker-navigation');
  await start('replacement');
  await barrier('replacement', origin + '/worker-after-navigation');
  check(
    count('/worker-navigation') === 0 &&
      count('/worker-after-navigation') === 1 &&
      seen('original', 'stale-result').length === 0,
    'old document worker survived navigation'
  );
  console.log('PASS dedicated Worker owner navigation revokes pending fetch');
  policy.onBeforeRequest(null);
  policy.onHeadersReceived(null);
  protocol.unhandle('workerfixture');
  clearTimeout(watchdog);
  window.destroy();
  server.close();
  app.exit(0);
} catch (error) {
  console.error('worker fetch fixture failed at ' + stage, error.stack ?? error);
  for (const callback of holds.values()) {
    try {
      callback({ cancel: true });
    } catch {}
  }
  holds.clear();
  if (window && !window.isDestroyed()) {
    window.destroy();
  }
  if (server) {
    server.close();
  }
  app.exit(1);
}
