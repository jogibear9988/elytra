/** Real native provider with actual queued LibJS dispatch; no fabricated click result. */
import * as native from 'menu-test';
import createMenus from '../../core/menu/index.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

const delay = () => new Promise((resolve) => setTimeout(resolve, 15));

// Exercise the independent native boundary before any successful generation.
for (const wire of [
  'null',
  JSON.stringify({ schema: 1.5, generation: 1, items: null }),
  JSON.stringify({ schema: 1, generation: 0, items: null }),
  JSON.stringify({ schema: 1, generation: 1, items: null, unknown: true }),
  ' '.repeat(65537)
]) {
  let rejected = false;
  try {
    native.menuApply(wire);
  } catch {
    rejected = true;
  }
  check(rejected && !JSON.parse(native.state()).bar, 'native schema/budget rejection');
}

let quits = 0;
let calls = 0;
const menus = createMenus(
  native,
  error,
  unsupported,
  () => ++quits,
  () => undefined
);
const { Menu, MenuItem } = menus;

native.setHandler((wire) => menus.dispatch(0, JSON.parse(wire)));

check(Menu.getApplicationMenu() === null, 'initial menu');
Menu.setApplicationMenu(null);
check(!JSON.parse(native.state()).bar, 'null suppression');
const checkbox = new MenuItem({
  id: 'check',
  label: 'Unicode 世界',
  type: 'checkbox',

  click(item, window, event) {
    check(
      this === undefined && item === checkbox && window === undefined,
      'callback identity/context'
    );
    check(typeof event.metaKey === 'boolean', 'native modifiers');
    ++calls;
  }
});
const quit = new MenuItem({
  role: 'quit',

  click() {
    throw Error('role click must be ignored');
  }
});
const menu = Menu.buildFromTemplate([
  { label: 'Example', submenu: [checkbox, { type: 'separator' }, quit] }
]);
Menu.setApplicationMenu(menu);
check(
  Menu.getApplicationMenu() === menu && menu.getMenuItemById('check') === checkbox,
  'public identity'
);
let state = JSON.parse(native.state(checkbox.commandId));
check(state.bar && state.label === 'Unicode 世界', 'native label/tree');
if (process.argv.includes('--offscreen')) {
  check(state.platform === 'offscreen' && !state.native, 'explicit offscreen menu tree only');
} else if (process.platform === 'darwin') {
  check(state.native, 'real native macOS menu bar');
}
check(native.trigger(checkbox.commandId), 'native QAction missing');
await delay();
check(
  calls === 1 && checkbox.checked && JSON.parse(native.state(checkbox.commandId)).checked,
  'native checkbox callback/state'
);
checkbox.enabled = false;
check(native.trigger(checkbox.commandId), 'native checkbox QAction missing');
await delay();
check(
  calls === 1 && !JSON.parse(native.state(checkbox.commandId)).enabled,
  'disabled action called'
);
checkbox.enabled = true;
checkbox.visible = false;
check(native.trigger(checkbox.commandId), 'native checkbox QAction missing');
await delay();
check(calls === 1, 'hidden action called');
checkbox.visible = true;
checkbox.label = 'Updated';
checkbox.checked = false;
check(JSON.parse(native.state(checkbox.commandId)).label === 'Updated', 'native property update');
native.attachWindow();
check(native.trigger(checkbox.commandId), 'native checkbox QAction missing');
await delay();
check(calls === 2 && checkbox.checked, 'new window/native tree');
check(native.trigger(quit.commandId), 'native quit QAction missing');
// Wait for the actual QAction -> native queue -> LibJS role dispatch, rather than
// assuming that one timer turn is enough on a loaded release builder.
const quitDeadline = Date.now() + 2000;
while (quits === 0 && Date.now() < quitDeadline) {
  await delay();
}
if (quits !== 1) {
  console.error('native quit diagnostics', native.state(quit.commandId));
}
check(quits === 1, 'native quit role dispatch');
// A queued native selection must not call a replacement application menu.
check(native.trigger(checkbox.commandId), 'native checkbox QAction missing');
Menu.setApplicationMenu(Menu.buildFromTemplate([{ label: 'Replacement', submenu: [] }]));
await delay();
check(calls === 2, 'stale generation called');
Menu.setApplicationMenu(null);
check(!JSON.parse(native.state()).bar, 'native menu removal');
console.log(
  'PASS actual Qt application menu/checkbox/state, queued LibJS callbacks, roles and stale-generation teardown'
);
