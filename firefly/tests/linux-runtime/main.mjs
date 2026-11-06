/** Native Linux app slice: Mite HTTP, real window, isolated preload/IPC and native clipboard. */
import { app, BrowserWindow, ipcMain, clipboard, screen, nativeTheme } from 'firefly';
import { createServer } from 'node:http';
import { readFile } from 'node:fs/promises';
import { once } from 'node:events';
import { fileURLToPath } from 'node:url';
import { setTimeout as sleep } from 'node:timers/promises';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

function unsupported(operation) {
  try {
    operation();
    throw new Error('unfinished native service returned fabricated data');
  } catch (error) {
    check(error.code === 'ERR_FIREFLY_UNSUPPORTED_API', 'missing explicit native service refusal');
  }
}

app
  .whenReady()
  .then(async () => {
    const deadline = setTimeout(() => {
      console.error('Linux runtime acceptance deadline exceeded');
      app.exit(1);
    }, 25000);
    const assets = new Map();
    for (const [path, file, type] of [
      ['/', 'index.html', 'text/html; charset=utf-8'],
      ['/renderer.mjs', 'renderer.mjs', 'text/javascript; charset=utf-8']
    ]) {
      assets.set(path, { bytes: await readFile(new URL(file, import.meta.url)), type });
    }
    const server = createServer((request, response) => {
      const asset = assets.get(request.url);
      if (request.method !== 'GET' || !asset) {
        response.writeHead(404).end();
        return;
      }
      response.writeHead(200, {
        'Content-Type': asset.type,
        'Content-Security-Policy': "default-src 'self'; script-src 'self'; connect-src 'none'"
      });
      response.end(asset.bytes);
    });
    server.listen(0, '127.0.0.1');
    await once(server, 'listening');
    const url = `http://127.0.0.1:${server.address().port}/`;
    const window = new BrowserWindow({
      title: 'Linux runtime acceptance',
      width: 640,
      height: 420,
      webPreferences: {
        preload: fileURLToPath(new URL('preload.cjs', import.meta.url)),
        sandbox: true,
        contextIsolation: true,
        nodeIntegration: false
      }
    });
    let calls = 0;

    ipcMain.handle('linux:roundtrip', (event, value) => {
      check(
        event.sender === window.webContents &&
          event.senderFrame === window.webContents.mainFrame &&
          event.senderFrame.url === url,
        'native IPC sender ownership/URL'
      );
      check(value.text === 'Grüße 🪲\0tail' && value.nested.value === 42, 'IPC input changed');
      calls++;
      return { text: value.text, answer: value.nested.value + 1 };
    });

    await window.loadURL(url);
    for (
      let attempt = 0;
      attempt < 1000 && window.webContents.getTitle() !== 'LINUX_RUNTIME_OK';
      attempt++
    ) {
      check(
        !window.webContents.getTitle().startsWith('LINUX_RUNTIME_ERROR'),
        window.webContents.getTitle()
      );
      await sleep(10);
    }
    check(
      window.webContents.getTitle() === 'LINUX_RUNTIME_OK' && calls === 1,
      'renderer/preload IPC did not complete'
    );
    window.setContentSize(480, 300);
    window.center();
    const bounds = window.getBounds();
    check(bounds.width > 0 && bounds.height > 0, 'native window bounds');
    const text = 'Native Linux clipboard 🪲';
    await clipboard.writeText(text);
    check((await clipboard.readText()) === text, 'native clipboard did not round-trip');

    unsupported(() => screen.getAllDisplays());

    unsupported(() => nativeTheme.shouldUseDarkColors);

    unsupported(() => nativeTheme.themeSource);

    unsupported(() => {
      nativeTheme.themeSource = 'system';
    });

    const observed = {
      renderer: window.webContents.getOSProcessId(),
      ipc: calls,
      clipboard: true,
      explicitUnsupported: true,
      rendered: true
    };
    // The independent peer checks the live kernel process state before replying.
    const result = await fetch(process.env.FIREFLY_RUNTIME_CONTROL_URL, {
      method: 'POST',
      body: JSON.stringify(observed)
    });
    check(
      result.status === 200 && (await result.text()) === 'VERIFIED',
      'independent kernel check failed'
    );
    console.log(JSON.stringify(observed));
    ipcMain.removeHandler('linux:roundtrip');
    clearTimeout(deadline);
    window.destroy();
    server.close();
    app.quit();
  })
  .catch((error) => {
    console.error(error.stack || String(error));
    app.exit(1);
  });
