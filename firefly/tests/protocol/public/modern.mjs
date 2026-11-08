/** Public modern handlers through the real main VM, WebContent and native byte channel. */
import { app, BrowserWindow, protocol, session, net } from 'firefly';
import fs from 'node:fs';
import path from 'node:path';
import { pathToFileURL } from 'node:url';

const root = process.argv[2];

const delay = (ms) => new Promise((resolve) => setTimeout(resolve, ms));

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

async function until(test, message) {
  for (let attempt = 0; attempt < 500; attempt++) {
    if (test()) {
      return;
    }
    await delay(10);
  }
  throw Error(message);
}

async function denied(promise, code) {
  try {
    await promise;
  } catch (error) {
    check(error.code === code, `Expected ${code}, got ${error.stack ?? error}`);
    return;
  }
  throw Error(`Expected rejection ${code}`);
}

try {
  app.disableHardwareAcceleration();
  protocol.registerSchemesAsPrivileged([
    { scheme: 'modern', privileges: { standard: true, secure: true, supportFetchAPI: true } },
    { scheme: 'app', privileges: { standard: true, secure: true, supportFetchAPI: true } }
  ]);
  await app.whenReady();
  check(session.defaultSession.protocol === protocol, 'Shared default session');
  let fileCalls = 0;

  const blockFile = (request) => {
    check(request instanceof Request && request.method === 'GET', 'Genuine file Request');
    fileCalls++;
    return Response.error();
  };

  check(protocol.handle('file', blockFile) === undefined, 'handle returns undefined');
  const window = new BrowserWindow({ show: false });
  await denied(window.loadFile(path.join(root, 'native.html')), 'ERR_FAILED');
  check(fileCalls === 1, `File navigation did not invoke actual handler: ${fileCalls}`);
  const nativeURL = pathToFileURL(path.join(root, 'native.html')).href;
  let pendingSignal;
  let bodyCancelled = 0;
  let pulls = 0;
  let heads = 0;
  const length = 4194323;

  function modern(request) {
    check(request instanceof Request, 'Genuine modern Request');
    const url = new URL(request.url);
    if (url.pathname === '/index.html' || url.pathname === '/pending.html') {
      return new Response(
        fs.readFileSync(
          path.join(root, url.pathname === '/pending.html' ? 'pending.html' : 'modern.html')
        ),
        {
          headers: { 'Content-Type': 'text/html' }
        }
      );
    }
    if (url.pathname === '/pending') {
      pendingSignal = request.signal;
      return new Response(
        new ReadableStream(
          {
            pull() {
              return new Promise(() => {});
            },

            cancel() {
              bodyCancelled++;
            }
          },
          { highWaterMark: 0 }
        )
      );
    }
    if (url.pathname === '/redirect') {
      return new Response(null, { status: 302, headers: { Location: '/binary' } });
    }
    if (url.pathname === '/binary') {
      if (request.method === 'HEAD') {
        heads++;
        return new Response(null, {
          status: 404,
          headers: { 'Content-Length': String(length), 'Content-Type': 'application/octet-stream' }
        });
      }
      let offset = 0;
      return Promise.resolve(
        new Response(
          new ReadableStream(
            {
              pull(controller) {
                pulls++;
                const size = Math.min(32768, length - offset);
                if (!size) {
                  controller.close();
                  return;
                }
                const bytes = new Uint8Array(size);
                for (let i = 0; i < size; i++) {
                  bytes[i] = ((offset + i) * 37 + 11) & 255;
                }
                offset += size;
                controller.enqueue(bytes);
              }
            },
            { highWaterMark: 0 }
          ),
          {
            status: 404,
            headers: { 'Content-Type': 'application/octet-stream', 'Set-Cookie': 'private=1' }
          }
        )
      );
    }
    return new Response('unavailable', { status: 500 });
  }

  protocol.handle('modern', modern);
  check(!protocol.registerFileProtocol('modern', () => {}), 'Legacy collision replaced handler');
  check(protocol.isProtocolHandled('modern'), 'Modern handler missing');
  const mainResponse = await net.fetch('modern://bundle/redirect');
  check(mainResponse.status === 404 && mainResponse.redirected, 'Main protocol redirect/status');
  check((await mainResponse.arrayBuffer()).byteLength === length, 'Main binary response');
  await denied(
    net.fetch('modern://bundle/binary', { bypassCustomProtocolHandlers: true }),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );
  await denied(net.fetch(nativeURL), 'ERR_FIREFLY_PROTOCOL_NETWORK');
  check(fileCalls === 2, 'Main net.fetch did not honor file interception');
  console.log(
    'PASS public modern main net.fetch: binary/status/redirect/bypass and genuine file denial'
  );

  await window.loadURL('modern://bundle/index.html');
  await until(
    () => window.webContents.getTitle() === 'MODERN_OK',
    `Modern document failed: ${window.webContents.getTitle()}`
  );
  check(heads === 1 && pulls > 128, 'Renderer HEAD/body production');
  console.log('PASS public modern renderer: real HTML/binary404/HEAD500/header filtering');
  await window.loadURL('modern://bundle/pending.html');
  await until(() => pendingSignal, 'Pending stream was never admitted');
  check(!pendingSignal.aborted, 'Request signal already aborted');
  protocol.unhandle('modern');
  await until(
    () => pendingSignal.aborted && bodyCancelled === 1,
    'Retired native stream did not abort/cancel exactly once'
  );
  await until(
    () => window.webContents.getTitle() === 'RETIRED',
    'Renderer stream remained pending'
  );
  protocol.handle('modern', modern);
  await window.loadURL('modern://bundle/index.html');
  await until(() => window.webContents.getTitle() === 'MODERN_OK', 'New generation failed');
  console.log(
    'PASS public modern native cancellation: pending reader/request signal and fresh generation'
  );

  check(
    protocol.registerFileProtocol('app', (_request, callback) =>
      callback(path.join(root, 'legacy.html'))
    ),
    'Legacy registration'
  );
  await window.loadURL('app://bundle/index.html');
  await until(() => window.webContents.getTitle() === 'LEGACY_OK', 'Legacy file callback broken');
  protocol.unhandle('file');
  await window.loadFile(path.join(root, 'native.html'));
  await until(
    () => window.webContents.getTitle() === 'NATIVE_OK',
    'Unhandle did not restore ordinary native file navigation'
  );
  protocol.handle('file', blockFile);

  await until(
    () => window.webContents.getTitle() === 'FILE_DENIED',
    'Late file interception failed'
  );
  check(fileCalls >= 3, 'Late file request did not execute main handler');
  console.log(
    'PASS public file policy: legacy route, native restoration, late authenticated document interception'
  );
  protocol.unhandle('file');

  let fileBodies = 0;

  protocol.handle('file', (request) => {
    if (request.url.endsWith('/payload.bin')) {
      fileBodies++;
      return new Response(new Uint8Array([0, 255, 17]), {
        status: 201,
        headers: { 'Content-Type': 'application/octet-stream' }
      });
    }
    const blocked = request.url.endsWith('/blocked.html');
    return new Response(
      `<!doctype html><title>FILE_PENDING</title><script>
      fetch('./payload.bin').then(async response => {
        const bytes = new Uint8Array(await response.arrayBuffer());
        document.title = response.status === 201 && bytes.length === 3 && bytes[1] === 255 ? 'INTERCEPTED_FILE' : 'BAD';
      }, () => document.title = ${JSON.stringify(blocked ? 'CSP_BLOCKED' : 'BAD')});
    </script>`,
      {
        headers: {
          'Content-Type': 'text/html',
          'Content-Security-Policy':
            "default-src 'none'; script-src 'unsafe-inline'; connect-src " +
            (blocked ? "'none'" : 'file:')
        }
      }
    );
  });

  await window.loadFile(path.join(root, 'native.html'));
  await until(
    () => window.webContents.getTitle() === 'INTERCEPTED_FILE',
    'File handler response was not used for navigation'
  );
  check(fileBodies === 1, 'File-owned fetch did not consume genuine binary handler response');
  await window.loadFile(path.join(root, 'blocked.html'));
  await until(
    () => window.webContents.getTitle() === 'CSP_BLOCKED',
    'File interception bypassed CSP'
  );
  check(fileBodies === 1, 'CSP-denied resource reached main handler');
  console.log(
    'PASS public file response: genuine HTML/binary handler replies and CSP before native filesystem access'
  );
  pendingSignal = null;
  await window.loadURL('modern://bundle/pending.html');
  await until(() => pendingSignal, 'Closing-window pending request not admitted');
  const cancellationsBeforeClose = bodyCancelled;
  window.destroy();
  await until(
    () => pendingSignal.aborted && bodyCancelled === cancellationsBeforeClose + 1,
    'Closed native owner retained handler/body callbacks'
  );
  console.log(
    'PASS public modern closed owner: request signal and pending body cancelled exactly once'
  );
  protocol.unhandle('file');
  protocol.unhandle('modern');
  protocol.unregisterProtocol('app');
  app.exit(0);
} catch (error) {
  console.error(error.stack ?? error);
  app.exit(1);
}
