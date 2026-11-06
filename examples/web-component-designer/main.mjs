/**
 * Entry for the pinned Designer sample. Mite serves assets; Firefly owns the native window.
 */
import { startServer } from './server.mjs';
import { randomUUID } from 'node:crypto';
import { readFile } from 'node:fs/promises';
import { fileURLToPath } from 'node:url';

const web = fileURLToPath(new URL('./web/', import.meta.url));
let report;
const smoke = process.argv.includes('--smoke')
  ? {
      token: randomUUID(),
      script: await readFile(fileURLToPath(new URL('./smoke.mjs', import.meta.url))),

      report: (body) => report(body)
    }
  : undefined;
let window;
const controls = process.argv.includes('--serve-only')
  ? undefined
  : {
      token: randomUUID(),

      execute: (action) => {
        if (!window || window.isDestroyed()) {
          throw Error('Window unavailable');
        }
        if (action === 'close') {
          setTimeout(() => window.close(), 0);
        } else if (action !== 'state') {
          window[action]();
        }
        return { minimized: window.isMinimized(), maximized: window.isMaximized() };
      }
    };
const server = await startServer(web, smoke, controls);
const url = `http://127.0.0.1:${server.address().port}/`;
if (process.argv.includes('--serve-only')) {
  console.log(url);
} else {
  const { app, BrowserWindow } = await import('firefly');
  await app.whenReady();
  window = new BrowserWindow({
    frame: false,
    width: 1280,
    height: 850,
    title: 'Web Component Designer · Firefly'
  });

  app.on('window-all-closed', () => app.quit());

  app.on('quit', () => server.close());

  if (smoke) {
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

    report = (body) => {
      try {
        const result = JSON.parse(body);
        if (
          !result.ready ||
          !result.edited ||
          !result.monaco ||
          !result.privilegesAbsent ||
          !result.windowControls ||
          !minimized
        ) {
          throw Error(body);
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
    };
  }
  try {
    await window.loadURL(url);
    console.log('DESIGNER_DOCUMENT_LOADED');
  } catch (error) {
    console.error(error);
    app.exit(1);
  }
}
