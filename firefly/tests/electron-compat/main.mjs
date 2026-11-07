/** Real native host/renderer acceptance for the explicitly enabled import alias. */
import firefly, { app, BrowserWindow } from 'firefly';
import { createRequire, isBuiltin } from 'node:module';
import path from 'node:path';
const require = createRequire(import.meta.url);
const compatible = process.argv[2] === 'compat';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

async function run() {
  check(process.argv[3] === '--electron-compat', 'Application args remain untouched');
  const imported = await import('electron');
  const legacy = require('electron');
  if (compatible) {
    check(imported.default === firefly && legacy === firefly, 'Same ESM/CommonJS host singleton');
    check(imported.app === app && imported.BrowserWindow === BrowserWindow, 'Named host exports');
    check(require.resolve('electron') === 'electron', 'Host alias wins over npm');
    check(
      imported.protocol === firefly.protocol &&
        legacy.protocol === firefly.protocol &&
        imported.Menu === firefly.Menu &&
        legacy.MenuItem === firefly.MenuItem &&
        !('contentTracing' in imported),
      'Partial protocol/menu exports are shared; unsupported contentTracing remains unavailable'
    );
  } else {
    check(
      legacy.marker === 'NPM_PACKAGE' && imported.default === legacy,
      'Default ordinary npm lookup'
    );
  }
  check(
    !isBuiltin('electron') && process.versions.electron === undefined,
    'No fabricated engine identity'
  );
  await app.whenReady();
  const window = new BrowserWindow({
    show: false,
    webPreferences: { preload: path.join(import.meta.dirname, 'preload.cjs') }
  });
  await window.loadFile('index.html');
  for (let attempt = 0; attempt < 300; ++attempt) {
    const title = window.webContents.getTitle();
    if (title === `ALIAS_${compatible}`) {
      break;
    }
    check(title !== 'FAIL', 'Renderer alias isolation');
    await new Promise((resolve) => setTimeout(resolve, 10));
  }
  check(window.webContents.getTitle() === `ALIAS_${compatible}`, 'Actual preload/page round trip');
  console.log(
    JSON.stringify({ compatible, sameHost: true, isolatedPreload: true, noPagePrivileges: true })
  );
  window.destroy();
  app.quit();
}

run().catch((error) => {
  console.error(error.message);
  app.exit(1);
});
