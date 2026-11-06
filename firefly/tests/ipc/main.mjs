/** Native IPC migration fixture: assert handler behavior and native sender ownership in both runtimes. */
'use strict';
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import { pathToFileURL, fileURLToPath } from 'node:url';
const __dirname = path.dirname(fileURLToPath(import.meta.url));
let owner;
let echoes = 0;
let authorized = 0;

function sender(event) {
  if (
    event.sender !== owner.webContents ||
    event.senderFrame !== owner.webContents.mainFrame ||
    event.senderFrame.url !== pathToFileURL(path.join(__dirname, 'page.html')).href
  ) {
    throw Error(
      'Wrong native sender ' +
        JSON.stringify({
          owner: event.sender === owner.webContents,
          frame: event.senderFrame === owner.webContents.mainFrame,
          url: event.senderFrame.url,
          expected: pathToFileURL(path.join(__dirname, 'page.html')).href
        })
    );
  }
  authorized++;
}

ipcMain.handle('boot', () => 'BOOT');

ipcMain.handle('echo', function (event, data) {
  sender(event);
  if (this !== undefined) {
    throw Error('Handler receiver');
  }
  echoes++;
  data.value.a = 2;
  return data;
});

ipcMain.handleOnce('once', (_event, value) => value + 2);

ipcMain.handle('denied', async () => {
  throw Error('denied');
});

ipcMain.handle('zero', () => undefined);

ipcMain.handle('測\0', (_event, value) => value * 2);

ipcMain.handle('parallel', async (_event, value) => {
  await new Promise((resolve) => setTimeout(resolve, (6 - value) * 2));
  return value * 2;
});

ipcMain.handle('owner', (event) => {
  sender(event);
  return 'OWNER';
});

let duplicate = false;
try {
  ipcMain.handle('echo', () => 0);
} catch {
  duplicate = true;
}

ipcMain.handle('removed', () => 0);

ipcMain.removeHandler('removed');

ipcMain.handle('removed', () => 1);

async function title(window, expected) {
  for (let i = 0; i < 500; i++) {
    const value = window.webContents.getTitle();
    if (value === expected) {
      return;
    }
    if (value.startsWith('IPC_ERROR')) {
      throw Error(value);
    }
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  throw Error('IPC page timeout: ' + window.webContents.getTitle());
}

app
  .whenReady()
  .then(async () => {
    owner = new BrowserWindow({
      show: false,
      webPreferences: { preload: path.join(__dirname, 'preload.cjs') }
    });
    await owner.loadFile('page.html');
    await title(owner, 'IPC_READY');
    const foreign = new BrowserWindow({
      show: false,
      webPreferences: { preload: path.join(__dirname, 'foreign.cjs') }
    });
    await foreign.loadFile('foreign.html');
    await title(foreign, 'IPC_FOREIGN_REJECTED');
    if (!duplicate || echoes !== 1 || authorized !== 2) {
      throw Error('Handler registration or dispatch count');
    }
    console.log(
      JSON.stringify({
        invoke: true,
        clones: true,
        early: true,
        once: true,
        parallel: true,
        sender: true,
        foreign: true
      })
    );
    owner.destroy();
    foreign.destroy();
    app.quit();
  })
  .catch((error) => {
    console.error(error.message);
    app.exit(1);
  });
