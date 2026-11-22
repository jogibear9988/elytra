/** Serve the pinned Designer inside a native Firefly window. */
import { createAssetReader, startServer } from './server.mjs';
import { fileURLToPath } from 'node:url';

const web = fileURLToPath(new URL('./web/', import.meta.url));
if (process.argv.includes('--serve-only')) {
  const server = await startServer(web);
  console.log(`http://127.0.0.1:${server.address().port}/`);
} else {
  const { app, BrowserWindow, ipcMain, protocol } = await import('firefly');
  const smoke = process.argv.includes('--smoke');
  const scheme = 'designer';
  const entry = 'designer://bundle/index.html';
  protocol.registerSchemesAsPrivileged([
    { scheme, privileges: { standard: true, secure: true, supportFetchAPI: true } }
  ]);
  await app.whenReady();
  const readAsset = await createAssetReader(web);

  protocol.handle(scheme, async (request) => {
    const url = new URL(request.url);
    if (url.hostname !== 'bundle' || url.username || url.password || url.port) {
      return new Response(null, { status: 403 });
    }
    const { status, headers, body } = await readAsset(url.pathname, request.method, smoke);
    let offset = 0;
    const stream = body?.length
      ? new ReadableStream({
          pull(controller) {
            const chunk = body.subarray(offset, offset + 65536);
            controller.enqueue(chunk);
            offset += chunk.length;
            if (offset === body.length) {
              controller.close();
            }
          }
        })
      : null;
    return new Response(stream, { status, headers });
  });

  const window = new BrowserWindow({
    frame: false,
    width: 1280,
    height: 850,
    title: 'Web Component Designer · Firefly',
    webPreferences: {
      preload: fileURLToPath(new URL('./preload.cjs', import.meta.url)),
      additionalArguments: smoke ? ['--designer-smoke'] : []
    }
  });

  function checkSender(event) {
    if (
      window.isDestroyed() ||
      event.sender !== window.webContents ||
      event.senderFrame !== window.webContents.mainFrame ||
      event.senderFrame.url !== entry
    ) {
      throw Error('Designer IPC sender is unavailable');
    }
  }

  ipcMain.handle('designer:window', (event, action) => {
    checkSender(event);
    if (action === 'close') {
      setTimeout(() => window.close(), 0);
    } else if (['minimize', 'maximize', 'unmaximize', 'restore'].includes(action)) {
      window[action]();
    } else if (action !== 'state') {
      throw Error('Unknown Designer window action');
    }
    return { minimized: window.isMinimized(), maximized: window.isMaximized() };
  });

  app.on('window-all-closed', () => app.quit());

  if (smoke) {
    ipcMain.on('designer:smoke-stage', (event, stage) => {
      checkSender(event);
      if (
        [
          'defined',
          'container',
          'ready',
          'edited',
          'monaco',
          'geometry',
          'controls',
          'close'
        ].includes(stage)
      ) {
        console.log('DESIGNER_STAGE ' + stage);
      }
    });

    let minimized = false;

    window.once('minimize', () => {
      minimized = true;
      console.log('DESIGNER_NATIVE_MINIMIZE_OK');

      setTimeout(() => window.restore(), 350);
    });

    const deadline = setTimeout(() => {
      console.error('DESIGNER_SMOKE_TIMEOUT');
      app.exit(1);
    }, 45000);

    ipcMain.handle('designer:smoke-result', (event, result) => {
      checkSender(event);
      try {
        if (
          !result.ready ||
          !result.edited ||
          !result.monaco ||
          !result.privilegesAbsent ||
          !result.windowControls ||
          !minimized
        ) {
          throw Error(JSON.stringify(result));
        }
        clearTimeout(deadline);
        console.log('DESIGNER_UI_EDIT_MONACO_ISOLATION_OK');
        console.log('DESIGNER_WINDOW_CONTROLS_OK');
        const closing = setTimeout(() => {
          console.error('DESIGNER_HTML_CLOSE_TIMEOUT');
          app.exit(1);
        }, 5000);

        window.once('closed', () => {
          clearTimeout(closing);
          console.log('DESIGNER_HTML_CLOSE_OK');
        });
      } catch (error) {
        console.error(error);
        app.exit(1);
      }
    });
  }
  try {
    await window.loadURL(entry);
    console.log('DESIGNER_DOCUMENT_LOADED');
  } catch (error) {
    console.error(error);
    app.exit(1);
  }
}
