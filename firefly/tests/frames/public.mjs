/** One hidden genuine frame-tree batch, including native identity before first preload IPC. */
import { app, BrowserWindow, webFrameMain, ipcMain } from 'firefly';
import http from 'node:http';
import fs from 'node:fs';
import path from 'node:path';
import { once } from 'node:events';
const directory = import.meta.dirname;
app.setPath('userData', path.join(directory, 'config'));

app.on('window-all-closed', () => {});

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

const pause = () => new Promise((resolve) => setTimeout(resolve, 10));

const servers = [];
const reports = [];
const commands = { navigate: false, hash: false, ipc: false };
let window;
let stage = 'readiness';
let ipcCalls = 0;

async function until(predicate, label) {
  stage = label;
  for (let i = 0; i < 700; i++) {
    const error = reports.find((value) => value.error);
    if (error) {
      throw Error(JSON.stringify(error));
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
    if (request.url === '/commands') {
      response.setHeader('Content-Type', 'application/json');
      response.end(JSON.stringify(commands));
      return;
    }
    if (request.url === '/report') {
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
          response.end();
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
    response.end(fs.readFileSync(path.join(directory, url.pathname.slice(1))));
  });
  server.listen(0, '127.0.0.1');
  await once(server, 'listening');
  servers.push(server);
  return 'http://127.0.0.1:' + server.address().port;
}

app.whenReady().then(async () => {
  const watchdog = setTimeout(() => {
    console.error('Frame acceptance deadline at ' + stage);
    app.exit(1);
  }, 24000);
  let status = 1;
  try {
    const first = await listen();
    const cross = await listen();
    window = new BrowserWindow({
      show: false,
      width: 180,
      height: 120,
      webPreferences: { preload: path.join(directory, 'preload.cjs') }
    });

    ipcMain.handle('frames:identity', (event) => {
      ipcCalls++;
      check(
        event.sender === window.webContents && event.senderFrame === window.webContents.mainFrame,
        'Actual IPC senderFrame shares canonical main wrapper'
      );
      return true;
    });

    const target = first + '/page.html?cross=' + encodeURIComponent(cross);
    await window.loadURL(target);
    await until(
      () =>
        ['top', 'same', 'cross'].every((tag) =>
          reports.some((value) => value.kind === 'ready' && value.tag === tag)
        ),
      'actual same and cross origin documents'
    );
    const main = window.webContents.mainFrame;
    check(
      main && !main.isDestroyed() && ipcCalls === 0,
      'Native mainFrame exists before any preload IPC'
    );
    check(
      main.url === target && main.origin === first && main.parent === null && main.top === main,
      'Native main URL origin and canonical root links'
    );
    check(
      Number.isInteger(main.processId) &&
        main.processId > 0 &&
        Number.isInteger(main.routingId) &&
        main.routingId >= 0,
      'Actual internal process/routing identities'
    );
    check(
      main.osProcessId === window.webContents.getOSProcessId(),
      'Actual OS pid separately describes real renderer process'
    );
    check(
      webFrameMain.fromId(main.processId, main.routingId) === main,
      'Public native identity lookup'
    );
    await until(() => main.frames.length === 2, 'native canonical children');
    const children = main.frames;
    const same = children.find((value) => new URL(value.url).searchParams.get('tag') === 'same');
    const other = children.find((value) => new URL(value.url).searchParams.get('tag') === 'cross');
    check(
      same && other && same !== other,
      'Distinct canonical documents for both native child frames'
    );
    for (const child of [same, other]) {
      check(
        child.parent === main && child.top === main && !child.isDestroyed(),
        'Canonical child ancestry'
      );
      check(
        webFrameMain.fromId(child.processId, child.routingId) === child,
        'Child process/routing lookup identity'
      );
    }
    check(same.origin === first && other.origin === cross, 'Actual cross-origin metadata');
    console.log(
      'PASS public canonical frames before IPC genuine native tree origins process identities and lookup'
    );
    commands.hash = true;
    await until(
      () => reports.some((value) => value.kind === 'hash'),
      'actual same-document hash change'
    );
    await until(() => main.url.endsWith('#retained'), 'native same-document URL update');
    check(
      window.webContents.mainFrame === main && !main.isDestroyed(),
      'Same-document navigation retains frame wrapper'
    );
    const treeId = same.frameTreeNodeId;
    commands.navigate = true;
    await until(
      () =>
        reports.some(
          (value) => value.tag === 'same' && value.revision === '2' && value.kind === 'ready'
        ),
      'actual child cross-document navigation'
    );
    await until(
      () => main.frames.some((value) => value.url.includes('revision=2')),
      'updated native child tree'
    );
    const replacement = main.frames.find((value) => value.url.includes('revision=2'));
    check(
      replacement.parent === main &&
        replacement.top === main &&
        replacement.frameTreeNodeId === treeId,
      'Child replacement keeps actual frame-tree node ancestry'
    );
    if (typeof process.versions.mite === 'string') {
      check(
        replacement !== same && same.isDestroyed(),
        'Firefly active-document identity retires old wrapper'
      );
      let stale;
      try {
        same.url;
      } catch (failure) {
        stale = failure;
      }
      check(
        stale?.code === 'ERR_FIREFLY_OBJECT_DESTROYED',
        'Retired document property access refuses stale data'
      );
    }
    console.log(
      'PASS public canonical frames same-document identity and genuine child document replacement'
    );
    commands.ipc = true;
    await until(() => reports.some((value) => value.kind === 'ipc'), 'actual preload IPC identity');
    check(ipcCalls === 1, 'One real senderFrame/mainFrame identity assertion');
    let closed = false;

    window.once('closed', () => {
      closed = true;
    });

    window.destroy();
    await until(() => closed && window.isDestroyed(), 'actual native window closure');
    // Electron's RFHs may retire after the BrowserWindow closed event.
    await until(
      () => main.isDestroyed() && replacement.isDestroyed() && other.isDestroyed(),
      'actual native frame retirement after owner closure'
    );
    check(
      main.isDestroyed() && replacement.isDestroyed() && other.isDestroyed(),
      'Window closure invalidates native frame wrappers'
    );
    console.log(
      'PASS public canonical IPC senderFrame identity and actual native owner-close retirement'
    );
    status = 0;
  } catch (failure) {
    console.error('Frame acceptance failure at ' + stage + ': ' + (failure.stack ?? failure));
  } finally {
    clearTimeout(watchdog);
    try {
      if (window && !window.isDestroyed()) {
        window.destroy();
      }
    } catch {}
    for (const server of servers) {
      server.closeAllConnections();
      server.close();
    }
    app.exit(status);
  }
});
