import {app, BrowserWindow} from 'firefly';
import {fileURLToPath} from 'node:url';
import process from 'node:process';

app.on('window-all-closed', () => app.quit());
async function createWindow() {
  const window = new BrowserWindow({
    width: 800,
    height: 600,
    title: 'Firefly',
    webPreferences: {preload: fileURLToPath(new URL('./preload.cjs', import.meta.url))}
  });
  await window.loadFile(fileURLToPath(new URL('./index.html', import.meta.url)));
  console.log('Firefly loaded with renderer PID', window.webContents.getOSProcessId());

  // Explicit native smoke mode; ordinary launches keep the window open.
  if (process.argv.includes('--smoke')) {
    for (let i = 0; i < 300 && window.webContents.getTitle() !== 'Firefly'; ++i) {
      await new Promise(resolve => setTimeout(resolve, 10));
    }
    if (window.webContents.getTitle() !== 'Firefly') throw Error('ESM page/preload did not initialize');
    console.log('HELLO_ESM_PRELOAD_OK');
    window.destroy();
    app.quit();
  }
}

app.whenReady().then(createWindow).catch(error => {
  console.error(error.message);
  app.exit(1);
});
