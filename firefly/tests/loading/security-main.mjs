/** File script support must not grant general file reads or network-to-file imports. */
import { app, BrowserWindow } from 'firefly';
import http from 'node:http';
import path from 'node:path';
import { pathToFileURL, fileURLToPath } from 'node:url';

const directory = path.dirname(fileURLToPath(import.meta.url));
const file = pathToFileURL(path.join(directory, 'csp-bypass.mjs')).href;

async function title(window, expected) {
  for (let attempt = 0; attempt < 500; attempt++) {
    const current = window.webContents.getTitle();
    if (current === expected) {
      return;
    }
    if (current.startsWith('FAIL:')) {
      throw Error(current);
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('Missing security title: ' + window.webContents.getTitle());
}

app
  .whenReady()
  .then(async () => {
    const window = new BrowserWindow({ show: false });
    await window.loadFile('fetch.html');
    await title(window, 'FETCH_DENIED');
    const server = http.createServer((_request, response) => {
      response.setHeader('Content-Type', 'text/html');
      response.setHeader('Connection', 'close');
      response.end(`<!doctype html><title>LOADING</title>
      <script type="module" src="${file}" onerror="document.title='NETWORK_FILE_DENIED'"></script>`);
    });
    await new Promise((resolve) => server.listen(0, '127.0.0.1', resolve));
    await window.loadURL('http://127.0.0.1:' + server.address().port + '/');
    await title(window, 'NETWORK_FILE_DENIED');
    await new Promise((resolve) => server.close(resolve));
    console.log(JSON.stringify({ fileFetchDenied: true, networkFileDenied: true }));
    window.destroy();
    app.quit();
  })
  .catch((error) => {
    console.error(error.message);
    app.exit(1);
  });
