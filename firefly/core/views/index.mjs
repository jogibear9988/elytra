/** Native child-view composition. Contents IDs belong to documents, never hidden windows. */
export default function createViews(
  native,
  EventEmitter,
  makeContents,
  validatePreferences,
  requireReady,
  error,
  unsupported
) {
  const records = new Map();
  const endpoints = new WeakMap();
  const contentsOwners = new WeakMap();
  const roots = new WeakMap();
  let stopped = false;

  function alive(host) {
    if (stopped || host._destroyed) {
      throw error('ERR_FIREFLY_OBJECT_DESTROYED', 'View has been destroyed');
    }
  }

  function owned(view) {
    const host = endpoints.get(view);
    if (!host) {
      throw error('ERR_INVALID_THIS', 'An owned WebContentsView is required');
    }
    alive(host);
    return host;
  }

  function windowAlive(win) {
    if (stopped || win._destroyed || win._closing) {
      throw error('ERR_FIREFLY_OBJECT_DESTROYED', 'Parent window has been destroyed');
    }
  }

  /** Own data properties avoid calling application getters during native mutation. */
  function shape(value, allowed, name) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', `${name} must be an object`);
    }
    for (const key of Reflect.ownKeys(value)) {
      if (!allowed.includes(key)) {
        unsupported(`${name} option ${String(key)}`);
      }
      const descriptor = Object.getOwnPropertyDescriptor(value, key);
      if (!descriptor || !Object.hasOwn(descriptor, 'value')) {
        throw error('ERR_INVALID_ARG_TYPE', `${name} requires own data properties`);
      }
    }
  }

  function rectangle(value) {
    const keys = ['x', 'y', 'width', 'height'];
    shape(value, keys, 'View bounds');
    const result = {};
    for (const key of keys) {
      const item = Object.getOwnPropertyDescriptor(value, key)?.value;
      if (typeof item !== 'number' || !Number.isFinite(item)) {
        throw error('ERR_INVALID_ARG_TYPE', `View bounds ${key} must be finite`);
      }
      result[key] = Math.round(key === 'width' || key === 'height' ? Math.max(0, item) : item);
    }
    if (
      result.width > 16384 ||
      result.height > 16384 ||
      result.x < -2147483648 ||
      result.y < -2147483648 ||
      result.x > 2147483647 ||
      result.y > 2147483647 ||
      result.x + result.width > 2147483647 ||
      result.y + result.height > 2147483647
    ) {
      throw error('ERR_INVALID_ARG_VALUE', 'View bounds exceed native coordinate limits');
    }
    return result;
  }

  /** Update JS ownership only after native attachment has accepted the operation. */
  function removeOwner(host) {
    if (!host.owner) {
      return;
    }
    const parent = roots.get(host.owner);
    if (parent) {
      const index = parent.children.indexOf(host.view);
      if (index !== -1) {
        parent.children.splice(index, 1);
      }
    }
    host.owner = null;
  }

  class WebContentsView extends EventEmitter {
    constructor(options = {}) {
      super();
      if (new.target !== WebContentsView) {
        unsupported('WebContentsView subclassing');
      }
      requireReady();
      if (stopped) {
        throw error('ERR_FIREFLY_APP_EXITED', 'Application has exited');
      }
      shape(options, ['webPreferences'], 'WebContentsView');
      const prefs = validatePreferences(options.webPreferences ?? {});
      const host = new EventEmitter();
      // Preference/emitter hooks can reenter application shutdown before allocation.
      requireReady();
      if (stopped) {
        throw error('ERR_FIREFLY_APP_EXITED', 'Application has exited');
      }
      Object.defineProperty(host, 'id', {
        value: native.createView(prefs.preload, prefs.additionalArguments),
        enumerable: true
      });
      host._destroyed = false;
      host._closing = false;
      host.owner = null;
      host.view = this;

      host.isDestroyed = () => host._destroyed;

      host.destroy = () => {
        if (stopped || host._destroyed || host._closing) {
          return;
        }
        native.closeContents(host.id);
        if (!host._destroyed) {
          host._closing = true;
        }
      };

      host._closeRequest = () => {
        host.destroy();
        return true;
      };

      try {
        host.webContents = makeContents(host);
      } catch (failure) {
        // Failed wrapper construction must not leave an unreachable native document.
        native.closeContents(host.id);
        throw failure;
      }
      records.set(host.id, host);
      endpoints.set(this, host);
      contentsOwners.set(host.webContents, host);
      Object.defineProperty(this, 'webContents', {
        enumerable: true,

        get: () => (host._destroyed ? undefined : host.webContents)
      });
    }

    setBounds(bounds, options) {
      const host = owned(this);
      if (options !== undefined) {
        unsupported('View bounds animation/options');
      }
      const value = rectangle(bounds);
      native.setViewBounds(host.id, value.x, value.y, value.width, value.height);
    }

    getBounds() {
      const host = owned(this);
      const raw = native.viewBounds(host.id);
      const value = typeof raw === 'string' ? JSON.parse(raw) : raw;
      return { x: value[0], y: value[1], width: value[2], height: value[3] };
    }

    setVisible(value) {
      const host = owned(this);
      if (typeof value !== 'boolean') {
        throw error('ERR_INVALID_ARG_TYPE', 'View visibility must be boolean');
      }
      native.setViewVisible(host.id, value);
    }

    getVisible() {
      return native.viewVisible(owned(this).id);
    }

    setBackgroundColor() {
      owned(this);
      unsupported('View background color');
    }

    setBorderRadius() {
      owned(this);
      unsupported('View border radius');
    }

    addChildView() {
      owned(this);
      unsupported('Nested child views');
    }

    removeChildView() {
      owned(this);
      unsupported('Nested child views');
    }

    get children() {
      owned(this);
      return [];
    }
  }

  /** A root composes native child views alongside the window's internal main view. */
  function contentView(win) {
    const existing = roots.get(win);
    if (existing) {
      return existing.root;
    }
    const state = { root: new EventEmitter(), children: [] };
    const root = state.root;
    Object.defineProperty(root, 'children', {
      enumerable: true,

      get() {
        windowAlive(win);
        if (typeof native.childViewIds === 'function') {
          const ids = JSON.parse(native.childViewIds(win.id));
          return ids.map((id) => {
            const child = records.get(id);
            if (!child || child._destroyed || child.owner !== win) {
              throw error('ERR_FIREFLY_VIEW_STATE', 'Native child ownership is inconsistent');
            }
            return child.view;
          });
        }
        // Metadata projection for non-GUI policy hosts only.
        return state.children.slice();
      }
    });

    root.addChildView = (view, index) => {
      windowAlive(win);
      const host = owned(view);
      if (host._closing) {
        throw error('ERR_FIREFLY_OBJECT_DESTROYED', 'Child contents are closing');
      }
      if (index !== undefined && (!Number.isInteger(index) || index < 0)) {
        throw error('ERR_INVALID_ARG_VALUE', 'Child index must be a nonnegative integer');
      }
      const count = state.children.length - (host.owner === win ? 1 : 0);
      const position = index === undefined ? count : Math.min(index, count);
      native.attachView(win.id, host.id, position);
      removeOwner(host);
      state.children.splice(position, 0, view);
      host.owner = win;
    };

    root.removeChildView = (view) => {
      windowAlive(win);
      const host = owned(view);
      if (host.owner !== win) {
        return;
      }
      native.detachView(win.id, host.id);
      removeOwner(host);
    };

    for (const method of ['addChildView', 'removeChildView']) {
      Object.defineProperty(root, method, { value: root[method], enumerable: true });
    }
    for (const method of [
      'setBounds',
      'getBounds',
      'setVisible',
      'getVisible',
      'setBackgroundColor',
      'setBorderRadius'
    ]) {
      root[method] = () => {
        windowAlive(win);
        unsupported(`Root contentView.${method}`);
      };
    }
    roots.set(win, state);
    return root;
  }

  function ownerOf(contents) {
    const host = contentsOwners.get(contents);
    const win = host?.owner;
    return !stopped &&
      host &&
      !host._destroyed &&
      !host._closing &&
      win &&
      !win._destroyed &&
      !win._closing
      ? win
      : null;
  }

  function detachWindow(win) {
    const state = roots.get(win);
    if (!state) {
      return;
    }
    for (const view of state.children) {
      const host = endpoints.get(view);
      if (host?.owner === win) {
        host.owner = null;
      }
    }
    state.children.length = 0;
  }

  function destroyed(id) {
    const host = records.get(id);
    if (!host || host._destroyed) {
      return;
    }
    host._destroyed = true;
    host._closing = false;
    removeOwner(host);
    records.delete(id);
    host.webContents.emit('destroyed');
    host.emit('closed');
  }

  function shutdown() {
    stopped = true;
    for (const host of records.values()) {
      host._destroyed = true;
      host._closing = false;
      removeOwner(host);
    }
    records.clear();
  }

  return { WebContentsView, records, contentView, ownerOf, detachWindow, destroyed, shutdown };
}
