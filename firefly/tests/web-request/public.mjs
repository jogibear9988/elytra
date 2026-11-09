/** Genuine renderer and main net interception. All windows stay hidden; no native endpoint is faked. */
import { app, BrowserWindow, session, protocol, webFrameMain, net, utilityProcess } from 'firefly';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { once } from 'node:events';
const directory = import.meta.dirname;
app.setPath('userData', path.join(directory, 'config'));
protocol.registerSchemesAsPrivileged([
  {
    scheme: 'app',
    privileges: { standard: true, secure: true, supportFetchAPI: true, corsEnabled: true }
  }
]);

app.on('window-all-closed', () => {});

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const pause = () => new Promise((resolve) => setTimeout(resolve, 10));

const servers = [];
const reports = [];
const hits = new Map();
const observed = [];
let window;
let command = null;
let nonce = 0;
let stage = 'readiness';
let customHits = 0;
const limited = typeof process.versions.mite === 'string';

const count = (url) => hits.get(url) ?? 0;

async function until(predicate, label) {
  stage = label;
  for (let i = 0; i < 1000; i++) {
    if (predicate()) {
      return;
    }
    await pause();
  }
  throw Error('Timed out ' + label + ' reports=' + JSON.stringify(reports));
}

async function listen() {
  const server = http.createServer((request, response) => {
    response.setHeader('Connection', 'close');
    response.setHeader('Cache-Control', 'no-store');
    const origin = 'http://127.0.0.1:' + server.address().port;
    const url = new URL(request.url, origin);
    if (url.pathname === '/commands' || url.pathname === '/report') {
      response.setHeader('Access-Control-Allow-Origin', '*');
      response.setHeader('Content-Type', 'application/json');
      if (url.pathname === '/commands') {
        response.end(JSON.stringify(command));
      } else {
        try {
          reports.push(JSON.parse(url.searchParams.get('value')));
          response.end('{}');
        } catch {
          response.statusCode = 400;
          response.end('{}');
        }
      }
      return;
    }
    if (url.pathname.startsWith('/data/')) {
      const key = origin + url.pathname;
      hits.set(key, count(key) + 1);
      if (url.pathname === '/data/filter-image') {
        response.setHeader('Content-Type', 'image/svg+xml');
        response.end('<svg xmlns="http://www.w3.org/2000/svg" width="1" height="1"/>');
        return;
      }
      response.setHeader('Content-Type', 'text/plain');
      if (!url.pathname.startsWith('/data/cors-')) {
        response.setHeader('Access-Control-Allow-Origin', '*');
      }
      response.end('body:' + url.pathname);
      return;
    }
    if (!['/page.html', '/page.mjs'].includes(url.pathname)) {
      response.statusCode = 404;
      response.end();
      return;
    }
    response.setHeader(
      'Content-Type',
      url.pathname.endsWith('.mjs') ? 'text/javascript' : 'text/html'
    );
    response.end(fs.readFileSync(path.join(directory, url.pathname.slice(1))));
  });

  server.on('upgrade', (request, socket) => {
    const key = 'ws://127.0.0.1:' + server.address().port + request.url;
    hits.set(key, count(key) + 1);
    socket.destroy();
  });

  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  servers.push(server);
  return 'http://127.0.0.1:' + server.address().port;
}

async function run(tag, url, kind = 'fetch') {
  const id = ++nonce;
  command = { tag, url, kind, nonce: id };
  await until(
    () =>
      reports.some((value) => value.kind === 'result' && value.nonce === id && value.tag === tag),
    'renderer ' + kind + ' ' + url
  );
  command = null;
  return reports.find(
    (value) => value.kind === 'result' && value.nonce === id && value.tag === tag
  );
}

function metadata(details, owner, tag) {
  check(
    Number.isInteger(details.id) &&
      details.id > 0 &&
      details.method === 'GET' &&
      details.resourceType === 'xhr',
    'Real request identity/method/resource type'
  );
  check(
    details.webContents === owner.webContents && details.webContentsId === owner.webContents.id,
    'Actual webContents identity'
  );
  const frame = details.frame;
  check(
    frame && !frame.isDestroyed() && frame.top === owner.webContents.mainFrame,
    'Actual native frame ownership'
  );
  check(webFrameMain.fromId(frame.processId, frame.routingId) === frame, 'Canonical frame lookup');
  check(
    tag === 'main'
      ? frame === owner.webContents.mainFrame
      : frame.parent === owner.webContents.mainFrame,
    'Native main/child ancestry'
  );
  const page = reports.find((value) => value.kind === 'ready' && value.tag === tag);
  check(frame.url === page.url, 'Actual requesting frame URL');
  observed.push({
    id: details.id,
    url: details.url,
    frame,
    stage: details.responseHeaders ? 'headers' : 'before'
  });
}

app.whenReady().then(async () => {
  const watchdog = setTimeout(() => {
    console.error('webRequest deadline ' + stage);
    app.exit(1);
  }, 27000);
  let status = 1;
  try {
    const first = await listen();
    const cross = await listen();
    const requests = session.defaultSession.webRequest;
    if (limited) {
      const child = utilityProcess.fork(path.join(directory, 'utility-child.mjs'), [], {
        stdio: 'ignore'
      });
      let booted = false;

      child.on('message', () => {
        booted = true;
      });

      await until(() => booted, 'actual utility request-containment peer');
      let refused = false;
      try {
        requests.onBeforeRequest((_details, callback) => callback({ cancel: false }));
      } catch (failure) {
        refused = /live utility processes/.test(failure.message);
      }
      const exited = once(child, 'exit');
      child.kill();
      await exited;
      check(refused, 'Filters must refuse unmediated live utility networking');
    }
    window = new BrowserWindow({ show: false, width: 180, height: 120 });
    await window.loadURL(first + '/page.html?cross=' + encodeURIComponent(cross));
    await until(
      () =>
        reports.some((value) => value.kind === 'ready' && value.tag === 'main') &&
        reports.some((value) => value.kind === 'ready' && value.tag === 'child'),
      'real main and cross-origin documents'
    );

    requests.onBeforeRequest((details, callback) => {
      if (details.url === first + '/data/cancel') {
        metadata(details, window, 'main');
        callback({ cancel: true });
      } else {
        callback({ cancel: false });
      }
    });

    if (limited) {
      let refused = false;
      try {
        utilityProcess.fork(path.join(directory, 'utility-child.mjs'), [], { stdio: 'ignore' });
      } catch (failure) {
        refused = /session request filters/.test(failure.message);
      }
      check(refused, 'Active filters must prevent new unmediated utility networking');
      console.log('PASS public webRequest native utility transport containment');
      const socket = first.replace('http:', 'ws:') + '/data/socket-blocked';
      check(
        !(await run('main', socket, 'socket')).ok && count(socket) === 0,
        'Unmediated WebSocket handshakes must fail before the real upgrade endpoint'
      );
      console.log('PASS public webRequest unsupported WebSocket transport fails closed');
    }

    check(
      !(await run('main', first + '/data/cancel')).ok && count(first + '/data/cancel') === 0,
      'Canceled renderer request cannot reach real endpoint'
    );
    let old = 0;
    let current = 0;

    requests.onBeforeRequest((details, callback) => {
      if (details.url === first + '/data/replaced') {
        old++;
      }
      callback({ cancel: true });
    });

    requests.onBeforeRequest((details, callback) => {
      if (details.url === first + '/data/replaced') {
        current++;
      }
      callback({ cancel: false });
    });

    check(
      (await run('main', first + '/data/replaced')).ok && old === 0 && current === 1,
      'Only last before listener runs'
    );
    requests.onBeforeRequest(null);
    check(
      (await run('main', first + '/data/cancel')).ok && count(first + '/data/cancel') === 1,
      'Null before listener restores genuine network access'
    );

    requests.onBeforeRequest(
      { urls: [first + '/data/*'], excludeUrls: [first + '/data/excluded*'], types: ['xhr'] },
      (_details, callback) => callback({ cancel: true })
    );

    check(
      !(await run('main', first + '/data/filter-hit')).ok &&
        count(first + '/data/filter-hit') === 0,
      'Include filter cancels before endpoint'
    );
    check(
      (await run('main', first + '/data/excluded')).ok && count(first + '/data/excluded') === 1,
      'Excluded URL bypasses listener'
    );
    check(
      (await run('main', first + '/data/filter-image', 'image')).ok &&
        count(first + '/data/filter-image') === 1,
      'Resource type filter distinguishes actual image from xhr'
    );
    requests.onBeforeRequest(null);
    console.log(
      'PASS public webRequest renderer cancel endpoint-zero filters last-listener and null reset'
    );

    check(
      !(await run('main', cross + '/data/cors-baseline')).ok &&
        count(cross + '/data/cors-baseline') === 1,
      'Independent genuine CORS baseline fails without ACAO'
    );
    let oldHeaders = 0;
    let edited = 0;

    requests.onHeadersReceived((details, callback) => {
      oldHeaders++;
      callback({ cancel: true });
    });

    requests.onHeadersReceived((details, callback) => {
      if (details.url === cross + '/data/cors-edited') {
        edited++;
        check(
          details.statusCode === 200 &&
            Object.values(details.responseHeaders).every((value) => Array.isArray(value)),
          'Actual response metadata arrays'
        );
        callback({
          responseHeaders: {
            ...details.responseHeaders,
            'Access-Control-Allow-Origin': ['*'],
            'Access-Control-Expose-Headers': ['X-Intercepted'],
            'X-Intercepted': ['native-edit']
          }
        });
      } else if (details.url === first + '/data/headers-cancel') {
        callback({ cancel: true });
      } else {
        callback({ cancel: false });
      }
    });

    const cors = await run('main', cross + '/data/cors-edited');
    check(
      cors.ok &&
        cors.text === 'body:/data/cors-edited' &&
        cors.edited === 'native-edit' &&
        edited === 1 &&
        oldHeaders === 0,
      'Actual header rewrite runs before CORS and response exposure'
    );
    check(
      !(await run('main', first + '/data/headers-cancel')).ok &&
        count(first + '/data/headers-cancel') === 1,
      'Headers cancellation occurs after real endpoint before body exposure'
    );
    requests.onHeadersReceived(null);
    check(
      !(await run('main', cross + '/data/cors-reset')).ok &&
        count(cross + '/data/cors-reset') === 1,
      'Reset removes actual ACAO rewrite'
    );
    console.log(
      'PASS public webRequest response header ACAO before CORS last-listener cancellation and reset'
    );

    requests.onBeforeRequest((details, callback) => {
      if (details.url === first + '/data/frame-main') {
        metadata(details, window, 'main');
      }
      if (details.url === first + '/data/frame-child') {
        metadata(details, window, 'child');
      }
      callback({ cancel: false });
    });

    requests.onHeadersReceived((details, callback) => {
      if (details.url === first + '/data/frame-main') {
        metadata(details, window, 'main');
      }
      if (details.url === first + '/data/frame-child') {
        metadata(details, window, 'child');
      }
      callback({ cancel: false });
    });

    check(
      (await run('main', first + '/data/frame-main')).ok &&
        (await run('child', first + '/data/frame-child')).ok,
      'Actual native main/cross-frame transfers'
    );
    for (const path of ['/data/frame-main', '/data/frame-child']) {
      const events = observed.filter((value) => value.url === first + path);
      check(
        events.length === 2 && events[0].id === events[1].id && events[0].frame === events[1].frame,
        'Stable logical request/frame identity across before and headers'
      );
      if (limited) {
        check(events[0].id % 2 === 1, 'Native renderer identity namespace is odd');
      }
    }
    console.log(
      'PASS public webRequest canonical same cross-origin frames and stable request identity across stages'
    );

    const mainRequests = [];

    requests.onBeforeRequest((details, callback) => {
      if (details.url.startsWith(first + '/data/main-')) {
        check(
          details.frame == null &&
            details.webContents === undefined &&
            details.webContentsId === undefined,
          'Main requests must not borrow renderer authority'
        );
        if (limited) {
          check(details.frame === null && details.id % 2 === 0, 'Main request identity namespace');
        }
        mainRequests.push({ id: details.id, url: details.url, stage: 'before' });
      }
      callback({
        cancel:
          details.url === first + '/data/main-fetch-cancel' ||
          details.url === first + '/data/main-request-cancel'
      });
    });

    requests.onHeadersReceived((details, callback) => {
      if (details.url === first + '/data/main-header') {
        mainRequests.push({ id: details.id, url: details.url, stage: 'headers' });
        callback({
          responseHeaders: { ...details.responseHeaders, 'X-Intercepted': ['main-edit'] }
        });
      } else {
        callback({ cancel: false });
      }
    });

    let denied = false;
    try {
      await net.fetch(first + '/data/main-fetch-cancel');
    } catch {
      denied = true;
    }
    check(
      denied && count(first + '/data/main-fetch-cancel') === 0,
      'Main net.fetch cancellation before endpoint'
    );
    const request = net.request(first + '/data/main-request-cancel');
    let response = false;

    request.on('response', () => {
      response = true;
    });

    const failed = new Promise((resolve) => request.once('error', resolve));
    request.end();
    await failed;
    check(
      !response && count(first + '/data/main-request-cancel') === 0,
      'Main net.request cancellation before endpoint'
    );
    const mainResponse = await net.fetch(first + '/data/main-header');
    check(
      mainResponse.headers.get('x-intercepted') === 'main-edit' &&
        (await mainResponse.text()) === 'body:/data/main-header',
      'Main header rewrite before response exposure'
    );
    const mainEvents = mainRequests.filter((value) => value.url === first + '/data/main-header');
    check(
      mainEvents.length === 2 && mainEvents[0].id === mainEvents[1].id,
      'Main logical ID stable across stages'
    );
    requests.onBeforeRequest(null);
    requests.onHeadersReceived(null);
    console.log(
      'PASS public webRequest actual main net fetch request cancellation header rewrite and no renderer authority'
    );

    protocol.registerFileProtocol('app', (request, callback) => {
      const url = new URL(request.url);
      if (url.pathname === '/data/custom-cancel') {
        customHits++;
      }
      const name = url.pathname.endsWith('page.mjs')
        ? 'page.mjs'
        : url.pathname.startsWith('/data/')
          ? 'payload.txt'
          : 'page.html';
      callback({
        path: path.join(directory, name),
        headers: {
          'Content-Type': name.endsWith('.mjs')
            ? 'text/javascript'
            : name.endsWith('.txt')
              ? 'text/plain'
              : 'text/html'
        }
      });
    });

    requests.onBeforeRequest((details, callback) =>
      callback({ cancel: details.url === 'app://fixture/data/custom-cancel' })
    );

    command = null;
    await window.loadURL('app://fixture/page.html?tag=custom&control=' + encodeURIComponent(first));
    await until(
      () => reports.some((value) => value.kind === 'ready' && value.tag === 'custom'),
      'actual custom protocol document'
    );
    check(
      !(await run('custom', 'app://fixture/data/custom-cancel')).ok && customHits === 0,
      'Custom cancellation before privileged native file provider'
    );
    requests.onBeforeRequest(null);
    protocol.unregisterProtocol('app');
    console.log('PASS public webRequest custom protocol cancellation before native file provider');
    status = 0;
  } catch (failure) {
    console.error('webRequest acceptance failure at ' + stage + ': ' + (failure.stack ?? failure));
  } finally {
    clearTimeout(watchdog);
    try {
      session.defaultSession.webRequest.onBeforeRequest(null);
      session.defaultSession.webRequest.onHeadersReceived(null);
    } catch {}
    try {
      window?.destroy();
    } catch {}
    for (const server of servers) {
      server.closeAllConnections();
      server.close();
    }
    app.exit(status);
  }
});
