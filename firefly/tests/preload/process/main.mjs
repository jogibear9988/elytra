/** Hidden windows exercise immutable launch snapshots, real helper identity and Code-OSS config IPC. */
import { app, BrowserWindow, ipcMain } from 'firefly';
import path from 'node:path';
import fs from 'node:fs';
import { fileURLToPath } from 'node:url';
const directory = path.dirname(fileURLToPath(import.meta.url));
const reference = process.argv.includes('--reference');
const key = 'FIREFLY_PROCESS_FIXTURE';
const entries = [];

function assert(value, message) {
  if (!value) {
    throw Error(message);
  }
}

app.on('window-all-closed', () => {});

ipcMain.handle('vscode:window-config', (event, id) => {
  assert(
    entries.some((entry) => entry.window.webContents === event.sender && entry.id === id),
    'Config sender'
  );
  return { userEnv: { FIREFLY_USER_ENV: 'merged' }, id };
});

ipcMain.handle('process:report', (event, value) => {
  const entry = entries.find((entry) => entry.window.webContents === event.sender);
  assert(entry, 'Report sender');
  entry.report = value;
  return true;
});

app
  .whenReady()
  .then(async () => {
    const invalid = [
      [null, 'ERR_INVALID_ARG_TYPE'],
      [['ok', 1], 'ERR_INVALID_ARG_TYPE'],
      [Array(1), 'ERR_INVALID_ARG_TYPE'],
      [Array(65).fill(''), 'ERR_INVALID_ARG_VALUE'],
      [['bad\0value'], 'ERR_INVALID_ARG_VALUE'],
      [['\ud800'], 'ERR_INVALID_ARG_VALUE'],
      [['a'.repeat(4097)], 'ERR_INVALID_ARG_VALUE'],
      [Array(5).fill('a'.repeat(4096)), 'ERR_INVALID_ARG_VALUE']
    ];
    if (!reference) {
      for (const [additionalArguments, code] of invalid) {
        let result;
        try {
          new BrowserWindow({
            show: false,
            webPreferences: { preload: path.join(directory, 'preload.cjs'), additionalArguments }
          });
        } catch (error) {
          result = error.code;
        }
        assert(result === code, 'Argument rejection: ' + result + ' expected ' + code);
      }
      let rejected = false;
      try {
        new BrowserWindow({ show: false, webPreferences: { additionalArguments: ['orphan'] } });
      } catch (error) {
        rejected = error.code === 'ERR_FIREFLY_UNSUPPORTED_API';
      }
      assert(rejected, 'Arguments without preload must fail explicitly');
    }
    for (const id of ['A', 'B']) {
      process.env[key] = 'snapshot-' + id;
      const args = [
        '--vscode-window-config=' + id,
        'Unicode ä ✓ = space',
        '--firefly-process-fixture=ordinary',
        '',
        'duplicate',
        'duplicate'
      ];
      const window = new BrowserWindow({
        show: false,
        webPreferences: { preload: path.join(directory, 'preload.cjs'), additionalArguments: args }
      });
      entries.push({ id, args: [...args], window, report: null });
      args[0] = '--vscode-window-config=INVALID';
      // Load before changing the parent's environment so the Electron reference also
      // receives its native per-renderer environment at creation.
      await window.loadFile(path.join(directory, 'page.html'));
    }
    for (let round = 0; round < 2; round++) {
      process.env[key] = 'parent-' + round;
      for (const entry of entries) {
        if (round) {
          entry.report = null;
          await entry.window.loadFile(path.join(directory, 'page.html'));
        }
        for (let attempt = 0; attempt < 500 && !entry.report; attempt++) {
          await new Promise((resolve) => setTimeout(resolve, 10));
        }
        const report = entry.report;
        assert(report && !report.error, 'Missing/failed process report: ' + JSON.stringify(report));
        assert(
          report.pageIsolation &&
            report.frameIsolation &&
            report.flags &&
            report.localEnv &&
            report.config,
          'Process/isolation: ' + JSON.stringify(report)
        );
        assert(
          report.pid === entry.window.webContents.getOSProcessId() && report.pid !== process.pid,
          'Real renderer PID'
        );
        assert(
          report.platform === process.platform && report.arch === process.arch,
          'Native platform/arch'
        );
        assert(
          typeof report.argv[0] === 'string' && fs.statSync(report.execPath).isFile(),
          'Real executable path'
        );
        assert(
          reference
            ? entry.args
                .filter((value) => value !== '')
                .every(
                  (value) =>
                    report.argv.filter((item) => item === value).length ===
                    entry.args.filter((item) => item === value).length
                )
            : JSON.stringify(report.argv.slice(-entry.args.length)) === JSON.stringify(entry.args),
          'App argv snapshot: ' + JSON.stringify(report.argv)
        );
        assert(
          process.env[key] === 'parent-' + round && process.env.FIREFLY_USER_ENV === undefined,
          'Renderer cannot mutate parent environment'
        );
        if (!reference) {
          assert(
            report.originalEnv === 'snapshot-' + entry.id,
            'Original window environment on reload'
          );
          assert(
            report.execPath === report.argv[0] && report.argv.length === entry.args.length + 1,
            'Internal control arguments stay private'
          );
          assert(
            report.version === 'v0.1.0' &&
              report.versions.firefly === '0.1.0' &&
              /^[a-f0-9]{40}$/.test(report.versions.libjs) &&
              report.versions.libjs === report.versions.ladybird &&
              report.versions.libjs === process.versions.libjs,
            'Truthful engine versions'
          );
          assert(
            report.versions.node === undefined &&
              report.versions.v8 === undefined &&
              report.versions.electron === undefined,
            'No fake Node/V8/Electron identity'
          );
          assert(
            report.unsupported && report.nativeExportDenied,
            'Explicit unsupported methods and raw native publication guard'
          );
        }
        console.log('PROCESS_HELPER=' + report.pid);
      }
    }
    // Let the Python observer independently verify live native parent/PID identity.
    console.log('PROCESS_OBSERVE_READY');
    await new Promise((resolve) => setTimeout(resolve, 1500));
    for (const entry of entries) {
      entry.window.destroy();
    }
    delete process.env[key];
    console.log(
      'PASS sandbox process: argv/env/reload/multiwindow/native identity/config IPC/isolation'
    );
    app.exit(0);
  })
  .catch((error) => {
    console.error(error.stack ?? error);
    app.exit(1);
  });
