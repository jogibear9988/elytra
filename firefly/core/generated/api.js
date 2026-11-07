/* Generated from firefly/core/api.mjs; edit the ESM source. */
(function () {
'use strict';
// firefly/core/crash/index.mjs
function createCrashReporter(native, error, unsupported) {
  let started = false;
  function object(value, description) {
    if (value === null || typeof value !== "object" || Array.isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", `${description} must be an object`);
    }
  }
  function annotations(value = {}) {
    object(value, "Crash annotations");
    const result = /* @__PURE__ */ Object.create(null);
    const keys = Reflect.ownKeys(value);
    if (keys.length > 64) {
      throw error("ERR_OUT_OF_RANGE", "At most 64 annotations per group");
    }
    for (const key of keys) {
      if (typeof key !== "string" || !/^[A-Za-z0-9_.-]{1,39}$/.test(key)) {
        throw error("ERR_INVALID_ARG_VALUE", "Invalid annotation key");
      }
      const text = value[key];
      if (typeof text !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Annotation values must be strings");
      }
      if (text.length > 2048) {
        throw error("ERR_OUT_OF_RANGE", "Crash annotation value exceeds budget");
      }
      result[key] = text;
    }
    return result;
  }
  function requireStart() {
    if (!started) {
      throw error("ERR_FIREFLY_NOT_READY", "Start crashReporter first");
    }
  }
  return {
    start(options) {
      if (started) {
        throw error("ERR_FIREFLY_CRASH_REPORTER_STARTED", "Crash reporter already started");
      }
      object(options, "Crash options");
      const allowed = /* @__PURE__ */ new Set([
        "uploadToServer",
        "submitURL",
        "productName",
        "companyName",
        "ignoreSystemCrashHandler",
        "rateLimit",
        "compress",
        "extra",
        "globalExtra"
      ]);
      for (const key of Reflect.ownKeys(options)) {
        if (!allowed.has(key)) {
          unsupported("crashReporter option");
        }
      }
      for (const key of ["uploadToServer", "ignoreSystemCrashHandler", "rateLimit", "compress"]) {
        if (options[key] !== void 0 && typeof options[key] !== "boolean") {
          throw error("ERR_INVALID_ARG_TYPE", `${key} must be a boolean`);
        }
      }
      if (options.uploadToServer !== false) {
        unsupported("crash report uploads; set uploadToServer: false");
      }
      if (options.ignoreSystemCrashHandler === true) {
        unsupported("suppressing the operating-system crash handler");
      }
      if (options.submitURL !== void 0 && typeof options.submitURL !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "submitURL must be a string");
      }
      const productName = options.productName === void 0 ? "Firefly" : options.productName;
      const companyName = options.companyName === void 0 ? "" : options.companyName;
      for (const text of [productName, companyName]) {
        if (typeof text !== "string") {
          throw error("ERR_INVALID_ARG_TYPE", "Product/company names must be strings");
        }
        if (text.length > 256) {
          throw error("ERR_OUT_OF_RANGE", "Crash name exceeds metadata budget");
        }
      }
      const wire = JSON.stringify({
        productName,
        companyName,
        extra: annotations(options.extra),
        globalExtra: annotations(options.globalExtra)
      });
      if (wire.length > 16 * 1024) {
        throw error("ERR_OUT_OF_RANGE", "Crash metadata exceeds budget");
      }
      native.crashStart(wire);
      started = true;
    },
    getUploadToServer() {
      requireStart();
      return false;
    },
    setUploadToServer(value) {
      if (typeof value !== "boolean") {
        throw error("ERR_INVALID_ARG_TYPE", "Upload preference must be boolean");
      }
      if (value) {
        unsupported("crash report uploads");
      }
    },
    getParameters() {
      requireStart();
      return JSON.parse(native.crashParameters());
    }
  };
}

// firefly/core/protocol/files.mjs
function createFileHandlers(native, isReady, error, standard) {
  const handlers = /* @__PURE__ */ new Map();
  const pending = /* @__PURE__ */ new Map();
  function ready() {
    if (!isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "Protocol handlers require app readiness");
    }
  }
  function scheme(value) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", "scheme must be a string");
    }
    if (!/^[a-z][a-z0-9+.-]{0,63}$/.test(value)) {
      throw error("ERR_INVALID_ARG_VALUE", "Invalid canonical scheme");
    }
  }
  function metadata(token, response) {
    if (typeof response === "string") {
      response = { path: response };
    }
    if (response === null || typeof response !== "object" || Array.isArray(response)) {
      throw error("ERR_INVALID_ARG_TYPE", "File response must be a path or object");
    }
    for (const key of Reflect.ownKeys(response)) {
      if (!["path", "headers", "error"].includes(key)) {
        throw error("ERR_FIREFLY_UNSUPPORTED_API", "Unsupported file response option");
      }
    }
    if (Object.hasOwn(response, "error")) {
      if (Object.keys(response).length !== 1 || !Number.isInteger(response.error) || response.error >= 0) {
        throw error("ERR_INVALID_ARG_VALUE", "File response error must be a negative integer");
      }
      return { token, error: true };
    }
    if (typeof response.path !== "string" || response.path.length > 32768 || response.path.includes("\0")) {
      throw error("ERR_INVALID_ARG_VALUE", "File response needs a bounded native path");
    }
    const headers = [];
    if (response.headers !== void 0) {
      if (response.headers === null || typeof response.headers !== "object" || Array.isArray(response.headers)) {
        throw error("ERR_INVALID_ARG_TYPE", "File headers must be an object");
      }
      for (const name of Reflect.ownKeys(response.headers)) {
        if (typeof name !== "string") {
          throw error("ERR_INVALID_ARG_VALUE", "Header names must be strings");
        }
        const values = response.headers[name];
        for (const value of Array.isArray(values) ? values : [values]) {
          if (typeof value !== "string") {
            throw error("ERR_INVALID_ARG_TYPE", "Header values must be strings");
          }
          headers.push([name, value]);
          if (headers.length > 64) {
            throw error("ERR_OUT_OF_RANGE", "File response header budget exceeded");
          }
        }
      }
    }
    return { token, path: response.path, headers };
  }
  function deny(token) {
    return native.protocolFileReply(JSON.stringify({ token, error: true }));
  }
  function finish(token, response) {
    if (!pending.delete(token)) {
      return false;
    }
    try {
      const wire = JSON.stringify(metadata(token, response));
      if (wire.length > 128 * 1024) {
        throw error("ERR_OUT_OF_RANGE", "File reply exceeds metadata budget");
      }
      return native.protocolFileReply(wire);
    } catch (failure) {
      deny(token);
      throw failure;
    }
  }
  return {
    api: {
      registerFileProtocol(name, handler) {
        ready();
        scheme(name);
        if (typeof handler !== "function") {
          throw error("ERR_INVALID_ARG_TYPE", "File handler must be callable");
        }
        if (!standard(name)) {
          throw error(
            "ERR_FIREFLY_UNSUPPORTED_API",
            "File handlers require a registered standard custom scheme with supportFetchAPI: true"
          );
        }
        if (handlers.has(name)) {
          return false;
        }
        const generation = native.protocolRegisterFile(name);
        if (generation === false) {
          return false;
        }
        handlers.set(name, { handler, generation });
        return true;
      },
      unregisterProtocol(name) {
        ready();
        scheme(name);
        const removed = native.protocolUnregisterFile(name);
        handlers.delete(name);
        for (const [token, request] of pending) {
          if (request.scheme === name) {
            pending.delete(token);
          }
        }
        return removed;
      },
      isProtocolHandled(name) {
        ready();
        scheme(name);
        return handlers.has(name);
      }
    },
    canLoad: (name) => handlers.has(name),
    dispatch(type, request) {
      if (type === "protocol-cancel") {
        pending.delete(request.token);
        return;
      }
      const entry = handlers.get(request.scheme);
      if (!entry || entry.generation !== request.generation || pending.size >= 64) {
        deny(request.token);
        return;
      }
      pending.set(request.token, { scheme: request.scheme });
      try {
        const result = entry.handler(
          Object.freeze({ url: request.url, method: request.method }),
          (response) => {
            finish(request.token, response);
          }
        );
        Promise.resolve(result).catch(() => {
          if (pending.delete(request.token)) {
            deny(request.token);
          }
        });
      } catch {
        if (pending.delete(request.token)) {
          deny(request.token);
        }
      }
    }
  };
}

// firefly/core/protocol/index.mjs
function createProtocol(native, isReady, error, unsupported) {
  let registered = false;
  const fetchEnabledSchemes = /* @__PURE__ */ new Set();
  const files = createFileHandlers(
    native,
    isReady,
    error,
    (scheme) => fetchEnabledSchemes.has(scheme)
  );
  const reserved = /* @__PURE__ */ new Set([
    "http",
    "https",
    "ws",
    "wss",
    "ftp",
    "file",
    "about",
    "blob",
    "data",
    "javascript",
    "resource",
    "chrome",
    "chrome-extension",
    "devtools",
    "webkit",
    "moz-extension",
    "mite",
    "mite-host",
    "firefly"
  ]);
  const privileges = /* @__PURE__ */ new Set([
    "standard",
    "secure",
    "bypassCSP",
    "allowServiceWorkers",
    "supportFetchAPI",
    "corsEnabled",
    "stream",
    "codeCache"
  ]);
  function object(value, description) {
    if (value === null || typeof value !== "object" || Array.isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", `${description} must be an object`);
    }
  }
  const protocol = {
    ...files.api,
    registerSchemesAsPrivileged(customSchemes) {
      if (isReady()) {
        throw error("ERR_FIREFLY_PROTOCOL_TOO_LATE", "Register schemes before app is ready");
      }
      if (registered) {
        throw error(
          "ERR_FIREFLY_PROTOCOL_ALREADY_REGISTERED",
          "Schemes can be registered only once"
        );
      }
      if (!Array.isArray(customSchemes)) {
        throw error("ERR_INVALID_ARG_TYPE", "customSchemes must be an array");
      }
      if (customSchemes.length > 32) {
        throw error("ERR_OUT_OF_RANGE", "At most 32 custom schemes can be registered");
      }
      const seen = /* @__PURE__ */ new Set();
      const schemes = [];
      const fetchSchemes = [];
      for (const entry of customSchemes) {
        object(entry, "Custom scheme");
        for (const key of Reflect.ownKeys(entry)) {
          if (key !== "scheme" && key !== "privileges") {
            throw error("ERR_INVALID_ARG_VALUE", "Unknown custom scheme option");
          }
        }
        const scheme = entry.scheme;
        if (typeof scheme !== "string") {
          throw error("ERR_INVALID_ARG_TYPE", "scheme must be a string");
        }
        if (!/^[a-z][a-z0-9+.-]{0,63}$/.test(scheme) || reserved.has(scheme) || seen.has(scheme)) {
          throw error("ERR_INVALID_ARG_VALUE", "Scheme must be canonical, custom and unique");
        }
        seen.add(scheme);
        const values = entry.privileges === void 0 ? {} : entry.privileges;
        object(values, "Scheme privileges");
        let standard = false;
        let secure = false;
        let fetch = false;
        for (const key of Reflect.ownKeys(values)) {
          if (!privileges.has(key)) {
            throw error("ERR_INVALID_ARG_VALUE", "Unknown scheme privilege");
          }
          const value = values[key];
          if (typeof value !== "boolean") {
            throw error("ERR_INVALID_ARG_TYPE", `Privilege ${key} must be a boolean`);
          }
          if (key === "standard") {
            standard = value;
          } else if (key === "secure") {
            secure = value;
          } else if (key === "supportFetchAPI") {
            fetch = value;
          } else if (value) {
            unsupported(`protocol privilege ${key}`);
          }
        }
        if (secure && !standard) {
          unsupported("secure nonstandard protocol origins");
        }
        if (fetch && !standard) {
          unsupported("fetch for nonstandard custom protocols");
        }
        if (fetch) {
          fetchSchemes.push(scheme);
        }
        schemes.push({ scheme, standard, secure });
      }
      native.registerSchemes(JSON.stringify({ schema: 1, schemes }));
      registered = true;
      for (const scheme of fetchSchemes) {
        fetchEnabledSchemes.add(scheme);
      }
    }
  };
  return { protocol, dispatch: files.dispatch, canLoad: files.canLoad };
}

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

// firefly/core/clipboard/index.mjs
function createClipboard(native, error, unsupported) {
  const BlobClass = globalThis.Blob;
  const BufferClass = globalThis.Buffer;
  const entries = /* @__PURE__ */ new WeakMap();
  const formats = ["text/plain", "text/html", "text/rtf"];
  const budget = 8 * 1024 * 1024;
  function supported(format) {
    if (!formats.includes(format)) {
      unsupported(`clipboard format ${format}`);
    }
  }
  function request(value) {
    const result = JSON.parse(native.clipboardRequest(JSON.stringify(value)));
    if (result.error) {
      throw error(result.error.code, result.error.message);
    }
    return result;
  }
  class ClipboardItem {
    constructor(items) {
      if (!items || typeof items !== "object" || Array.isArray(items)) {
        throw error("ERR_INVALID_ARG_TYPE", "ClipboardItem requires a MIME record");
      }
      const values = new Map(Object.entries(items));
      if (!values.size) {
        throw error("ERR_INVALID_ARG_VALUE", "ClipboardItem needs at least one representation");
      }
      entries.set(this, values);
    }
    get types() {
      return [...entries.get(this).keys()];
    }
    async getType(type) {
      const values = entries.get(this);
      if (!values.has(type)) {
        throw error("ERR_INVALID_ARG_VALUE", "ClipboardItem does not contain this type");
      }
      const value = await values.get(type);
      if (typeof value === "string") {
        return new BlobClass([value], { type });
      }
      if (value instanceof BlobClass) {
        return value;
      }
      throw error("ERR_INVALID_ARG_TYPE", "Clipboard payload must resolve to a string or Blob");
    }
  }
  const clipboard = {
    async readText() {
      return request({ op: "readText" }).text;
    },
    async writeText(text) {
      if (typeof text !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "text must be a string");
      }
      await clipboard.write([new ClipboardItem({ "text/plain": text })]);
    },
    async has(format) {
      supported(format);
      return request({ op: "has", format }).has;
    },
    async read() {
      const { items } = request({ op: "read" });
      return items.map(
        (item) => new ClipboardItem(
          Object.fromEntries(
            item.map(({ format, data }) => [
              format,
              new BlobClass([BufferClass.from(data, "base64")], { type: format })
            ])
          )
        )
      );
    },
    async write(items) {
      if (!Array.isArray(items) || items.length > 1) {
        throw error("ERR_INVALID_ARG_VALUE", "clipboard.write accepts zero or one ClipboardItem");
      }
      const selected = items.map((item) => {
        if (!entries.has(item)) {
          throw error("ERR_INVALID_ARG_TYPE", "Expected a Firefly ClipboardItem");
        }
        const types = item.types;
        types.forEach(supported);
        return { item, types };
      });
      let size = 0;
      const encoded = [];
      for (const { item, types } of selected) {
        const representations = [];
        for (const format of types) {
          const blob = await item.getType(format);
          size += blob.size;
          if (size > budget) {
            throw error("ERR_FIREFLY_CLIPBOARD_LIMIT", "Clipboard data exceeds eight MiB");
          }
          const bytes = BufferClass.from(await blob.arrayBuffer());
          representations.push({ format, data: bytes.toString("base64") });
        }
        encoded.push(representations);
      }
      request({ op: "write", items: encoded });
    },
    clear() {
      request({ op: "clear" });
    }
  };
  return { clipboard, ClipboardItem };
}

// firefly/core/theme/index.mjs
function createTheme(native, EventEmitter, error) {
  const nativeTheme = new EventEmitter();
  let source = "system";
  let baseline = null;
  const fields = [
    "shouldUseHighContrastColors",
    "shouldUseInvertedColorScheme",
    "prefersReducedTransparency",
    "shouldDifferentiateWithoutColor"
  ];
  function requireNativeTheme() {
    if (native.themeAvailable === false) {
      throw error(
        "ERR_FIREFLY_UNSUPPORTED_API",
        "Native theme services are not implemented on this platform"
      );
    }
  }
  function snapshot() {
    requireNativeTheme();
    const system = JSON.parse(native.themeSnapshot());
    const dark = source === "system" ? system.dark : source === "dark";
    const value = {
      themeSource: source,
      shouldUseDarkColors: dark,
      shouldUseDarkColorsForSystemIntegratedUI: dark
    };
    for (const key of fields) {
      value[key] = system[key];
    }
    if (!baseline) {
      baseline = value;
    }
    return value;
  }
  Object.defineProperty(nativeTheme, "themeSource", {
    enumerable: true,
    get() {
      requireNativeTheme();
      return source;
    },
    set(value) {
      requireNativeTheme();
      if (!["system", "light", "dark"].includes(value)) {
        throw error("ERR_INVALID_ARG_VALUE", "themeSource must be system, light or dark");
      }
      if (value === source) {
        return;
      }
      snapshot();
      native.themeSetSource(value);
      source = value;
    }
  });
  for (const key of [
    "shouldUseDarkColors",
    "shouldUseDarkColorsForSystemIntegratedUI",
    ...fields
  ]) {
    Object.defineProperty(nativeTheme, key, {
      enumerable: true,
      get: () => snapshot()[key]
    });
  }
  nativeTheme.on("newListener", (name) => {
    if (name === "updated") {
      snapshot();
    }
  });
  function changed() {
    if (!baseline) {
      return;
    }
    const next = snapshot();
    const previous = baseline;
    baseline = next;
    if (Object.keys(next).some((key) => next[key] !== previous[key])) {
      nativeTheme.emit("updated");
    }
  }
  return { nativeTheme, changed };
}

// firefly/core/window/geometry.mjs
function createWindowGeometry(native, EventEmitter, alive, dimension, error, unsupported) {
  function coordinate(value, name) {
    if (typeof value !== "number" || !Number.isInteger(value) || value < -2147483648 || value > 2147483647) {
      throw error("ERR_INVALID_ARG_TYPE", `${name} must be a signed 32-bit integer`);
    }
    return value;
  }
  function get(window, content) {
    alive(window);
    return JSON.parse(native.windowBounds(window.id, content));
  }
  function set(window, value, content, animate) {
    alive(window);
    if (animate) {
      unsupported("animated window geometry");
    }
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", "bounds must be a rectangle object");
    }
    const updates = {};
    if (content && ["x", "y", "width", "height"].some((key) => !Object.hasOwn(value, key))) {
      throw error("ERR_INVALID_ARG_VALUE", "Content bounds require a complete rectangle");
    }
    for (const key of Object.keys(value)) {
      if (!["x", "y", "width", "height"].includes(key)) {
        unsupported(`window bounds ${key}`);
      }
      updates[key] = ["width", "height"].includes(key) ? dimension(value[key], key) : coordinate(value[key], key);
    }
    const rect = { ...get(window, content), ...updates };
    if (rect.x + rect.width - 1 > 2147483647 || rect.y + rect.height - 1 > 2147483647) {
      throw error("ERR_OUT_OF_RANGE", "Window rectangle exceeds signed 32-bit coordinates");
    }
    native.setWindowBounds(window.id, rect.x, rect.y, rect.width, rect.height, content);
  }
  class WindowGeometry extends EventEmitter {
    getBounds() {
      return get(this, false);
    }
    getContentBounds() {
      return get(this, true);
    }
    setBounds(bounds, animate = false) {
      set(this, bounds, false, animate);
    }
    setContentBounds(bounds, animate = false) {
      set(this, bounds, true, animate);
    }
    getSize() {
      const { width, height } = this.getBounds();
      return [width, height];
    }
    getContentSize() {
      const { width, height } = this.getContentBounds();
      return [width, height];
    }
    setSize(width, height, animate = false) {
      this.setBounds({ width, height }, animate);
    }
    setContentSize(width, height, animate = false) {
      this.setContentBounds({ ...this.getContentBounds(), width, height }, animate);
    }
    getPosition() {
      const { x, y } = this.getBounds();
      return [x, y];
    }
    setPosition(x, y, animate = false) {
      this.setBounds({ x, y }, animate);
    }
    center() {
      alive(this);
      native.centerWindow(this.id);
    }
  }
  return { WindowGeometry, coordinate };
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
    getPath(name) {
      if (typeof name !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Path name must be a string");
      }
      if (name !== "crashDumps") {
        unsupported(`app.getPath(${name})`);
      }
      return native.crashDirectory();
    }
    setPath(name, value) {
      if (typeof name !== "string" || typeof value !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Path name/value must be strings");
      }
      if (name !== "crashDumps") {
        unsupported(`app.setPath(${name})`);
      }
      native.crashDirectory(value);
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
  const crashReporter = createCrashReporter(native, error, unsupported);
  const protocols = createProtocol(native, () => ready, error, unsupported);
  const { protocol } = protocols;
  const defaultSession = Object.freeze({ protocol });
  const session = {};
  Object.defineProperty(session, "defaultSession", {
    enumerable: true,
    get() {
      if (!ready) {
        throw error("ERR_FIREFLY_NOT_READY", "defaultSession requires app readiness");
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
      if (!["file:", "http:", "https:"].includes(parsed.protocol) && !protocols.canLoad(parsed.protocol.slice(0, -1))) {
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
  class BrowserWindow extends geometry.WindowGeometry {
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
        if (![
          "width",
          "height",
          "x",
          "y",
          "useContentSize",
          "title",
          "show",
          "frame",
          "webPreferences"
        ].includes(key)) {
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
      const useContentSize = options.useContentSize ?? false;
      const x = options.x === void 0 ? void 0 : geometry.coordinate(options.x, "x");
      const y = options.y === void 0 ? void 0 : geometry.coordinate(options.y, "y");
      if (x !== void 0 && x + width - 1 > 2147483647 || y !== void 0 && y + height - 1 > 2147483647) {
        throw error(
          "ERR_OUT_OF_RANGE",
          "Initial window rectangle exceeds signed 32-bit coordinates"
        );
      }
      if (typeof useContentSize !== "boolean") {
        throw error("ERR_INVALID_ARG_TYPE", "useContentSize must be a boolean");
      }
      if (typeof title !== "string" || typeof show !== "boolean" || typeof frame !== "boolean") {
        throw error("ERR_INVALID_ARG_TYPE", "Invalid title/show option");
      }
      this._destroyed = false;
      this._closing = false;
      this.id = native.create(
        width,
        height,
        title,
        show,
        frame,
        prefs.preload,
        useContentSize,
        x,
        y
      );
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
    if (type === "protocol-request" || type === "protocol-cancel") {
      protocols.dispatch(type, JSON.parse(data));
      return;
    }
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
    if (type === "theme-changed") {
      theme.changed();
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
    } else if (type === "move") {
      win.emit("move");
    }
  });
  return {
    app,
    crashReporter,
    protocol,
    session,
    BrowserWindow,
    ipcMain,
    screen: screens.screen,
    clipboard,
    ClipboardItem,
    nativeTheme: theme.nativeTheme
  };
}
return createFireflyApi;
})()
