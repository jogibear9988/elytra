const { app, BrowserWindow } = require('firefly');
const path = require('node:path');

app.whenReady().then(async () => {
  const win = new BrowserWindow({ width: 800, height: 600, title: 'Firefly' });
  await win.loadFile(path.join(__dirname, 'index.html'));
  console.log('Firefly loaded with renderer PID', win.webContents.getOSProcessId());
});
app.on('window-all-closed', () => app.quit());
