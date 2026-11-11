/** Pure JS menu graph/transaction policy, run under Node and Mite. */
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

function fails(action, code) {
  try {
    action();
  } catch (e) {
    check(e.code === code, String(e));
    return;
  }
  throw Error(`Expected ${code}`);
}

let wire;
let refused = false;
let quit = 0;
let close = 0;
const { Menu, MenuItem, dispatch } = createMenus(
  {
    menuApply(value) {
      if (refused) {
        throw Error('native');
      }
      wire = JSON.parse(value);
    }
  },
  error,
  unsupported,
  () => ++quit,
  (id) =>
    id === 7
      ? {
          close() {
            ++close;
          }
        }
      : undefined
);

const acceleratorItem = new MenuItem({ accelerator: 'Ctrl+X', userSettingsLabel: 'ctrl+x' });
check(
  acceleratorItem.accelerator === 'Ctrl+X' && acceleratorItem.userSettingsLabel === 'ctrl+x',
  'Accelerator and JS-only keybinding metadata'
);

fails(() => new MenuItem({ accelerator: 'AltGr+X' }), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => new MenuItem({ accelerator: 'Ctrl+X+Y' }), 'ERR_INVALID_ARG_VALUE');

fails(() => new MenuItem({ role: 'copy' }), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => new MenuItem({ type: 'radio' }), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => new MenuItem({ checked: false }), 'ERR_FIREFLY_UNSUPPORTED_API');

const cycle = [];
cycle.push({ label: 'cycle', submenu: cycle });

fails(() => Menu.buildFromTemplate(cycle), 'ERR_INVALID_ARG_VALUE');

const reusable = new MenuItem({ id: 'one', label: 'One', accelerator: 'Ctrl+X' });

fails(() => Menu.buildFromTemplate([reusable, { id: 'one' }]), 'ERR_INVALID_ARG_VALUE');

const child = Menu.buildFromTemplate([reusable]);
const menu = Menu.buildFromTemplate([
  { label: 'Root', submenu: child },
  { label: 'Other', submenu: [] }
]);
Menu.setApplicationMenu(menu);
const generation = wire.generation;
check(
  wire.items[0].submenu[0].accelerator.key === 'X' &&
    wire.items[0].submenu[0].accelerator.modifiers === 2,
  'Native canonical shortcut descriptor'
);
check(menu.getMenuItemById('one') === reusable, 'recursive lookup');

fails(() => child.append(new MenuItem({ label: 'late' })), 'ERR_FIREFLY_UNSUPPORTED_API');

refused = true;
try {
  reusable.label = 'failed';
} catch {}
check(reusable.label === 'One' && Menu.getApplicationMenu() === menu, 'native failure rollback');
refused = false;
reusable.enabled = false;
check(!wire.items[0].submenu[0].enabled, 'property publication');
let calls = 0;

reusable.click = () => ++calls;

dispatch(0, {
  generation: wire.generation,
  command: reusable.commandId,
  checked: false,
  modifiers: 0
});
check(!calls, 'disabled dispatch');
reusable.enabled = true;
dispatch(0, { generation, command: reusable.commandId, checked: false, modifiers: 0 });
check(!calls, 'stale generation');
dispatch(0, {
  generation: wire.generation,
  command: reusable.commandId,
  checked: false,
  modifiers: 0
});
check(calls === 1, 'live callback');
Menu.setApplicationMenu(null);
check(Menu.getApplicationMenu() === null && wire.items === null, 'null install');
const roles = Menu.buildFromTemplate([
  {
    label: 'Roles',
    submenu: [
      { id: 'quit', role: 'quit' },
      { id: 'close', role: 'close' }
    ]
  }
]);
Menu.setApplicationMenu(roles);
for (const id of ['quit', 'close']) {
  dispatch(7, {
    generation: wire.generation,
    command: roles.getMenuItemById(id).commandId,
    checked: false,
    modifiers: 0
  });
}
check(quit === 1 && close === 1, 'role routing');
console.log(
  'PASS menu graph validation, cycles/ids, install rollback, live properties, roles and stale events'
);
