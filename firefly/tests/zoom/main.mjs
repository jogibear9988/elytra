/** Main-process native zoom and real renderer layout; every presentation host stays hidden. */
import { app, BrowserWindow, WebContentsView, ipcMain } from 'firefly';
import assert from 'node:assert/strict';
import path from 'node:path';

const reference = process.argv.includes('--reference');
const directory = import.meta.dirname;
app.setPath('userData', path.join(directory, 'config'));

app.on('window-all-closed', () => {});

let window;
let child;
let independent;
const reports = [];

const close = (a, b) => Math.abs(a - b) < 0.00001;

const fail = (message) => {
  throw Error(message);
};

const watchdog = setTimeout(() => {
  console.error('Public zoom probe timed out');
  app.exit(1);
}, 25000);

ipcMain.handle('public-zoom:config', (event) => {
  assert.equal(event.sender, window.webContents);
  return { round: reports.length + 1, initial: reports.length && !reference ? 0.75 : 1.44 };
});

ipcMain.handle('public-zoom:command', (event, command) => {
  assert.equal(event.sender, window.webContents);
  const contents = window.webContents;
  if (command.kind === 'factor') {
    contents.setZoomFactor(command.value);
  } else if (command.kind === 'level') {
    contents.setZoomLevel(command.value);
  } else if (command.kind !== 'observe') {
    fail('Unknown native zoom command');
  }
  const factor = contents.getZoomFactor();
  const level = contents.getZoomLevel();
  assert.ok(close(factor, command.kind === 'level' ? 1.2 ** command.value : command.value));
  assert.ok(close(level, Math.log(factor) / Math.log(1.2)));
  return { factor, level };
});

ipcMain.handle('public-zoom:report', (event, report) => {
  assert.equal(event.sender, window.webContents);
  if (report.error) {
    console.error('Renderer zoom measurement failed:', JSON.stringify(report));
  }
  assert.equal(report.error, undefined, JSON.stringify(report));
  assert.equal(report.round, reports.length + 1);
  reports.push(report);
  return true;
});

ipcMain.handle('public-zoom:independent', (event, sample) => {
  assert.equal(event.sender, child.webContents);
  assert.equal(sample.error, undefined, JSON.stringify(sample));
  independent = sample;
  return true;
});

async function waitFor(predicate, label) {
  const end = Date.now() + 6000;
  while (!predicate()) {
    assert.ok(Date.now() < end, `Missing ${label}: ${JSON.stringify(reports)}`);
    await new Promise((resolve) => setTimeout(resolve, 15));
  }
}

app.whenReady().then(async () => {
  try {
    window = new BrowserWindow({
      show: false,
      width: 600,
      height: 400,
      webPreferences: { preload: path.join(directory, 'preload.cjs'), zoomFactor: 1.44 }
    });
    if (!reference) {
      // Electron applies the constructor preference during first navigation;
      // Firefly's native view already has its requested factor before that load.
      assert.ok(close(window.webContents.getZoomFactor(), 1.44));
      assert.ok(close(window.webContents.getZoomLevel(), 2));
      child = new WebContentsView({
        webPreferences: { preload: path.join(directory, 'preload.cjs'), zoomFactor: 1.2 }
      });
      child.setBounds({ x: 0, y: 0, width: 240, height: 160 });
      assert.ok(close(child.webContents.getZoomFactor(), 1.2));
      await child.webContents.loadFile(path.join(directory, 'independent.html'));
      await waitFor(() => independent, 'independent native renderer baseline');
    }
    await window.loadFile(path.join(directory, 'page.html'));
    await waitFor(() => reports.length === 1, 'runtime native zoom layout');
    console.log(
      'PASS public zoom constructor and main setters match real renderer layout DPR and preload getters'
    );
    assert.ok(close(window.webContents.zoomFactor, 1));
    window.webContents.zoomFactor = 1.2;
    assert.ok(close(window.webContents.getZoomLevel(), 1));
    window.webContents.zoomLevel = -1;
    assert.ok(close(window.webContents.getZoomFactor(), 1 / 1.2));
    assert.ok(close(window.webContents.zoomLevel, -1));
    if (!reference) {
      assert.ok(close(child.webContents.getZoomFactor(), 1.2));
      const oldSample = independent;
      await waitFor(() => independent !== oldSample, 'fresh independent renderer sample');
      assert.ok(close(independent.factor, 1.2));
      assert.ok(close(independent.dpr, oldSample.dpr));
      assert.equal(independent.width, oldSample.width);
      console.log(
        'PASS Firefly zoom independent WebContentsView constructor and native renderer state'
      );
      const before = window.webContents.getZoomFactor();
      for (const value of [NaN, Infinity, 0, -1, 0.29, 5.01, '1', {}]) {
        assert.throws(() => window.webContents.setZoomFactor(value));

        assert.ok(close(window.webContents.getZoomFactor(), before));
      }
      for (const value of [NaN, Infinity, '1', {}, -1000, 1000]) {
        assert.throws(() => window.webContents.setZoomLevel(value));

        assert.ok(close(window.webContents.getZoomFactor(), before));
      }
      console.log('PASS Firefly zoom finite range and strict types reject without mutation');
    }
    window.webContents.setZoomFactor(0.75);
    await window.loadFile(path.join(directory, 'page.html'));
    // The second page performs an ordinary location.reload after its accepted report.
    // Electron reapplies its constructor preference; Firefly retains current view zoom.
    await waitFor(() => reports.length === 3, 'actual navigation and renderer reload');
    assert.ok(close(window.webContents.getZoomFactor(), reference ? 1.44 : 0.75));
    assert.equal(window.isVisible(), false);
    console.log(
      'PASS public zoom navigation and reload follow verified native view policy while hidden'
    );
    child?.webContents.close({ waitForBeforeUnload: false });
    window.destroy();
    clearTimeout(watchdog);
    app.exit(0);
  } catch (error) {
    console.error(error.stack ?? error);
    clearTimeout(watchdog);
    app.exit(1);
  }
});
