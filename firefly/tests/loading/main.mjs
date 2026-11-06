/** Compare external file modules in actual renderer processes, without Node privileges. */
import { app, BrowserWindow } from 'firefly';

async function waitForTitle(window, expected) {
  for (let attempt = 0; attempt < 500; attempt++) {
    const title = window.webContents.getTitle();
    if (title === expected) {
      return;
    }
    if (title.startsWith('FAIL:')) {
      throw Error(title);
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error(`Missing ${expected}: ${window.webContents.getTitle()}`);
}

app
  .whenReady()
  .then(async () => {
    const window = new BrowserWindow({ show: false });
    await window.loadFile('index.html');
    await waitForTitle(window, 'MODULES_OK');
    // A new document gets its own module map, while imports within it stay cached.
    await window.loadFile('index.html');
    await waitForTitle(window, 'MODULES_OK');
    await window.loadFile('mime.html');
    await waitForTitle(window, 'MIME_REJECTED');
    await window.loadFile('csp.html');
    await new Promise((resolve) => setTimeout(resolve, 200));
    if (window.webContents.getTitle() !== 'CSP_BLOCKED') {
      throw Error('Local module policy bypassed CSP');
    }
    console.log(
      JSON.stringify({
        external: true,
        dynamic: true,
        cache: true,
        relative: true,
        unicode: true,
        reload: true,
        mime: true,
        csp: true,
        node: false
      })
    );
    window.destroy();
    app.quit();
  })
  .catch((error) => {
    console.error(error.message);
    app.exit(1);
  });
