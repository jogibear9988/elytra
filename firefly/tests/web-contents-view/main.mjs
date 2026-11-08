/** Batched real-renderer ownership acceptance. Every native window remains hidden. */
import { app, BrowserWindow, WebContentsView, webContents, ipcMain } from 'firefly';
import path from 'node:path';

const directory = import.meta.dirname;
// The runner creates this isolated directory before the native app starts.
app.setPath('userData', path.join(directory, 'config'));
const limited = process.versions.electron === undefined;
const windows = [];
const entries = [];
const requests = new Map();
let sequence = 0;
let stage = 'app readiness';

const wait = (duration) => new Promise((resolve) => setTimeout(resolve, duration));

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function cleanup() {
  for (const entry of entries) {
    try {
      if (!entry.contents.isDestroyed()) {
        entry.contents.close({ waitForBeforeUnload: false });
      }
    } catch (error) {
      console.error('Child cleanup: ' + error.message);
    }
  }
  for (const window of windows) {
    try {
      if (!window.isDestroyed()) {
        window.destroy();
      }
    } catch (error) {
      console.error('Window cleanup: ' + error.message);
    }
  }
}

app.on('window-all-closed', () => {});

ipcMain.handle('view:ready', (event, identity) => {
  const entry = entries.find((value) => value.contents === event.sender);
  check(entry && entry.identity === identity, 'Ready sender belongs to its independent view');
  entry.ready = true;
  return true;
});

ipcMain.handle('view:echo', (event, value) => {
  check(
    entries.some((entry) => entry.contents === event.sender),
    'Echo sender is a live child'
  );
  check(value.bytes instanceof Uint8Array && value.bytes.length === 4096, 'Native binary invoke');
  return value;
});

ipcMain.handle('view:report', (event, report) => {
  const request = requests.get(report.token);
  check(request && request.entry.contents === event.sender, 'Report cannot cross view ownership');
  requests.delete(report.token);
  request.resolve(report);
  return true;
});

async function sample(entry, title, width, height) {
  const deadline = Date.now() + 3500;
  let observed;
  do {
    const token = ++sequence;
    const pending = new Promise((resolve) => requests.set(token, { entry, resolve }));
    entry.contents.send('view:sample', { token });
    const report = await pending;
    observed = report;
    check(!report.error, report.error);
    check(
      report.identity === entry.identity && report.secure && report.nodeAbsent && report.binary,
      'Actual isolated page/preload and concurrent binary IPC roundtrips'
    );
    check(
      Number.isInteger(report.pid) && report.pid > 0 && report.pid !== process.pid,
      'Real preload PID'
    );
    check(report.title === title, 'Expected independent document title');
    if (report.width === width && report.height === height) {
      return report;
    }
    await wait(30);
  } while (Date.now() < deadline);
  throw Error(
    'Renderer viewport did not follow native bounds: ' +
      JSON.stringify({
        identity: entry.identity,
        expected: { width, height },
        actual: { width: observed?.width, height: observed?.height }
      })
  );
}

async function loaded(entry, file) {
  entry.ready = false;
  await entry.contents.loadFile(path.join(directory, file));
  const deadline = Date.now() + 3500;
  while (!entry.ready && Date.now() < deadline) {
    await wait(20);
  }
  check(entry.ready, 'Actual rendered page installed its preload bridge');
}

const watchdog = setTimeout(() => {
  console.error('FAIL WebContentsView acceptance timed out at ' + stage);
  cleanup();

  setTimeout(() => app.exit(1), 100);
}, 30000);

app
  .whenReady()
  .then(async () => {
    stage = 'hidden host creation';
    const first = new BrowserWindow({ show: false, width: 500, height: 350 });
    const second = new BrowserWindow({ show: false, width: 500, height: 350 });
    windows.push(first, second);
    check(first.contentView.children.length === 0, 'Primary contents is outside overlay children');
    for (const identity of ['A', 'B']) {
      const view = new WebContentsView({
        webPreferences: {
          preload: path.join(directory, 'preload.cjs'),
          additionalArguments: ['--view-fixture=' + identity]
        }
      });
      const contents = view.webContents;
      entries.push({ identity, view, contents, id: contents.id, ready: false });
      check(
        JSON.stringify(view.getBounds()) === JSON.stringify({ x: 0, y: 0, width: 0, height: 0 }) &&
          view.getVisible() === true,
        'Fresh view defaults'
      );
      check(webContents.fromId(contents.id) === contents, 'Public contents registry identity');
      check(
        webContents.getAllWebContents().includes(contents),
        'Public contents registry includes child'
      );
      if (limited) {
        const edge = { x: -2147483648, y: -2147483648, width: 0, height: 0 };
        view.setBounds(edge);
        check(
          JSON.stringify(view.getBounds()) === JSON.stringify(edge),
          'Native INT_MIN zero-size bounds roundtrip'
        );
        let failure;
        try {
          view.setBounds({ x: 2147483647, y: 0, width: 1, height: 0 });
        } catch (error) {
          failure = error;
        }
        check(failure?.code === 'ERR_INVALID_ARG_VALUE', 'Native coordinate overflow is rejected');
        check(
          JSON.stringify(view.getBounds()) === JSON.stringify(edge),
          'Rejected bounds preserve native geometry'
        );
      }
      view.setBounds({ x: identity === 'A' ? 0 : 200, y: 0, width: 180, height: 110 });
      first.contentView.addChildView(view);
    }
    const [a, b] = entries;
    const snapshot = first.contentView.children;
    snapshot.length = 0;
    check(first.contentView.children.length === 2, 'Children getter returns a snapshot');
    first.contentView.addChildView(a.view);
    check(first.contentView.children[1] === a.view, 'Re-add moves existing child to top');
    first.contentView.addChildView(a.view, 0);
    check(first.contentView.children[0] === a.view, 'Explicit insertion order');
    second.contentView.removeChildView(a.view);
    check(first.contentView.children.includes(a.view), 'Removing from wrong parent is harmless');
    a.view.setVisible(false);
    check(a.view.getVisible() === false, 'Native view visibility flag');
    a.view.setVisible(true);
    stage = 'independent real page loads';
    await loaded(a, 'page-a.html');
    await loaded(b, 'page-b.html');
    const originalA = await sample(a, 'View document A', 180, 110);
    const originalB = await sample(b, 'View document B', 180, 110);
    const pidA = a.contents.getOSProcessId();
    const pidB = b.contents.getOSProcessId();
    check(
      Number.isInteger(pidA) &&
        pidA > 0 &&
        pidA !== process.pid &&
        Number.isInteger(pidB) &&
        pidB > 0 &&
        pidB !== process.pid,
      'Actual Ladybird/Electron renderer PIDs'
    );
    check(a.contents.getOwnerBrowserWindow() === first, 'Initial view owner');
    stage = 'native reparent and resize';
    second.contentView.addChildView(a.view);
    check(
      !first.contentView.children.includes(a.view) && second.contentView.children[0] === a.view,
      'Reparent updates both parents'
    );
    check(a.contents.getOwnerBrowserWindow() === second, 'Reparent updates owner lookup');
    check(a.contents.getOSProcessId() === pidA, 'Reparent preserves native renderer PID');
    a.view.setBounds({ x: 5, y: 7, width: 150, height: 95 });
    const resized = await sample(a, 'View document A', 150, 95);
    check(
      resized.documentIdentity === originalA.documentIdentity && resized.pid === originalA.pid,
      'Reparent retains document and preload process'
    );
    const bounds = a.view.getBounds();
    bounds.width = 999;
    check(a.view.getBounds().width === 150, 'Bounds getter returns a copy');
    stage = 'detached navigation';
    second.contentView.removeChildView(a.view);
    check(
      second.contentView.children.length === 0 && !a.contents.isDestroyed(),
      'Detached child remains alive'
    );
    await loaded(a, 'page-b.html');
    const detached = await sample(a, 'View document B', 150, 95);
    second.contentView.addChildView(a.view);
    check(
      (await sample(a, 'View document B', 150, 95)).documentIdentity === detached.documentIdentity,
      'Detached navigation survives reattachment'
    );
    stage = 'old host destruction';
    const primary = first.webContents;
    first.destroy();
    const closedDeadline = Date.now() + 3500;
    while (!primary.isDestroyed() && Date.now() < closedDeadline) {
      await wait(20);
    }
    check(primary.isDestroyed() && first.isDestroyed(), 'Host destroys its primary webContents');
    check(
      !b.contents.isDestroyed() && b.contents.getOSProcessId() === pidB,
      'Extra native view survives host destruction'
    );
    const survivor = await sample(b, 'View document B', 180, 110);
    check(
      survivor.documentIdentity === originalB.documentIdentity && survivor.pid === originalB.pid,
      'Surviving native document and IPC remain usable'
    );
    second.contentView.addChildView(b.view);
    check(b.contents.getOwnerBrowserWindow() === second, 'Orphan child can attach to another host');
    stage = 'explicit child close';
    for (const entry of entries) {
      let destroyed = 0;

      entry.contents.on('destroyed', () => ++destroyed);

      const ended = new Promise((resolve) => entry.contents.once('destroyed', resolve));
      entry.contents.close({ waitForBeforeUnload: false });
      await ended;
      await wait(0);
      check(
        destroyed === 1 && entry.contents.isDestroyed() && entry.view.webContents === undefined,
        'Explicit close retires contents once'
      );
      check(
        webContents.fromId(entry.id) === undefined &&
          !webContents.getAllWebContents().includes(entry.contents),
        'Destroyed contents leave public registry'
      );
    }
    check(
      windows.every((window) => window.isDestroyed() || !window.isVisible()),
      'All windows remain hidden'
    );
    cleanup();
    clearTimeout(watchdog);
    console.log(
      'PASS public WebContentsView: real pages/PIDs/viewport/binary IPC, order/reparent/detached navigation, host and child lifetime'
    );
    app.exit(0);
  })
  .catch((error) => {
    console.error('WebContentsView stage: ' + stage + '\n' + (error.stack ?? error));
    clearTimeout(watchdog);
    cleanup();

    setTimeout(() => app.exit(1), 100);
  });
