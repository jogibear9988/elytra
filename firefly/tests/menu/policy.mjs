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
// Controlled transport only: application and Dock generations must never share dispatch maps.
let dockWire;
let appWire;
let dockRefused = false;
let dockCalls = 0;
let appCalls = 0;
const separate = createMenus(
  {
    menuApply(value) {
      appWire = JSON.parse(value);
    },

    dockMenuAvailable() {
      return true;
    },

    dockMenuApply(value) {
      if (dockRefused) {
        throw Error('native Dock refused');
      }
      dockWire = JSON.parse(value);
    }
  },
  error,
  unsupported,
  () => {},
  () => undefined
);
const dockItem = new separate.MenuItem({ label: 'New Window', click: () => dockCalls++ });
const dockMenu = separate.Menu.buildFromTemplate([dockItem]);
check(separate.dock.getMenu() === null, 'Initial Dock menu');
separate.dock.setMenu(dockMenu);
const oldDockGeneration = dockWire.generation;
const appItem = new separate.MenuItem({ label: 'Application action', click: () => appCalls++ });
const appMenu = separate.Menu.buildFromTemplate([{ label: 'Root', submenu: [appItem] }]);
separate.Menu.setApplicationMenu(appMenu);
separate.dispatch(0, { generation: appWire.generation, command: dockItem.commandId, modifiers: 0 });
check(!dockCalls && !appCalls, 'Dock ID cannot route through application generation');
separate.dispatch(
  0,
  { generation: dockWire.generation, command: dockItem.commandId, modifiers: 0 },
  true
);
check(dockCalls === 1 && !appCalls, 'Dock callback uses its own map');
dockRefused = true;
try {
  dockItem.label = 'refused';
} catch {}
check(
  dockItem.label === 'New Window' && separate.dock.getMenu() === dockMenu,
  'Dock install failure rollback'
);
dockRefused = false;
dockItem.enabled = false;
separate.dispatch(
  0,
  { generation: oldDockGeneration, command: dockItem.commandId, modifiers: 0 },
  true
);
check(dockCalls === 1, 'Stale Dock generation denied');

fails(() => separate.Menu.setApplicationMenu(dockMenu), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => separate.dock.setMenu(appMenu), 'ERR_FIREFLY_UNSUPPORTED_API');

fails(() => separate.dock.bounce(), 'ERR_FIREFLY_UNSUPPORTED_API');

// Full desktop applications publish hundreds of commands. The former 256-item
// and 64KiB transport ceilings both rejected an otherwise valid native tree.
let largeWire;
let largeCalls = 0;
let nativeInstalls = 0;
const large = createMenus(
  {
    menuApply(wire) {
      ++nativeInstalls;
      largeWire = wire;
    }
  },
  error,
  unsupported,
  () => {},
  () => undefined
);
const children = Array.from({ length: 2047 }, (_, index) => ({
  label: `Command ${index}`,
  id: `large-${index}`,

  click() {
    ++largeCalls;
  }
}));
const largeMenu = large.Menu.buildFromTemplate([{ label: 'Commands', submenu: children }]);
large.Menu.setApplicationMenu(largeMenu);
check(
  new TextEncoder().encode(largeWire).byteLength > 65536,
  'large menu genuinely exceeds former wire ceiling'
);
const acceptedWire = JSON.parse(largeWire);
large.dispatch(0, {
  generation: acceptedWire.generation,
  command: largeMenu.getMenuItemById('large-2046').commandId,
  modifiers: 0
});
check(largeCalls === 1, 'last command remains addressable beyond former ceiling');
const previousWire = largeWire;

fails(
  () =>
    large.Menu.setApplicationMenu(
      large.Menu.buildFromTemplate([
        { label: 'Too many', submenu: [...children, { label: 'Excess' }] }
      ])
    ),
  'ERR_OUT_OF_RANGE'
);

check(
  nativeInstalls === 1 &&
    largeWire === previousWire &&
    large.Menu.getApplicationMenu() === largeMenu,
  'item limit rejects before native mutation'
);

fails(
  () =>
    large.Menu.setApplicationMenu(
      large.Menu.buildFromTemplate([
        {
          label: 'UTF8 bytes',
          submenu: Array.from({ length: 1200 }, () => ({ label: '界'.repeat(300) }))
        }
      ])
    ),
  'ERR_OUT_OF_RANGE'
);

check(
  nativeInstalls === 1 && large.Menu.getApplicationMenu() === largeMenu,
  'UTF8 wire limit rejects before native mutation'
);

console.log(
  'PASS menu graph validation, cycles/ids, install rollback, live properties, roles and stale events'
);
