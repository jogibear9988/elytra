/** Main-owned application-menu graph; native actions carry IDs, never JS callbacks. */
import createAccelerators from './accelerator.mjs';

export default function createMenus(native, error, unsupported, quit, resolveWindow) {
  const accelerator = createAccelerators(error, unsupported, process.platform);
  const editRoles = new Set([
    'cut',
    'copy',
    'paste',
    'pasteAndMatchStyle',
    'undo',
    'redo',
    'selectAll'
  ]);
  const nativeRoles = new Set(['hide', 'hideOthers', 'unhide', 'front']);
  const menuRoles = new Set(['window', 'help', 'services']);
  const menuState = new WeakMap();
  const itemState = new WeakMap();
  let current = null;
  const building = new Set();
  let generation = 0;
  let nextCommand = 0;
  let commands = new Map();
  const options = new Set([
    'id',
    'label',
    'type',
    'submenu',
    'enabled',
    'visible',
    'checked',
    'click',
    'role',
    'accelerator',
    'userSettingsLabel'
  ]);

  function text(value, name) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', `${name} must be a string`);
    }
    if (value.length > 1024 || value.includes('\0')) {
      throw error('ERR_OUT_OF_RANGE', `${name} exceeds menu text budget`);
    }
    return value;
  }

  function boolean(value, name) {
    if (typeof value !== 'boolean') {
      throw error('ERR_INVALID_ARG_TYPE', `${name} must be boolean`);
    }
    return value;
  }

  function graph(menu) {
    const seen = new Set();
    const ids = new Set();
    const roles = new Set();
    const map = new Map();

    function walk(value, depth, enabled = true, visible = true) {
      if (depth > 8 || seen.has(value)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Menu cycle, shared submenu or excessive depth');
      }
      seen.add(value);
      const state = menuState.get(value);
      if (!state) {
        throw error('ERR_INVALID_ARG_TYPE', 'submenu must be a Menu');
      }
      return state.items.map((item) => {
        const data = itemState.get(item);
        if (map.has(data.command)) {
          throw error('ERR_INVALID_ARG_VALUE', 'Repeated MenuItem');
        }
        if (map.size >= 256) {
          throw error('ERR_OUT_OF_RANGE', 'At most 256 menu items');
        }
        if (data.id && ids.has(data.id)) {
          throw error('ERR_INVALID_ARG_VALUE', 'Duplicate menu item id');
        }
        if (data.id) {
          ids.add(data.id);
        }
        if (menuRoles.has(data.role)) {
          if (roles.has(data.role)) {
            throw error('ERR_INVALID_ARG_VALUE', 'Repeated native menu role');
          }
          roles.add(data.role);
        }
        map.set(data.command, {
          item,
          eligible: enabled && visible && data.enabled && data.visible
        });
        return {
          command: data.command,
          label: data.label,
          type: data.type,
          enabled: data.enabled,
          visible: data.visible,
          checked: data.checked,
          ...(data.accelerator == null ? {} : { accelerator: accelerator(data.accelerator) }),
          ...(menuRoles.has(data.role) ? { role: data.role } : {}),
          submenu: data.submenu
            ? walk(data.submenu, depth + 1, enabled && data.enabled, visible && data.visible)
            : []
        };
      });
    }

    return { items: walk(menu, 1), map, menus: seen };
  }

  function install(menu) {
    if (generation === 2147483647) {
      throw error('ERR_OUT_OF_RANGE', 'Menu generation exhausted');
    }
    const snapshot =
      menu === null ? { items: null, map: new Map(), menus: new Set() } : graph(menu);
    const wire = JSON.stringify({ schema: 1, generation: generation + 1, items: snapshot.items });
    if (wire.length > 64 * 1024) {
      throw error('ERR_OUT_OF_RANGE', 'Menu wire budget exceeded');
    }
    native.menuApply(wire);
    ++generation;
    current = menu;
    commands = snapshot.map;
  }

  function update(item, key, value) {
    const data = itemState.get(item);
    const previous = data[key];
    data[key] = value;
    try {
      if (current && commands.has(data.command)) {
        install(current);
      }
    } catch (failure) {
      data[key] = previous;
      throw failure;
    }
  }

  class MenuItem {
    constructor(value) {
      if (value === null || typeof value !== 'object' || Array.isArray(value)) {
        throw error('ERR_INVALID_ARG_TYPE', 'MenuItem options must be an object');
      }
      for (const key of Reflect.ownKeys(value)) {
        if (!options.has(key)) {
          unsupported(`MenuItem option ${String(key)}`);
        }
      }
      const role = value.role;
      if (role !== undefined && typeof role !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'MenuItem role must be a string');
      }
      if (
        role !== undefined &&
        !['quit', 'close', 'minimize', 'zoom'].includes(role) &&
        !nativeRoles.has(role) &&
        !menuRoles.has(role) &&
        !(editRoles.has(role) && typeof native.editContents === 'function')
      ) {
        unsupported(`MenuItem role ${role}`);
      }
      if (
        (nativeRoles.has(role) || menuRoles.has(role)) &&
        (typeof native.menuRoleAvailable !== 'function' || !native.menuRoleAvailable(role))
      ) {
        unsupported(`MenuItem native role ${role}`);
      }
      const acceleratorValue = value.accelerator;
      accelerator(acceleratorValue);
      const submenu =
        value.submenu === undefined
          ? null
          : Array.isArray(value.submenu)
            ? Menu.buildFromTemplate(value.submenu)
            : value.submenu;
      if (submenu !== null && !menuState.has(submenu)) {
        throw error('ERR_INVALID_ARG_TYPE', 'submenu must be a Menu or template');
      }
      const type = value.type === undefined ? (submenu ? 'submenu' : 'normal') : value.type;
      if (!['normal', 'separator', 'checkbox', 'submenu'].includes(type)) {
        unsupported(`MenuItem type ${type}`);
      }
      if ((type === 'submenu') !== (submenu !== null)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Submenu type/content mismatch');
      }
      if (value.click !== undefined && typeof value.click !== 'function') {
        throw error('ERR_INVALID_ARG_TYPE', 'click must be callable');
      }
      if (
        (type === 'separator' || type === 'submenu') &&
        (value.click !== undefined || (role !== undefined && !menuRoles.has(role)))
      ) {
        unsupported('click/role on a separator or submenu');
      }
      if (role !== undefined && type !== 'normal' && !(type === 'submenu' && menuRoles.has(role))) {
        unsupported('role on non-normal item');
      }
      if (menuRoles.has(role) && type !== 'submenu') {
        unsupported('menu role requires a submenu');
      }
      if (acceleratorValue != null && (type === 'submenu' || type === 'separator')) {
        unsupported('accelerator on a separator or submenu');
      }
      if (value.checked !== undefined && type !== 'checkbox') {
        unsupported('checked on a non-checkbox item');
      }
      if (nextCommand === 2147483647) {
        throw error('ERR_OUT_OF_RANGE', 'Menu command ids exhausted');
      }
      const data = {
        command: ++nextCommand,
        parent: null,
        role,
        accelerator: acceleratorValue,
        userSettingsLabel:
          value.userSettingsLabel === undefined
            ? undefined
            : text(value.userSettingsLabel, 'userSettingsLabel'),
        submenu,
        type,
        id: text(value.id === undefined ? '' : value.id, 'id'),
        label: text(
          value.label === undefined
            ? role === 'quit'
              ? 'Quit'
              : role === 'close'
                ? 'Close'
                : ''
            : value.label,
          'label'
        ),
        enabled: value.enabled === undefined ? true : boolean(value.enabled, 'enabled'),
        visible: value.visible === undefined ? true : boolean(value.visible, 'visible'),
        checked: value.checked === undefined ? false : boolean(value.checked, 'checked'),
        click: value.click
      };
      itemState.set(this, data);
      for (const key of ['type', 'id', 'role', 'submenu']) {
        Object.defineProperty(this, key, { enumerable: true, get: () => data[key] });
      }
      Object.defineProperty(this, 'commandId', { enumerable: true, get: () => data.command });
      for (const key of ['label', 'enabled', 'visible', 'checked']) {
        Object.defineProperty(this, key, {
          enumerable: true,

          get: () => data[key],

          set: (value) => {
            if (key === 'checked' && type !== 'checkbox') {
              unsupported('checked on a non-checkbox item');
            }
            update(this, key, key === 'label' ? text(value, key) : boolean(value, key));
          }
        });
      }
      Object.defineProperty(this, 'accelerator', {
        enumerable: true,

        get: () => data.accelerator,

        set: (value) => {
          accelerator(value);
          if (value != null && (type === 'submenu' || type === 'separator')) {
            unsupported('accelerator on a separator or submenu');
          }
          update(this, 'accelerator', value);
        }
      });
      Object.defineProperty(this, 'userSettingsLabel', {
        enumerable: true,

        get: () => data.userSettingsLabel,

        set: (value) => {
          data.userSettingsLabel =
            value === undefined ? undefined : text(value, 'userSettingsLabel');
        }
      });
      Object.defineProperty(this, 'click', {
        enumerable: true,

        get: () => data.click,

        set: (value) => {
          if (typeof value !== 'function' && value !== undefined) {
            throw error('ERR_INVALID_ARG_TYPE', 'click must be callable');
          }
          if (value && type !== 'normal' && type !== 'checkbox') {
            unsupported('click on a separator/submenu');
          }
          data.click = value;
        }
      });
    }
  }
  class Menu {
    constructor() {
      menuState.set(this, { items: [] });
    }

    static buildFromTemplate(template) {
      if (!Array.isArray(template)) {
        throw error('ERR_INVALID_ARG_TYPE', 'Menu template must be an array');
      }
      if (building.has(template) || building.size >= 8) {
        throw error('ERR_INVALID_ARG_VALUE', 'Recursive or excessive template depth');
      }
      building.add(template);
      try {
        const menu = new Menu();
        const list = menuState.get(menu).items;
        for (const value of template) {
          const item = itemState.has(value) ? value : new MenuItem(value);
          if (itemState.get(item).parent) {
            unsupported('sharing MenuItem between menus');
          }
          list.push(item);
        }
        graph(menu);
        for (const item of list) {
          itemState.get(item).parent = menu;
        }
        return menu;
      } finally {
        building.delete(template);
      }
    }

    static setApplicationMenu(menu) {
      if (menu !== null && !menuState.has(menu)) {
        throw error('ERR_INVALID_ARG_TYPE', 'Expected Menu or null');
      }
      install(menu);
    }

    static getApplicationMenu() {
      return current;
    }

    get items() {
      return Object.freeze([...menuState.get(this).items]);
    }

    append(item) {
      this.insert(menuState.get(this).items.length, item);
    }

    insert(position, item) {
      if (!itemState.has(item)) {
        throw error('ERR_INVALID_ARG_TYPE', 'Expected MenuItem');
      }
      const data = itemState.get(item);
      const list = menuState.get(this).items;
      if (!Number.isInteger(position) || position < 0 || position > list.length) {
        throw error('ERR_OUT_OF_RANGE', 'Invalid menu insertion index');
      }
      if (current && graph(current).menus.has(this)) {
        unsupported('structural changes to an installed application menu');
      }
      if (data.parent) {
        unsupported('sharing MenuItem between menus');
      }
      list.splice(position, 0, item);
      try {
        graph(this);
      } catch (failure) {
        list.splice(position, 1);
        throw failure;
      }
      data.parent = this;
    }

    getMenuItemById(id) {
      text(id, 'id');
      for (const { item } of graph(this).map.values()) {
        if (item.id && item.id === id) {
          return item;
        }
      }
      return null;
    }
  }
  return {
    Menu,
    MenuItem,

    dispatch(windowId, event) {
      if (event.generation !== generation) {
        return;
      }
      const found = commands.get(event.command);
      if (!found?.eligible) {
        return;
      }
      const item = found.item;
      const data = itemState.get(item);
      if (data.type !== 'normal' && data.type !== 'checkbox') {
        return;
      }
      if (data.type === 'checkbox') {
        if (typeof event.checked !== 'boolean') {
          return;
        }
        data.checked = event.checked;
      }
      const window = resolveWindow(windowId);
      if (data.role === 'quit') {
        quit();
      } else if (data.role === 'close') {
        window?.close();
      } else if (data.role === 'minimize') {
        window?.minimize();
      } else if (data.role === 'zoom') {
        if (window) {
          if (window.isMaximized()) {
            window.unmaximize();
          } else {
            window.maximize();
          }
        }
      } else if (nativeRoles.has(data.role)) {
        native.menuRole(data.role);
      } else if (editRoles.has(data.role)) {
        native.editContents(0, data.role);
      } else if (data.click) {
        Reflect.apply(data.click, undefined, [
          item,
          window,
          {
            shiftKey: !!(event.modifiers & 1),
            ctrlKey: !!(event.modifiers & 2),
            altKey: !!(event.modifiers & 4),
            metaKey: !!(event.modifiers & 8),
            triggeredByAccelerator: event.triggeredByAccelerator === true
          }
        ]);
      }
    }
  };
}
