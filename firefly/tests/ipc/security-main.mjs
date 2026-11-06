/** Pending replies cannot cross navigation or window destruction; budgets are reclaimed. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { fileURLToPath } from 'node:url';

const directory = path.dirname(fileURLToPath(import.meta.url));
const waiting = [];
let pings = 0;

ipcMain.handle('ping', (event) => {
  const suffix = pings % 2 === 0 ? '/fresh.html' : '/fresh.html#same-document';
  if (!event.senderFrame.url.endsWith(suffix)) {
    throw Error(
      `Native sender URL did not refresh: ping=${pings}, expected=${suffix}, actual=${event.senderFrame.url}`
    );
  }
  pings++;
  return 'PONG';
});

ipcMain.handle('bad-result', () => ({ unsupported() {} }));

ipcMain.handle('held', (event) => new Promise((resolve) => waiting.push({ event, resolve })));

async function title(window, expected) {
  for (let index = 0; index < 500; index++) {
    const current = window.webContents.getTitle();
    if (current === expected) {
      return;
    }
    if (current.startsWith('FAIL') || current === 'OLD_REPLY_RAN') {
      throw Error(current);
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error(`Missing title ${expected}: ${window.webContents.getTitle()}`);
}

await app.whenReady();
try {
  const window = new BrowserWindow({
    show: false,
    webPreferences: {
      preload: path.join(directory, 'security.cjs')
    }
  });
  for (let cycle = 0; cycle < 3; cycle++) {
    await window.loadFile('security.html');
    await title(window, 'SECURITY_READY');
    if (waiting.length !== 16 || pings !== cycle * 8) {
      throw Error('Invalid inputs reached handlers, or pending budget was lost');
    }
    const old = waiting.splice(0);
    await window.loadFile('fresh.html');
    await title(window, 'FRESH_READY');
    if (!old.every((entry) => entry.event.senderFrame.isDestroyed())) {
      throw Error('Old sender-frame snapshot survived navigation');
    }
    for (const entry of old) {
      entry.resolve({ late: true });
    }
    await new Promise((resolve) => setTimeout(resolve, 100));
    await title(window, 'FRESH_READY');
  }
  await window.loadFile('security.html');
  await title(window, 'SECURITY_READY');
  window.destroy();
  for (const entry of waiting.splice(0)) {
    entry.resolve('reply after destruction');
  }
  await new Promise((resolve) => setTimeout(resolve, 100));
  console.log(
    JSON.stringify({
      serialization: true,
      traps: 0,
      rawCapability: false,
      pendingLimit: 16,
      navigationCycles: 3,
      destroy: true
    })
  );
  app.quit();
} catch (error) {
  console.error(error.message);
  app.exit(1);
}
