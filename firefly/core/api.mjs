/** Main-process desktop API and native-window lifecycle policy. */
import createMenus from './menu/index.mjs';
import createTracing from './tracing/index.mjs';
import createCommandLine from './app/command-line.mjs';
import createCrashReporter from './crash/index.mjs';
import createProtocol from './protocol/index.mjs';
import createPermissions from './session/permissions.mjs';
import createWebRequest from './session/web-request.mjs';
import createMainIpc from './ipc-main.mjs';
import createFrames from './frames/index.mjs';
import createScreen from './screen/index.mjs';
import createClipboard from './clipboard/index.mjs';
import createTheme from './theme/index.mjs';
import createWindowGeometry from './window/geometry.mjs';
import createBackground from './window/background.mjs';
import createWindowInput from './window/input.mjs';
import createMessageChannels from './message-ports/index.mjs';
import createUtilityProcesses from './utility-process/index.mjs';
import createSafeStorage from './safe-storage/index.mjs';
import createViews from './views/index.mjs';
import createWebPreferences from './window/preferences.mjs';
import createNativeImages from './native-image/index.mjs';
import createDesktopCapture from './desktop-capture/index.mjs';
import createNotifications from './notifications/index.mjs';
import createPowerMonitor from './power-monitor/index.mjs';
import createPowerSaveBlocker from './power-save-blocker/index.mjs';
import createShell from './shell/index.mjs';
import createSystemPreferences from './system-preferences/index.mjs';
import createNet from './net/index.mjs';
import createGlobalShortcuts from './global-shortcut/index.mjs';
import createDialogs from './dialog/index.mjs';
import createGPU from './app/gpu.mjs';

export default function createFireflyApi(
  native,
  EventEmitter,
  paths,
  urls,
  files,
  streams,
  transport
) {
  const error = (code, message) => Object.assign(new Error(message), { code });

  const unsupported = (name) => {
    throw error('ERR_FIREFLY_UNSUPPORTED_API', `${name} is not implemented`);
  };

  const channels = createMessageChannels(native, EventEmitter, error);
  const background = createBackground(native, error, unsupported);
  const windowInput = createWindowInput(native, error, unsupported);
  const windows = new Map();
  const allContents = new Map();
  const frames = createFrames(
    native,
    (id) => {
      const contents = allContents.get(id);
      return !!contents && !contents.isDestroyed();
    },
    error,
    unsupported,
    () => allContents.keys()
  );
  const contentsToken = Symbol('WebContents constructor');
  let views;
  let ready = false;
  let quitting = false;
  let exited = false;
  let sandboxRequired = false;
  const preferences = createWebPreferences(paths, () => sandboxRequired, error, unsupported);
  let resolveReady;
  const readyPromise = new Promise((resolve) => {
    resolveReady = resolve;
  });

  const event = () => ({
    defaultPrevented: false,

    preventDefault() {
      this.defaultPrevented = true;
    }
  });

  const gpu = createGPU(
    native,
    () => ready,
    () => exited,
    error,
    unsupported
  );

  class App extends EventEmitter {
    getGPUFeatureStatus() {
      return gpu.getFeatureStatus();
    }

    disableHardwareAcceleration() {
      return gpu.disableHardwareAcceleration();
    }

    getGPUInfo(type) {
      return gpu.getInfo(type);
    }

    enableSandbox() {
      if (exited) {
        throw error('ERR_FIREFLY_APP_EXITED', 'Application has exited');
      }
      if (ready) {
        throw error(
          'ERR_FIREFLY_SANDBOX_TOO_LATE',
          'enableSandbox must be called before readiness'
        );
      }
      native.enableSandbox();
      sandboxRequired = true;
    }

    getPreferredSystemLanguages() {
      return native.appLocaleRead(0);
    }

    getSystemLocale() {
      if (!ready) {
        throw error('ERR_FIREFLY_NOT_READY', 'getSystemLocale requires app readiness');
      }
      return native.appLocaleRead(1);
    }

    getLocale() {
      if (!ready) {
        throw error('ERR_FIREFLY_NOT_READY', 'getLocale requires app readiness');
      }
      return native.appLocaleRead(2);
    }

    getLocaleCountryCode() {
      return native.appLocaleRead(3);
    }

    getPath(name) {
      if (typeof name !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Path name must be a string');
      }
      return name === 'crashDumps' ? native.crashDirectory() : native.appPathGet(name);
    }

    setPath(name, value) {
      if (typeof name !== 'string' || typeof value !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Path name/value must be strings');
      }
      if (name === 'crashDumps') {
        native.crashDirectory(value);
      } else {
        native.appPathSet(name, value);
      }
    }

    getAppPath() {
      return native.appPath;
    }

    setAppLogsPath(value) {
      if (value !== undefined && typeof value !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'Log path must be a string');
      }
      native.appLogsSet(value);
    }

    isReady() {
      return ready;
    }

    whenReady() {
      return readyPromise;
    }

    quit() {
      if (quitting || exited) {
        return;
      }
      // Mark the whole attempt, including user listeners, as in flight.
      quitting = true;
      const e = event();
      this.emit('before-quit', e);
      if (exited) {
        return;
      }
      if (e.defaultPrevented) {
        quitting = false;
        return;
      }
      for (const win of [...windows.values()]) {
        if (!win._closeRequest()) {
          quitting = false;
          break;
        }
      }
      if (quitting && windows.size === 0) {
        finishQuit();
      }
    }

    exit(exitCode = 0) {
      if (typeof exitCode !== 'number' || !Number.isInteger(exitCode)) {
        throw error('ERR_INVALID_ARG_TYPE', 'exitCode must be an integer');
      }
      if (exitCode < -2147483648 || exitCode > 2147483647) {
        throw error('ERR_OUT_OF_RANGE', 'exitCode must fit a signed 32-bit integer');
      }
      if (exited) {
        return;
      }
      exited = true;
      quitting = true;
      systemPreferencesService.stop();
      network.stop();
      protocols.stop();
      permissions.stop();
      requests.stop();
      shortcuts.stop();
      dialogs.stop();
      desktopShell.stop();
      powerSave.stop();
      power.stop();
      notifications.stop();
      capture.stop();
      channels.shutdown();
      utilities.shutdown();
      views.shutdown();
      frames.stop();
      allContents.clear();
      try {
        for (const win of [...windows.values()]) {
          native.destroyNow(win.id);
          win._destroyed = true;
          win._closing = false;
          windows.delete(win.id);
          win.webContents._reject(error('ERR_ABORTED', 'Application exiting'));
          win.webContents.emit('destroyed');
          win.emit('closed');
        }
        app.emit('quit', event(), exitCode);
      } finally {
        // Request termination even if a listener throws. The current JS turn can finish.
        native.exit(exitCode);
      }
    }
  }
  const app = new App();
  const dialogs = createDialogs(
    native,
    () => ready,
    () => exited,
    (value) => {
      // Use map identity, never a user-supplied numeric id or prototype alone.
      for (const [id, window] of windows) {
        if (window === value) {
          if (window._destroyed || window._closing) {
            throw error('ERR_FIREFLY_OBJECT_DESTROYED', 'Dialog parent closed');
          }
          return id;
        }
      }
      if (value instanceof BrowserWindow) {
        throw error('ERR_FIREFLY_OBJECT_DESTROYED', 'Dialog parent closed');
      }
      return undefined;
    },
    error,
    unsupported
  );
  const shortcuts = createGlobalShortcuts(
    native,
    () => ready,
    () => exited,
    error,
    unsupported,
    process.platform
  );
  const network = createNet(
    native,
    () => ready,
    () => exited,
    error,
    unsupported,
    transport,
    () => protocols,
    () => requests
  );
  const systemPreferencesService = createSystemPreferences(
    native,
    () => exited,
    error,
    unsupported
  );
  const desktopShell = createShell(native, () => exited, error, unsupported);
  const powerSave = createPowerSaveBlocker(native, () => exited, error, unsupported);
  const power = createPowerMonitor(
    native,
    EventEmitter,
    () => ready,
    () => exited,
    error,
    unsupported
  );
  const notifications = createNotifications(
    native,
    EventEmitter,
    () => ready,
    () => exited,
    error,
    unsupported
  );
  const images = createNativeImages(error, unsupported);
  const capture = createDesktopCapture(
    native,
    () => ready,
    () => exited,
    error,
    unsupported,
    images.createEmpty
  );
  const utilities = createUtilityProcesses(
    native.utility,
    native,
    EventEmitter,
    streams?.Readable,
    app,
    paths
  );
  Object.defineProperty(app, 'commandLine', {
    enumerable: true,
    value: createCommandLine(native, error, unsupported)
  });
  const menus = createMenus(
    native,
    error,
    unsupported,
    () => app.quit(),
    (id) => {
      const window = windows.get(id);
      return window && !window._closing && !window._destroyed ? window : undefined;
    }
  );
  const { Menu, MenuItem } = menus;
  const contentTracing = createTracing(native, () => ready, files, paths, error, unsupported);
  const crashReporter = createCrashReporter(native, error, unsupported);
  const protocols = createProtocol(native, () => ready, error, unsupported, transport);
  const { protocol } = protocols;
  const permissions = createPermissions(
    native,
    () => ready,
    () => exited,
    (id) => allContents.get(id),
    error,
    unsupported
  );
  const requests = createWebRequest(
    native,
    () => ready,
    () => exited,
    (id) => allContents.get(id),
    frames.fromDocument,
    error,
    unsupported,
    transport?.abortHooks
  );
  const defaultSession = Object.freeze({ protocol, ...permissions.api, webRequest: requests.api });
  const session = {};
  Object.defineProperty(session, 'defaultSession', {
    enumerable: true,

    get() {
      if (!ready) {
        throw error('ERR_FIREFLY_NOT_READY', 'defaultSession requires app readiness');
      }
      return defaultSession;
    }
  });
  const screens = createScreen(native, EventEmitter, () => ready, error, unsupported);
  const { clipboard, ClipboardItem } = createClipboard(native, error, unsupported);
  const theme = createTheme(native, EventEmitter, error);
  const geometry = createWindowGeometry(native, EventEmitter, alive, dimension, error, unsupported);
  const {
    ipcMain,
    cancel: cancelIpc,
    dispatch: dispatchIpc,
    dispatchEvent: dispatchIpcEvent
  } = createMainIpc(native, EventEmitter, error, frames.fromDocument);

  function finishQuit() {
    const e = event();
    app.emit('will-quit', e);
    if (e.defaultPrevented) {
      quitting = false;
      return;
    }
    exited = true;
    systemPreferencesService.stop();
    network.stop();
    protocols.stop();
    permissions.stop();
    requests.stop();
    shortcuts.stop();
    dialogs.stop();
    desktopShell.stop();
    powerSave.stop();
    power.stop();
    notifications.stop();
    capture.stop();
    channels.shutdown();
    utilities.shutdown();
    views.shutdown();
    frames.stop();
    allContents.clear();
    app.emit('quit', event(), 0);
    native.quit();
  }

  function alive(win) {
    if (win._destroyed) {
      throw error('ERR_FIREFLY_OBJECT_DESTROYED', 'BrowserWindow has been destroyed');
    }
  }

  function dimension(value, name) {
    if (!Number.isInteger(value) || value < 1 || value > 16384) {
      throw error('ERR_INVALID_ARG_VALUE', `${name} must be an integer in 1..16384`);
    }
    return value;
  }

  class WebContents extends EventEmitter {
    constructor(win, token) {
      super();
      if (token !== contentsToken) {
        throw error('ERR_ILLEGAL_CONSTRUCTOR', 'WebContents is created by its native owner');
      }
      Object.defineProperty(this, 'id', { value: win.id, enumerable: true });
      this._window = win;
      this._url = '';
      this._title = '';
      this._pending = null;
      allContents.set(win.id, this);
    }

    get session() {
      alive(this._window);
      return defaultSession;
    }

    postMessage() {
      unsupported('webContents.postMessage port/data delivery');
    }

    send(channel, ...args) {
      alive(this._window);
      if (typeof channel !== 'string' || channel.length > 256 || args.length > 64) {
        throw error(
          'ERR_INVALID_ARG_TYPE',
          'send requires a bounded channel and at most 64 arguments'
        );
      }
      native.sendIpc(this.id, native.encodeIpc([channel, ...args]));
    }

    get mainFrame() {
      alive(this._window);
      return frames.main(this.id);
    }

    getURL() {
      alive(this._window);
      return this._url;
    }

    getTitle() {
      alive(this._window);
      return this._title;
    }

    getOSProcessId() {
      alive(this._window);
      return native.pid(this.id);
    }

    isDestroyed() {
      return this._window._destroyed;
    }

    getOwnerBrowserWindow() {
      return BrowserWindow.fromWebContents(this);
    }

    focus() {
      alive(this._window);
      native.focusContents(this.id);
    }

    isFocused() {
      alive(this._window);
      return native.focusedContents() === this.id;
    }

    close(options = {}) {
      if (this.isDestroyed() || this._window._closing) {
        return;
      }
      if (!options || typeof options !== 'object' || Array.isArray(options)) {
        throw error('ERR_INVALID_ARG_TYPE', 'webContents.close options must be an object');
      }
      for (const key of Object.keys(options)) {
        if (key !== 'waitForBeforeUnload' || options[key] !== false) {
          unsupported(`webContents.close ${key}`);
        }
      }
      this._window.destroy();
    }

    loadURL(url, options) {
      alive(this._window);
      if (options !== undefined && Object.keys(options).length) {
        unsupported('loadURL options');
      }
      if (typeof url !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'url must be a string');
      }
      let parsed;
      try {
        parsed = new URL(url);
      } catch {
        return Promise.reject(error('ERR_INVALID_URL', 'Invalid URL'));
      }
      if (
        !['file:', 'http:', 'https:'].includes(parsed.protocol) &&
        !protocols.canLoad(parsed.protocol.slice(0, -1))
      ) {
        return Promise.reject(
          error('ERR_FIREFLY_UNSUPPORTED_API', 'URL scheme is not implemented')
        );
      }
      this._reject(error('ERR_ABORTED', 'Navigation superseded'));
      return new Promise((resolve, reject) => {
        const timer = setTimeout(
          () => this._reject(error('ERR_TIMED_OUT', 'Navigation timed out')),
          30000
        );
        this._pending = { resolve, reject, timer };
        try {
          native.load(this.id, parsed.href);
        } catch (e) {
          this._reject(e);
        }
      });
    }

    loadFile(file, options) {
      if (typeof file !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'filePath must be a string');
      }
      if (options !== undefined && Object.keys(options).length) {
        unsupported('loadFile options');
      }
      return this.loadURL(urls.pathToFileURL(paths.resolve(native.appPath, file)).href);
    }

    _reject(e) {
      if (this._pending) {
        const p = this._pending;
        this._pending = null;
        clearTimeout(p.timer);
        p.reject(e);
      }
    }
  }
  class BrowserWindow extends geometry.WindowGeometry {
    constructor(options = {}) {
      super();
      if (exited) {
        throw error('ERR_FIREFLY_APP_EXITED', 'Application is exiting');
      }
      if (!ready) {
        throw error('ERR_FIREFLY_NOT_READY', 'Cannot create BrowserWindow before app is ready');
      }
      if (!options || typeof options !== 'object' || Array.isArray(options)) {
        throw error('ERR_INVALID_ARG_TYPE', 'BrowserWindow options must be an object');
      }
      for (const key of Object.keys(options)) {
        if (
          ![
            'width',
            'height',
            'minWidth',
            'minHeight',
            'maxWidth',
            'maxHeight',
            'x',
            'y',
            'useContentSize',
            'title',
            'show',
            'frame',
            'backgroundColor',
            'experimentalDarkMode',
            'acceptFirstMouse',
            'webPreferences'
          ].includes(key)
        ) {
          unsupported(`BrowserWindow option ${key}`);
        }
      }
      const prefs = preferences(options.webPreferences ?? {});
      const additionalArguments = prefs.additionalArguments;
      const width = dimension(options.width ?? 800, 'width');
      const height = dimension(options.height ?? 600, 'height');
      const title = options.title ?? 'Firefly';
      const show = options.show ?? true;
      const frame = options.frame ?? true;
      const useContentSize = options.useContentSize ?? false;
      const limits = geometry.initialConstraints(options);
      const constraintWire = JSON.stringify(limits);
      const firstMouse = windowInput(options);
      const initialBackground = options.backgroundColor;
      const color = background.parse(
        initialBackground === undefined ? '#FFFFFF' : initialBackground
      );
      const x = options.x === undefined ? undefined : geometry.coordinate(options.x, 'x');
      const y = options.y === undefined ? undefined : geometry.coordinate(options.y, 'y');
      if (
        (x !== undefined && x + width - 1 > 2147483647) ||
        (y !== undefined && y + height - 1 > 2147483647)
      ) {
        throw error(
          'ERR_OUT_OF_RANGE',
          'Initial window rectangle exceeds signed 32-bit coordinates'
        );
      }
      if (typeof useContentSize !== 'boolean') {
        throw error('ERR_INVALID_ARG_TYPE', 'useContentSize must be a boolean');
      }
      if (typeof title !== 'string' || typeof show !== 'boolean' || typeof frame !== 'boolean') {
        throw error('ERR_INVALID_ARG_TYPE', 'Invalid title/show option');
      }
      this._destroyed = false;
      this._closing = false;
      if (exited) {
        throw error('ERR_FIREFLY_APP_EXITED', 'Application exited during window preparation');
      }
      Object.defineProperty(this, 'id', {
        enumerable: true,
        value: native.create(
          width,
          height,
          title,
          show,
          frame,
          prefs.preload,
          useContentSize,
          x,
          y,
          additionalArguments,
          color,
          constraintWire,
          firstMouse
        )
      });
      this.webContents = new WebContents(this, contentsToken);
      Object.defineProperty(this, 'contentView', {
        value: views.contentView(this),
        enumerable: true
      });
      windows.set(this.id, this);

      this.once('closed', () => cancelIpc(this));
    }

    static getAllWindows() {
      return [...windows.values()];
    }

    static fromId(id) {
      return windows.get(id) ?? null;
    }

    static fromWebContents(contents) {
      if (!contents || allContents.get(contents.id) !== contents) {
        return null;
      }
      if (typeof native.ownerWindow === 'function') {
        const owner = windows.get(native.ownerWindow(contents.id));
        return owner && !owner._closing && !owner._destroyed ? owner : null;
      }
      // Projection-only policy hosts do not own widgets. Production queries the
      // actual Qt parent above, rather than trusting only JS presentation metadata.
      return (
        [...windows.values()].find((win) => !win._closing && win.webContents === contents) ??
        views.ownerOf(contents) ??
        null
      );
    }

    isDestroyed() {
      return this._destroyed;
    }

    getBackgroundColor() {
      alive(this);
      return background.get(this.id);
    }

    setBackgroundColor(value) {
      alive(this);
      background.set(this.id, value);
    }

    close() {
      alive(this);
      this._closeRequest();
    }

    destroy() {
      if (this._destroyed || this._closing) {
        return;
      }
      this._closing = true;
      native.destroy(this.id);
    }

    _closeRequest() {
      if (this._destroyed || this._closing) {
        return true;
      }
      const e = event();
      this.emit('close', e);
      if (e.defaultPrevented) {
        return false;
      }
      this.destroy();
      return true;
    }

    show() {
      alive(this);
      native.show(this.id);
    }

    hide() {
      alive(this);
      native.hide(this.id);
    }

    isVisible() {
      alive(this);
      return native.visible(this.id);
    }

    focus() {
      alive(this);
      native.focus(this.id);
    }

    minimize() {
      alive(this);
      native.minimize(this.id);
    }

    maximize() {
      alive(this);
      native.maximize(this.id);
    }

    unmaximize() {
      alive(this);
      native.unmaximize(this.id);
    }

    restore() {
      alive(this);
      native.restore(this.id);
    }

    isMinimized() {
      alive(this);
      return native.minimized(this.id);
    }

    isMaximized() {
      alive(this);
      return native.maximized(this.id);
    }

    setTitle(title) {
      alive(this);
      if (typeof title !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'title must be a string');
      }
      native.title(this.id, title);
    }

    getTitle() {
      alive(this);
      return native.titleText(this.id);
    }

    loadFile(...args) {
      return this.webContents.loadFile(...args);
    }

    loadURL(...args) {
      return this.webContents.loadURL(...args);
    }
  }

  views = createViews(
    native,
    EventEmitter,
    (owner) => new WebContents(owner, contentsToken),
    preferences,
    () => {
      if (exited) {
        throw error('ERR_FIREFLY_APP_EXITED', 'Application is exiting');
      }
      if (!ready) {
        throw error('ERR_FIREFLY_NOT_READY', 'Cannot create a view before app readiness');
      }
    },
    error,
    unsupported,
    background
  );

  const webContents = {
    fromId(id) {
      return allContents.get(id) ?? undefined;
    },

    getAllWebContents() {
      return [...allContents.values()].filter((contents) => !contents.isDestroyed());
    },

    getFocusedWebContents() {
      return allContents.get(native.focusedContents()) ?? null;
    }
  };

  native.setHandler((id, type, data) => {
    if (type === 'menu-click') {
      menus.dispatch(id, JSON.parse(data));
      return;
    }
    if (type === 'permission-request') {
      permissions.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'web-request') {
      requests.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'protocol-request' || type === 'protocol-cancel' || type === 'protocol-pull') {
      protocols.dispatch(type, JSON.parse(data));
      return;
    }
    if (type === 'ready') {
      ready = true;
      screens.ready();
      power.ready();
      app.emit('ready');
      resolveReady();
      return;
    }
    if (type === 'preferences-notification') {
      systemPreferencesService.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'shell-result') {
      desktopShell.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'gpu-info-update') {
      if (!exited) {
        app.emit('gpu-info-update');
      }
      return;
    }
    if (type === 'gpu-process-gone') {
      if (exited) {
        return;
      }
      const details = JSON.parse(data);
      if (
        !details ||
        details.type !== 'GPU' ||
        !Number.isInteger(details.pid) ||
        details.pid < 1 ||
        !Number.isInteger(details.exitCode) ||
        !['clean-exit', 'abnormal-exit', 'crashed', 'killed', 'unknown'].includes(details.reason)
      ) {
        throw error('ERR_FIREFLY_GPU_DATA', 'Invalid native compositor exit event');
      }
      app.emit('child-process-gone', event(), details);
      return;
    }
    if (type === 'dialog-result') {
      dialogs.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'global-shortcut-event') {
      shortcuts.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'power-monitor-event') {
      power.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'notification-result') {
      notifications.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'desktop-capture-result') {
      capture.dispatch(JSON.parse(data));
      return;
    }
    if (type === 'screen-changed') {
      screens.changed();
      return;
    }
    if (type === 'theme-changed') {
      theme.changed();
      return;
    }
    const win = windows.get(id) ?? views.records.get(id);
    if (!win) {
      return;
    }
    const contents = win.webContents;
    if (type === 'ipc-send') {
      dispatchIpcEvent(win, typeof data === 'string' ? JSON.parse(data) : data);
      return;
    }
    if (type === 'ipc-invoke') {
      dispatchIpc(win, typeof data === 'string' ? JSON.parse(data) : data);
      return;
    }
    if (type === 'close-request' || type === 'contents-close-request') {
      return win._closeRequest();
    }
    if (type === 'contents-destroyed') {
      cancelIpc(win);
      contents._reject(error('ERR_ABORTED', 'Web contents closed'));
      allContents.delete(id);
      views.destroyed(id);
      return;
    }
    if (type === 'closed') {
      views.detachWindow(win);
      allContents.delete(id);
      win._destroyed = true;
      win._closing = false;
      windows.delete(id);
      contents._reject(error('ERR_ABORTED', 'Window closed'));
      contents.emit('destroyed');
      win.emit('closed');
      if (windows.size === 0) {
        if (quitting) {
          finishQuit();
        } else {
          app.emit('window-all-closed');
        }
      }
    } else if (type === 'loaded') {
      contents._url = data;
      const pending = contents._pending;
      contents._pending = null;
      if (pending) {
        clearTimeout(pending.timer);
      }
      contents.emit('did-finish-load');
      if (pending) {
        pending.resolve();
      }
    } else if (type === 'load-start') {
      cancelIpc(win);
      contents.emit('did-start-loading');
    } else if (type === 'url') {
      contents._url = data;
    } else if (type === 'title') {
      contents._title = data;
      contents.emit('page-title-updated', event(), data, true);
      if (windows.has(id)) {
        const e = event();
        win.emit('page-title-updated', e, data, true);
        if (!e.defaultPrevented && !win._closing) {
          native.title(id, data);
        }
      }
    } else if (type === 'paint' && windows.has(id)) {
      win.emit('ready-to-show');
    } else if (type === 'failed') {
      contents._reject(error('ERR_FAILED', data));
      contents.emit('did-fail-load', event(), -2, data, contents._url, true);
    } else if (type === 'crashed') {
      cancelIpc(win);
      contents._reject(error('ERR_FAILED', 'Renderer process crashed'));
      contents.emit('render-process-gone', event(), { reason: 'crashed', exitCode: -1 });
    } else if (['minimize', 'maximize', 'unmaximize', 'restore'].includes(type)) {
      win.emit(type);
    } else if (type === 'resize') {
      win.emit('resize');
    } else if (type === 'move') {
      win.emit('move');
    }
  });

  return {
    app,
    Menu,
    MenuItem,
    contentTracing,
    crashReporter,
    protocol,
    session,
    BrowserWindow,
    WebContentsView: views.WebContentsView,
    webContents,
    webFrameMain: Object.freeze({ fromId: frames.fromId }),
    ipcMain,
    MessageChannelMain: channels.MessageChannelMain,
    utilityProcess: utilities.utilityProcess,
    safeStorage: createSafeStorage(native, app, error, unsupported),
    screen: screens.screen,
    clipboard,
    ClipboardItem,
    nativeTheme: theme.nativeTheme,
    desktopCapturer: capture.desktopCapturer,
    nativeImage: images.nativeImage,
    Notification: notifications.Notification,
    powerMonitor: power.powerMonitor,
    powerSaveBlocker: powerSave.powerSaveBlocker,
    shell: desktopShell.shell,
    systemPreferences: systemPreferencesService.systemPreferences,
    dialog: dialogs.dialog,
    globalShortcut: shortcuts.globalShortcut,
    net: network.net
  };
}
