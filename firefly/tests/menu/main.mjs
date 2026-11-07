/** Actual main-host exports and native application-menu lifetime; no BrowserWindow. */
import firefly, { app, Menu, MenuItem } from 'firefly';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

try {
  const alias = await import('electron');
  check(
    firefly.Menu === Menu && alias.Menu === Menu && require('electron').MenuItem === MenuItem,
    'ESM/CommonJS main menu exports'
  );
  check(!app.isReady(), 'before-ready entry');
  Menu.setApplicationMenu(null);
  check(Menu.getApplicationMenu() === null, 'before-ready suppression');
  const item = new MenuItem({ id: 'check', label: 'Checked', type: 'checkbox' });
  const menu = Menu.buildFromTemplate([{ label: 'Test', submenu: [item] }]);
  Menu.setApplicationMenu(menu);
  check(Menu.getApplicationMenu() === menu, 'installed menu identity');
  item.checked = true;
  item.label = 'Updated 世界';
  check(menu.getMenuItemById('check') === item, 'menu lookup');
  await app.whenReady();
  Menu.setApplicationMenu(null);
  check(Menu.getApplicationMenu() === null, 'after-ready native removal');
  console.log('PASS actual Firefly main menu exports/install/update/remove without a window');
  app.exit(0);
} catch (error) {
  console.error(error);
  app.exit(1);
}
