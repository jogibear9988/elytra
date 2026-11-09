/** One real-renderer batch. Only the explicit offscreen Firefly surface becomes visible.
 * Electron remains hidden; no native desktop surface is shown by this fixture.
 */
import { app, BrowserWindow, session } from 'firefly';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { once } from 'node:events';

const directory = import.meta.dirname;
// Geolocation waits for a visible document. A matched Qt offscreen plugin makes
// this a virtual surface; never show a window on a physical platform backend.
const virtualVisible =
  typeof process.versions.mite === 'string' && process.env.QT_QPA_PLATFORM === 'offscreen';
app.setPath('userData', path.join(directory, 'config'));

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const pause = () => new Promise((resolve) => setTimeout(resolve, 10));

const reports = [];
const servers = [];
let window;
let secondWindow;
let stage = 'readiness';

app.on('window-all-closed', () => {});

async function until(predicate, label) {
  stage = label;
  for (let attempt = 0; attempt < 700; attempt++) {
    const bad = reports.find((report) => report.error);
    if (bad) {
      throw Error('Renderer error: ' + JSON.stringify(bad));
    }
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
    if (request.url === '/report' && request.method === 'POST') {
      let text = '';
      request.setEncoding('utf8');

      request.on('data', (chunk) => {
        text += chunk;
        if (text.length > 32768) {
          request.destroy();
        }
      });

      request.on('end', () => {
        try {
          reports.push(JSON.parse(text));
          response.end('ok');
        } catch {
          response.statusCode = 400;
          response.end('invalid report');
        }
      });

      return;
    }
    const url = new URL(request.url, 'http://127.0.0.1');
    if (!['/page.html', '/page.mjs'].includes(url.pathname)) {
      response.statusCode = 404;
      response.end();
      return;
    }
    response.setHeader(
      'Content-Type',
      url.pathname.endsWith('.mjs') ? 'text/javascript' : 'text/html'
    );
    response.setHeader(
      'Permissions-Policy',
      'geolocation=(self ' +
        servers.map((value) => '"http://127.0.0.1:' + value.address().port + '"').join(' ') +
        ')'
    );
    response.end(fs.readFileSync(path.join(directory, url.pathname.slice(1))));
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  servers.push(server);
  return 'http://127.0.0.1:' + server.address().port;
}

app.whenReady().then(async () => {
  const watchdog = setTimeout(() => {
    console.error('Session permission deadline at ' + stage);
    app.exit(1);
  }, 24000);
  let status = 1;
  try {
    const [first, cross, fresh] = [await listen(), await listen(), await listen()];
    const checks = [];
    const requests = [];
    const permissions = session.defaultSession;

    permissions.setPermissionCheckHandler((contents, permission, origin, details) => {
      checks.push({ contents, permission, origin, details: { ...details } });
      return false;
    });

    permissions.setPermissionRequestHandler((contents, permission, callback, details) => {
      requests.push({ contents, permission, details: { ...details } });
      callback(false);
    });

    window = new BrowserWindow({ show: virtualVisible, width: 180, height: 120 });
    check(
      window.webContents.session === permissions,
      'Owned contents share the actual default session'
    );
    const top = first + '/page.html?phase=query&cross=' + encodeURIComponent(cross);
    await window.loadURL(top);
    await until(
      () => reports.filter((value) => value.phase === 'query').length === 3,
      'real main/same/cross-origin permission queries'
    );
    check(
      reports.filter((value) => value.phase === 'query').every((value) => value.state === 'denied'),
      'All genuine permission queries denied'
    );
    for (const [tag, origin] of [
      ['top', first],
      ['same', first],
      ['cross', cross]
    ]) {
      const report = reports.find((value) => value.tag === tag);
      check(report && new URL(report.url).origin === origin, 'Self-authored frame report ' + tag);
      check(
        checks.some(
          (value) =>
            value.permission === 'geolocation' &&
            value.contents === window.webContents &&
            new URL(value.origin).origin === origin &&
            value.details.requestingUrl === report.url &&
            value.details.isMainFrame === (tag === 'top')
        ),
        'Native check frame/origin binding ' +
          tag +
          ': ' +
          JSON.stringify(checks.map((value) => ({ ...value, contents: value.contents?.id })))
      );
    }
    console.log(
      'PASS public session genuine renderer query denial and top same cross-origin frame binding'
    );
    // A fresh origin avoids treating a previously cached denial as request-handler acceptance.
    const target = fresh + '/page.html?phase=request';
    await window.loadURL(target);
    await until(
      () => reports.some((value) => value.phase === 'request'),
      'real geolocation permission request denial'
    );
    const denied = reports.find((value) => value.phase === 'request');
    check(denied.code === 1, 'Actual geolocation request must reject PERMISSION_DENIED');
    check(
      requests.some(
        (value) =>
          value.permission === 'geolocation' &&
          value.contents === window.webContents &&
          value.details.requestingUrl === target &&
          value.details.isMainFrame === true
      ),
      'Actual request handler must run; cached/OS denial alone is not acceptance'
    );
    console.log(
      'PASS public session actual geolocation request handler denial without device access or OS grant'
    );

    if (typeof process.versions.mite === 'string') {
      async function queryPage(
        owner,
        origin,
        tag,
        permission = 'geolocation',
        expected = 'denied'
      ) {
        const target = origin + '/page.html?phase=query&tag=' + tag + '&permission=' + permission;
        await owner.loadURL(target);
        await until(
          () => reports.some((value) => value.phase === 'query' && value.tag === tag),
          'permission query ' + tag
        );
        const report = reports.find((value) => value.phase === 'query' && value.tag === tag);
        check(
          report.state === expected,
          'Current native document permission ' + tag + ': ' + JSON.stringify(report)
        );
        return report;
      }

      let replacementCalls = 0;

      permissions.setPermissionCheckHandler((contents, permission, origin, details) => {
        check(contents === window.webContents, 'Replacement check actual owner');
        replacementCalls++;
        return false;
      });

      await queryPage(window, first, 'check-replaced');
      check(replacementCalls > 0, 'Replaced handler must be called by actual native query');
      permissions.setPermissionCheckHandler(null);
      const beforeReset = replacementCalls;
      await queryPage(window, first, 'check-reset');
      check(replacementCalls === beforeReset, 'Reset cannot call removed check handler');

      secondWindow = new BrowserWindow({ show: virtualVisible, width: 180, height: 120 });

      permissions.setPermissionCheckHandler(
        (contents, permission) =>
          permission === 'notifications' && contents === secondWindow.webContents
      );
      // Query-only policy grants never request or display a system notification.
      await queryPage(secondWindow, first, 'same-origin-query-grant', 'notifications', 'granted');
      await queryPage(window, first, 'same-origin-query-deny', 'notifications');
      permissions.setPermissionCheckHandler(null);
      console.log(
        'PASS public session native handler replacement reset and same-origin contents policy isolation'
      );

      const held = new Map();

      const holdHandler = (contents, permission, callback, details) => {
        const target = new URL(details.requestingUrl);
        if (permission === 'geolocation' && target.searchParams.get('phase') === 'hold') {
          const tag = target.searchParams.get('tag');
          check(!held.has(tag), 'One actual pending native request ' + tag);
          held.set(tag, { contents, callback, url: details.requestingUrl });
        } else {
          callback(false);
        }
      };

      async function hold(owner, tag) {
        const target = fresh + '/page.html?phase=hold&tag=' + tag;
        await owner.loadURL(target);
        await until(() => held.has(tag), 'actual pending geolocation handler ' + tag);
        const request = held.get(tag);
        check(
          request.contents === owner.webContents && request.url === target,
          'Pending request actual owner/document ' + tag
        );
        return request.callback;
      }

      permissions.setPermissionRequestHandler(holdHandler);
      const afterNavigation = await hold(window, 'stale-navigation');
      await window.loadURL(fresh + '/page.html?phase=idle&tag=navigated');
      await until(
        () => reports.some((value) => value.phase === 'idle' && value.tag === 'navigated'),
        'new document committed before stale callback'
      );
      afterNavigation(true);
      await queryPage(window, fresh, 'after-stale-navigation');

      const afterClose = await hold(secondWindow, 'stale-close');
      let ownerClosed = false;

      secondWindow.once('closed', () => {
        ownerClosed = true;
      });

      secondWindow.destroy();
      await until(
        () => ownerClosed && secondWindow.isDestroyed(),
        'actual native owner closure before stale callback'
      );
      check(ownerClosed && secondWindow.isDestroyed(), 'Native owner closed before stale callback');
      afterClose(true);
      await queryPage(window, fresh, 'after-stale-close');

      const afterReplacement = await hold(window, 'stale-replacement');

      permissions.setPermissionRequestHandler((_contents, _permission, callback) =>
        callback(false)
      );

      afterReplacement(true);
      await until(
        () => reports.some((value) => value.phase === 'hold' && value.tag === 'stale-replacement'),
        'replacement settles actual pending request'
      );
      check(
        reports.find((value) => value.phase === 'hold' && value.tag === 'stale-replacement')
          .code === 1,
        'Replacement denies pending native request'
      );
      await queryPage(window, fresh, 'after-stale-replacement');

      permissions.setPermissionRequestHandler(holdHandler);
      const afterReset = await hold(window, 'stale-reset');
      permissions.setPermissionRequestHandler(null);
      afterReset(true);
      await until(
        () => reports.some((value) => value.phase === 'hold' && value.tag === 'stale-reset'),
        'reset settles actual pending request'
      );
      check(
        reports.find((value) => value.phase === 'hold' && value.tag === 'stale-reset').code === 1,
        'Reset denies pending native request'
      );
      await queryPage(window, fresh, 'after-stale-reset');
      console.log(
        'PASS public session actual pending requests revoked by navigation owner-close handler replacement and reset'
      );
    }
    status = 0;
  } catch (failure) {
    console.error('Session permission failure at ' + stage + ': ' + (failure.stack ?? failure));
  } finally {
    clearTimeout(watchdog);
    try {
      session.defaultSession.setPermissionRequestHandler(null);
      session.defaultSession.setPermissionCheckHandler(null);
    } catch {}
    try {
      window?.destroy();
      if (secondWindow && !secondWindow.isDestroyed()) {
        secondWindow.destroy();
      }
    } catch {}
    for (const server of servers) {
      server.closeAllConnections();
      server.close();
    }
    app.exit(status);
  }
});
