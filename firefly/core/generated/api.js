/* Generated from firefly/core/api.mjs; edit the ESM source. */
(function () {
'use strict';
// firefly/core/ipc-main.mjs
function createMainIpc(native, EventEmitter, error) {
  const ipcHandlers = /* @__PURE__ */ new Map();
  const ipcRequests = /* @__PURE__ */ new Map();
  function channelName(channel) {
    if (typeof channel !== "string" || channel.length > 256) {
      throw error("ERR_INVALID_ARG_TYPE", "channel must be a string of at most 256 UTF-16 units");
    }
    return channel;
  }
  class IpcMain extends EventEmitter {
    handle(channel, listener) {
      this._register(channel, listener, false);
    }
    handleOnce(channel, listener) {
      this._register(channel, listener, true);
    }
    _register(channel, listener, once) {
      channelName(channel);
      if (typeof listener !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "IPC handler must be a function");
      }
      if (ipcHandlers.has(channel)) {
        throw new Error("A handler is already registered for this channel");
      }
      ipcHandlers.set(channel, { listener, once });
    }
    removeHandler(channel) {
      ipcHandlers.delete(channelName(channel));
    }
  }
  const ipcMain = new IpcMain();
  function cancelIpc(window) {
    for (const [key, pending] of ipcRequests) {
      if (pending.window === window) {
        clearTimeout(pending.timer);
        ipcRequests.delete(key);
      }
    }
    window.webContents._ipcFrame = null;
  }
  function dispatchIpc(window, data) {
    const key = `${window.id}/${data.document}/${data.request}`;
    const finish = (success, value) => {
      const pending = ipcRequests.get(key);
      if (!pending) {
        return;
      }
      ipcRequests.delete(key);
      clearTimeout(pending.timer);
      let wire;
      try {
        wire = native.encodeIpc(value);
      } catch {
        success = false;
        wire = native.encodeIpc("IPC result is not supported bounded data");
      }
      native.replyIpc(window.id, data.document, data.realm, data.request, success, wire);
    };
    if (ipcRequests.size >= 64) {
      native.replyIpc(
        window.id,
        data.document,
        data.realm,
        data.request,
        false,
        native.encodeIpc("IPC pending request budget exceeded")
      );
      return;
    }
    const timer = setTimeout(() => finish(false, "IPC handler timed out"), 3e4);
    ipcRequests.set(key, { window, timer });
    try {
      const args = native.decodeIpc(data.wire);
      if (!Array.isArray(args) || args.length < 1 || args.length > 65) {
        throw Error("Invalid IPC arguments");
      }
      const channel = channelName(args.shift());
      const record = ipcHandlers.get(channel);
      if (!record) {
        finish(false, `No handler registered for '${channel}'`);
        return;
      }
      let frame = window.webContents._ipcFrame;
      if (!frame || frame._document !== data.document || frame._realm !== data.realm) {
        frame = {
          _document: data.document,
          _realm: data.realm,
          url: data.url,
          isDestroyed() {
            return window.isDestroyed() || window.webContents._ipcFrame !== frame;
          }
        };
        window.webContents._ipcFrame = frame;
      }
      frame.url = data.url;
      if (record.once) {
        ipcHandlers.delete(channel);
      }
      const event = { sender: window.webContents, senderFrame: frame };
      const result = Reflect.apply(record.listener, void 0, [event, ...args]);
      Promise.resolve(result).then(
        (value) => finish(true, value),
        (failure) => {
          let message = "IPC handler rejected";
          try {
            if (typeof failure?.message === "string") {
              message = failure.message.slice(0, 1024);
            }
          } catch {
          }
          finish(false, message);
        }
      );
    } catch (failure) {
      let message = "IPC handler rejected";
      try {
        if (typeof failure?.message === "string") {
          message = failure.message.slice(0, 1024);
        }
      } catch {
      }
      finish(false, message);
    }
  }
  return { ipcMain, cancel: cancelIpc, dispatch: dispatchIpc };
}

// firefly/core/screen/index.mjs
function createScreen(native, EventEmitter, isReady, error, unsupported) {
  const parse = JSON.parse;
  const clone = (value) => JSON.parse(JSON.stringify(value));
  const eventNames = ["display-added", "display-removed", "display-metrics-changed"];
  const screen = new EventEmitter();
  let baseline = null;
  function snapshot() {
    if (!isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "screen requires app.whenReady()");
    }
    const value = parse(native.screenSnapshot());
    if (!baseline) {
      baseline = value;
    }
    return value;
  }
  function primary(value) {
    const display = value.displays.find((display2) => display2.id === value.primaryId);
    if (!display) {
      throw error("ERR_FIREFLY_DISPLAY_UNAVAILABLE", "No primary display is available");
    }
    return display;
  }
  function integer(value, name) {
    if (typeof value !== "number" || !Number.isInteger(value) || value < -2147483648 || value > 2147483647) {
      throw error("ERR_INVALID_ARG_TYPE", `${name} must be a signed 32-bit integer`);
    }
    return value;
  }
  function point(value) {
    if (!value || typeof value !== "object") {
      throw error("ERR_INVALID_ARG_TYPE", "Expected a Point");
    }
    return { x: integer(value.x, "x"), y: integer(value.y, "y") };
  }
  function rectangle(value) {
    const result = point(value);
    result.width = integer(value.width, "width");
    result.height = integer(value.height, "height");
    if (result.width < 0 || result.height < 0) {
      throw error("ERR_OUT_OF_RANGE", "Rectangle dimensions must be nonnegative");
    }
    return result;
  }
  function distance(point2, rect) {
    const dx = Math.max(rect.x - point2.x, 0, point2.x - rect.x - rect.width);
    const dy = Math.max(rect.y - point2.y, 0, point2.y - rect.y - rect.height);
    return dx * dx + dy * dy;
  }
  function nearest(value, position) {
    let best = primary(value);
    let minimum = distance(position, best.bounds);
    for (const display of value.displays) {
      const current = distance(position, display.bounds);
      if (current < minimum) {
        minimum = current;
        best = display;
      }
    }
    return best;
  }
  screen.getAllDisplays = () => clone(snapshot().displays);
  screen.getPrimaryDisplay = () => clone(primary(snapshot()));
  screen.getCursorScreenPoint = () => {
    primary(snapshot());
    return parse(native.cursorScreenPoint());
  };
  screen.getDisplayNearestPoint = (value) => {
    const current = snapshot();
    return clone(nearest(current, point(value)));
  };
  screen.getDisplayMatching = (value) => {
    const current = snapshot();
    const rect = rectangle(value);
    let best = primary(current);
    let maximum = 0;
    for (const display of [best, ...current.displays.filter((display2) => display2.id !== best.id)]) {
      const b = display.bounds;
      const area = Math.max(0, Math.min(rect.x + rect.width, b.x + b.width) - Math.max(rect.x, b.x)) * Math.max(0, Math.min(rect.y + rect.height, b.y + b.height) - Math.max(rect.y, b.y));
      if (area > maximum) {
        maximum = area;
        best = display;
      }
    }
    if (!maximum) {
      best = nearest(current, { x: rect.x + rect.width / 2, y: rect.y + rect.height / 2 });
    }
    return clone(best);
  };
  for (const name of [
    "screenToDipPoint",
    "dipToScreenPoint",
    "screenToDipRect",
    "dipToScreenRect"
  ]) {
    screen[name] = () => unsupported(`screen.${name}`);
  }
  screen.on("newListener", (name) => {
    if (isReady() && eventNames.includes(name)) {
      snapshot();
    }
  });
  function ready() {
    if (eventNames.some((name) => screen.listenerCount(name))) {
      snapshot();
    }
  }
  function changed() {
    if (!isReady() || !baseline) {
      return;
    }
    const next = snapshot();
    const previous = baseline;
    baseline = next;
    const old = new Map(previous.displays.map((display) => [display.id, display]));
    const fresh = new Map(next.displays.map((display) => [display.id, display]));
    for (const display of previous.displays) {
      if (!fresh.has(display.id)) {
        screen.emit("display-removed", {}, clone(display));
      }
    }
    for (const display of next.displays) {
      const prior = old.get(display.id);
      if (!prior) {
        screen.emit("display-added", {}, clone(display));
      } else {
        const metrics = ["bounds", "workArea", "scaleFactor", "rotation"].filter(
          (key) => JSON.stringify(display[key]) !== JSON.stringify(prior[key])
        );
        if (metrics.length) {
          screen.emit("display-metrics-changed", {}, clone(display), metrics);
        }
      }
    }
  }
  return { screen, ready, changed };
}

// firefly/core/api.mjs
function createFireflyApi(native, EventEmitter, paths, urls) {
  const error = (code, message) => Object.assign(new Error(message), { code });
  const unsupported = (name) => {
    throw error("ERR_FIREFLY_UNSUPPORTED_API", `${name} is not implemented`);
  };
  const windows = /* @__PURE__ */ new Map();
  let ready = false;
  let quitting = false;
  let exited = false;
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
  class App extends EventEmitter {
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
      quitting = true;
      const e = event();
      this.emit("before-quit", e);
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
      if (typeof exitCode !== "number" || !Number.isInteger(exitCode)) {
        throw error("ERR_INVALID_ARG_TYPE", "exitCode must be an integer");
      }
      if (exitCode < -2147483648 || exitCode > 2147483647) {
        throw error("ERR_OUT_OF_RANGE", "exitCode must fit a signed 32-bit integer");
      }
      if (exited) {
        return;
      }
      exited = true;
      quitting = true;
      try {
        for (const win of [...windows.values()]) {
          native.destroyNow(win.id);
          win._destroyed = true;
          win._closing = false;
          windows.delete(win.id);
          win.webContents._reject(error("ERR_ABORTED", "Application exiting"));
          win.emit("closed");
        }
        app.emit("quit", event(), exitCode);
      } finally {
        native.exit(exitCode);
      }
    }
  }
  const app = new App();
  const screens = createScreen(native, EventEmitter, () => ready, error, unsupported);
  const {
    ipcMain,
    cancel: cancelIpc,
    dispatch: dispatchIpc
  } = createMainIpc(native, EventEmitter, error);
  function finishQuit() {
    const e = event();
    app.emit("will-quit", e);
    if (e.defaultPrevented) {
      quitting = false;
      return;
    }
    exited = true;
    app.emit("quit", event(), 0);
    native.quit();
  }
  function alive(win) {
    if (win._destroyed) {
      throw error("ERR_FIREFLY_OBJECT_DESTROYED", "BrowserWindow has been destroyed");
    }
  }
  function dimension(value, name) {
    if (!Number.isInteger(value) || value < 1 || value > 16384) {
      throw error("ERR_INVALID_ARG_VALUE", `${name} must be an integer in 1..16384`);
    }
    return value;
  }
  class WebContents extends EventEmitter {
    constructor(win) {
      super();
      this._window = win;
      this._url = "";
      this._title = "";
      this._pending = null;
      this._ipcFrame = null;
    }
    get mainFrame() {
      alive(this._window);
      return this._ipcFrame;
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
      return native.pid(this._window.id);
    }
    isDestroyed() {
      return this._window._destroyed;
    }
    loadURL(url, options) {
      alive(this._window);
      if (options !== void 0 && Object.keys(options).length) {
        unsupported("loadURL options");
      }
      if (typeof url !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "url must be a string");
      }
      let parsed;
      try {
        parsed = new URL(url);
      } catch {
        return Promise.reject(error("ERR_INVALID_URL", "Invalid URL"));
      }
      if (!["file:", "http:", "https:"].includes(parsed.protocol)) {
        return Promise.reject(
          error("ERR_FIREFLY_UNSUPPORTED_API", "URL scheme is not implemented")
        );
      }
      this._reject(error("ERR_ABORTED", "Navigation superseded"));
      return new Promise((resolve, reject) => {
        const timer = setTimeout(
          () => this._reject(error("ERR_TIMED_OUT", "Navigation timed out")),
          3e4
        );
        this._pending = { resolve, reject, timer };
        try {
          native.load(this._window.id, parsed.href);
        } catch (e) {
          this._reject(e);
        }
      });
    }
    loadFile(file, options) {
      if (typeof file !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "filePath must be a string");
      }
      if (options !== void 0 && Object.keys(options).length) {
        unsupported("loadFile options");
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
  class BrowserWindow extends EventEmitter {
    constructor(options = {}) {
      super();
      if (exited) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application is exiting");
      }
      if (!ready) {
        throw error("ERR_FIREFLY_NOT_READY", "Cannot create BrowserWindow before app is ready");
      }
      if (!options || typeof options !== "object" || Array.isArray(options)) {
        throw error("ERR_INVALID_ARG_TYPE", "BrowserWindow options must be an object");
      }
      for (const key of Object.keys(options)) {
        if (!["width", "height", "title", "show", "frame", "webPreferences"].includes(key)) {
          unsupported(`BrowserWindow option ${key}`);
        }
      }
      const prefs = options.webPreferences ?? {};
      if (!prefs || typeof prefs !== "object" || Array.isArray(prefs)) {
        throw error("ERR_INVALID_ARG_TYPE", "webPreferences must be an object");
      }
      for (const key of Object.keys(prefs)) {
        if (key === "preload") {
          continue;
        }
        if (!["contextIsolation", "nodeIntegration", "sandbox"].includes(key)) {
          unsupported(`webPreferences.${key}`);
        }
        if (prefs[key] !== (key !== "nodeIntegration")) {
          unsupported(`webPreferences.${key}=${prefs[key]}`);
        }
      }
      if (prefs.preload !== void 0) {
        if (typeof prefs.preload !== "string") {
          throw error("ERR_INVALID_ARG_TYPE", "preload must be a string");
        }
        if (!paths.isAbsolute(prefs.preload)) {
          throw error("ERR_INVALID_ARG_VALUE", "preload must be an absolute path");
        }
        if (paths.extname(prefs.preload) === ".mjs") {
          unsupported("ES module preloads");
        }
      }
      const width = dimension(options.width ?? 800, "width");
      const height = dimension(options.height ?? 600, "height");
      const title = options.title ?? "Firefly";
      const show = options.show ?? true;
      const frame = options.frame ?? true;
      if (typeof title !== "string" || typeof show !== "boolean" || typeof frame !== "boolean") {
        throw error("ERR_INVALID_ARG_TYPE", "Invalid title/show option");
      }
      this._destroyed = false;
      this._closing = false;
      this.id = native.create(width, height, title, show, frame, prefs.preload);
      this.webContents = new WebContents(this);
      windows.set(this.id, this);
      this.once("closed", () => cancelIpc(this));
    }
    static getAllWindows() {
      return [...windows.values()];
    }
    static fromId(id) {
      return windows.get(id) ?? null;
    }
    static fromWebContents(contents) {
      return [...windows.values()].find((w) => w.webContents === contents) ?? null;
    }
    isDestroyed() {
      return this._destroyed;
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
      this.emit("close", e);
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
    setSize(width, height, animate = false) {
      alive(this);
      if (animate) {
        unsupported("animated resize");
      }
      native.resize(this.id, dimension(width, "width"), dimension(height, "height"));
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
    getSize() {
      alive(this);
      return native.size(this.id);
    }
    setTitle(title) {
      alive(this);
      if (typeof title !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "title must be a string");
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
  native.setHandler((id, type, data) => {
    if (type === "ready") {
      ready = true;
      screens.ready();
      app.emit("ready");
      resolveReady();
      return;
    }
    if (type === "screen-changed") {
      screens.changed();
      return;
    }
    const win = windows.get(id);
    if (!win) {
      return;
    }
    const contents = win.webContents;
    if (type === "ipc-invoke") {
      dispatchIpc(win, JSON.parse(data));
      return;
    }
    if (type === "close-request") {
      return win._closeRequest();
    }
    if (type === "closed") {
      win._destroyed = true;
      win._closing = false;
      windows.delete(id);
      contents._reject(error("ERR_ABORTED", "Window closed"));
      win.emit("closed");
      if (windows.size === 0) {
        if (quitting) {
          finishQuit();
        } else {
          app.emit("window-all-closed");
        }
      }
    } else if (type === "loaded") {
      contents._url = data;
      const pending = contents._pending;
      contents._pending = null;
      if (pending) {
        clearTimeout(pending.timer);
      }
      contents.emit("did-finish-load");
      if (pending) {
        pending.resolve();
      }
    } else if (type === "load-start") {
      cancelIpc(win);
      contents.emit("did-start-loading");
    } else if (type === "url") {
      contents._url = data;
    } else if (type === "title") {
      contents._title = data;
      const e = event();
      win.emit("page-title-updated", e, data, true);
      if (!e.defaultPrevented && !win._closing) {
        native.title(id, data);
      }
    } else if (type === "paint") {
      win.emit("ready-to-show");
    } else if (type === "failed") {
      contents._reject(error("ERR_FAILED", data));
      contents.emit("did-fail-load", event(), -2, data, contents._url, true);
    } else if (type === "crashed") {
      cancelIpc(win);
      contents._reject(error("ERR_FAILED", "Renderer process crashed"));
      contents.emit("render-process-gone", event(), { reason: "crashed", exitCode: -1 });
    } else if (["minimize", "maximize", "unmaximize", "restore"].includes(type)) {
      win.emit(type);
    } else if (type === "resize") {
      win.emit("resize");
    }
  });
  return { app, BrowserWindow, ipcMain, screen: screens.screen };
}
return createFireflyApi;
})()
