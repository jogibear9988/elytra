import { app, BrowserWindow } from 'firefly';
import { fileURLToPath } from 'node:url';
import process from 'node:process';

const trace = [];
const smoke = process.argv.includes('--smoke');
let deferred = false;
app.on('ready', () => trace.push('ready'));
app.on('before-quit', (event) => {
  if (smoke && !deferred) {
    deferred = true;
    event.preventDefault();
    trace.push('before-quit:deferred');
    setTimeout(() => app.quit(), 0);
  } else trace.push('before-quit');
});
app.on('will-quit', () => trace.push('will-quit'));
app.on('quit', (_event, code) => {
  trace.push(`quit:${code}`);
  console.log(JSON.stringify({ trace }));
});
app.on('window-all-closed', () => {
  trace.push('window-all-closed');
  app.quit();
});
app
  .whenReady()
  .then(async () => {
    trace.push('when-ready');
    if (!app.isReady()) throw Error('Readiness promise resolved too early');
    const forced = process.argv.find((value) => value.startsWith('--exit-code='));
    if (forced) {
      app.exit(Number(forced.slice('--exit-code='.length)));
      return;
    }
    const window = new BrowserWindow({ width: 800, height: 500 });
    window.on('close', () => trace.push('close'));
    window.on('closed', () => trace.push('closed'));
    await window.loadFile(fileURLToPath(new URL('./index.html', import.meta.url)));
    trace.push('loaded');
    if (smoke) app.quit();
  })
  .catch((error) => {
    console.error(error.message);
    app.exit(1);
  });
