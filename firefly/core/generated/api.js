/* Generated from firefly/core/api.mjs; edit the ESM source. */
(function () {
'use strict';
// firefly/core/menu/index.mjs
function createMenus(native, error, unsupported, quit, resolveWindow) {
  const menuState = /* @__PURE__ */ new WeakMap();
  const itemState = /* @__PURE__ */ new WeakMap();
  let current = null;
  const building = /* @__PURE__ */ new Set();
  let generation = 0;
  let nextCommand = 0;
  let commands = /* @__PURE__ */ new Map();
  const options = /* @__PURE__ */ new Set([
    "id",
    "label",
    "type",
    "submenu",
    "enabled",
    "visible",
    "checked",
    "click",
    "role"
  ]);
  function text(value, name) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", `${name} must be a string`);
    }
    if (value.length > 1024 || value.includes("\0")) {
      throw error("ERR_OUT_OF_RANGE", `${name} exceeds menu text budget`);
    }
    return value;
  }
  function boolean(value, name) {
    if (typeof value !== "boolean") {
      throw error("ERR_INVALID_ARG_TYPE", `${name} must be boolean`);
    }
    return value;
  }
  function graph(menu) {
    const seen = /* @__PURE__ */ new Set();
    const ids = /* @__PURE__ */ new Set();
    const map = /* @__PURE__ */ new Map();
    function walk(value, depth, enabled = true, visible = true) {
      if (depth > 8 || seen.has(value)) {
        throw error("ERR_INVALID_ARG_VALUE", "Menu cycle, shared submenu or excessive depth");
      }
      seen.add(value);
      const state = menuState.get(value);
      if (!state) {
        throw error("ERR_INVALID_ARG_TYPE", "submenu must be a Menu");
      }
      return state.items.map((item) => {
        const data = itemState.get(item);
        if (map.has(data.command)) {
          throw error("ERR_INVALID_ARG_VALUE", "Repeated MenuItem");
        }
        if (map.size >= 256) {
          throw error("ERR_OUT_OF_RANGE", "At most 256 menu items");
        }
        if (data.id && ids.has(data.id)) {
          throw error("ERR_INVALID_ARG_VALUE", "Duplicate menu item id");
        }
        if (data.id) {
          ids.add(data.id);
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
          submenu: data.submenu ? walk(data.submenu, depth + 1, enabled && data.enabled, visible && data.visible) : []
        };
      });
    }
    return { items: walk(menu, 1), map, menus: seen };
  }
  function install(menu) {
    if (generation === 2147483647) {
      throw error("ERR_OUT_OF_RANGE", "Menu generation exhausted");
    }
    const snapshot = menu === null ? { items: null, map: /* @__PURE__ */ new Map(), menus: /* @__PURE__ */ new Set() } : graph(menu);
    const wire = JSON.stringify({ schema: 1, generation: generation + 1, items: snapshot.items });
    if (wire.length > 64 * 1024) {
      throw error("ERR_OUT_OF_RANGE", "Menu wire budget exceeded");
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
      if (value === null || typeof value !== "object" || Array.isArray(value)) {
        throw error("ERR_INVALID_ARG_TYPE", "MenuItem options must be an object");
      }
      for (const key of Reflect.ownKeys(value)) {
        if (!options.has(key)) {
          unsupported(`MenuItem option ${String(key)}`);
        }
      }
      const role = value.role;
      if (role !== void 0 && !["quit", "close"].includes(role)) {
        unsupported(`MenuItem role ${role}`);
      }
      const submenu = value.submenu === void 0 ? null : Array.isArray(value.submenu) ? Menu.buildFromTemplate(value.submenu) : value.submenu;
      if (submenu !== null && !menuState.has(submenu)) {
        throw error("ERR_INVALID_ARG_TYPE", "submenu must be a Menu or template");
      }
      const type = value.type === void 0 ? submenu ? "submenu" : "normal" : value.type;
      if (!["normal", "separator", "checkbox", "submenu"].includes(type)) {
        unsupported(`MenuItem type ${type}`);
      }
      if (type === "submenu" !== (submenu !== null)) {
        throw error("ERR_INVALID_ARG_VALUE", "Submenu type/content mismatch");
      }
      if (value.click !== void 0 && typeof value.click !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "click must be callable");
      }
      if ((type === "separator" || type === "submenu") && (value.click !== void 0 || role !== void 0)) {
        unsupported("click/role on a separator or submenu");
      }
      if (role !== void 0 && type !== "normal") {
        unsupported("role on non-normal item");
      }
      if (value.checked !== void 0 && type !== "checkbox") {
        unsupported("checked on a non-checkbox item");
      }
      if (nextCommand === 2147483647) {
        throw error("ERR_OUT_OF_RANGE", "Menu command ids exhausted");
      }
      const data = {
        command: ++nextCommand,
        parent: null,
        role,
        submenu,
        type,
        id: text(value.id === void 0 ? "" : value.id, "id"),
        label: text(
          value.label === void 0 ? role === "quit" ? "Quit" : role === "close" ? "Close" : "" : value.label,
          "label"
        ),
        enabled: value.enabled === void 0 ? true : boolean(value.enabled, "enabled"),
        visible: value.visible === void 0 ? true : boolean(value.visible, "visible"),
        checked: value.checked === void 0 ? false : boolean(value.checked, "checked"),
        click: value.click
      };
      itemState.set(this, data);
      for (const key of ["type", "id", "role", "submenu"]) {
        Object.defineProperty(this, key, { enumerable: true, get: () => data[key] });
      }
      Object.defineProperty(this, "commandId", { enumerable: true, get: () => data.command });
      for (const key of ["label", "enabled", "visible", "checked"]) {
        Object.defineProperty(this, key, {
          enumerable: true,
          get: () => data[key],
          set: (value2) => {
            if (key === "checked" && type !== "checkbox") {
              unsupported("checked on a non-checkbox item");
            }
            update(this, key, key === "label" ? text(value2, key) : boolean(value2, key));
          }
        });
      }
      Object.defineProperty(this, "click", {
        enumerable: true,
        get: () => data.click,
        set: (value2) => {
          if (typeof value2 !== "function" && value2 !== void 0) {
            throw error("ERR_INVALID_ARG_TYPE", "click must be callable");
          }
          if (value2 && type !== "normal" && type !== "checkbox") {
            unsupported("click on a separator/submenu");
          }
          data.click = value2;
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
        throw error("ERR_INVALID_ARG_TYPE", "Menu template must be an array");
      }
      if (building.has(template) || building.size >= 8) {
        throw error("ERR_INVALID_ARG_VALUE", "Recursive or excessive template depth");
      }
      building.add(template);
      try {
        const menu = new Menu();
        const list = menuState.get(menu).items;
        for (const value of template) {
          const item = itemState.has(value) ? value : new MenuItem(value);
          if (itemState.get(item).parent) {
            unsupported("sharing MenuItem between menus");
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
        throw error("ERR_INVALID_ARG_TYPE", "Expected Menu or null");
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
        throw error("ERR_INVALID_ARG_TYPE", "Expected MenuItem");
      }
      const data = itemState.get(item);
      const list = menuState.get(this).items;
      if (!Number.isInteger(position) || position < 0 || position > list.length) {
        throw error("ERR_OUT_OF_RANGE", "Invalid menu insertion index");
      }
      if (current && graph(current).menus.has(this)) {
        unsupported("structural changes to an installed application menu");
      }
      if (data.parent) {
        unsupported("sharing MenuItem between menus");
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
      text(id, "id");
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
      if (data.type !== "normal" && data.type !== "checkbox") {
        return;
      }
      if (data.type === "checkbox") {
        if (typeof event.checked !== "boolean") {
          return;
        }
        data.checked = event.checked;
      }
      const window = resolveWindow(windowId);
      if (data.role === "quit") {
        quit();
      } else if (data.role === "close") {
        window?.close();
      } else if (data.click) {
        Reflect.apply(data.click, void 0, [
          item,
          window,
          {
            shiftKey: !!(event.modifiers & 1),
            ctrlKey: !!(event.modifiers & 2),
            altKey: !!(event.modifiers & 4),
            metaKey: !!(event.modifiers & 8)
          }
        ]);
      }
    }
  };
}

// firefly/core/tracing/index.mjs
function createTracing(native, isReady, files, paths, error, unsupported) {
  let state = "idle";
  let pending;
  let temporary;
  function ready() {
    if (!isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "contentTracing requires app readiness");
    }
  }
  function object(value) {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", "Tracing options must be an object");
    }
  }
  function categories(value, available) {
    if (!Array.isArray(value) || value.length > 32) {
      throw error(
        "ERR_INVALID_ARG_VALUE",
        "Tracing categories must be an array of at most 32 entries"
      );
    }
    for (const item of value) {
      if (typeof item !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Tracing categories must be strings");
      }
      if (item !== "*" && !available.includes(item)) {
        unsupported(`Tracing category ${item}`);
      }
    }
    return [...value];
  }
  function config(options) {
    object(options);
    const available = JSON.parse(native.traceCategories());
    const legacy = "categoryFilter" in options || "traceOptions" in options;
    const allowed = legacy ? ["categoryFilter", "traceOptions"] : ["included_categories", "excluded_categories", "recording_mode"];
    for (const key of Reflect.ownKeys(options)) {
      if (!allowed.includes(key)) {
        unsupported(`Tracing option ${String(key)}`);
      }
    }
    let included = ["*"];
    let excluded = [];
    let mode = options.recording_mode ?? "record-until-full";
    if (legacy) {
      const filter = options.categoryFilter ?? "*";
      const flags = options.traceOptions ?? "record-until-full";
      if (typeof filter !== "string" || typeof flags !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Legacy tracing options must be strings");
      }
      if (filter.length > 1024 || flags.length > 1024) {
        throw error("ERR_OUT_OF_RANGE", "Tracing options exceed budget");
      }
      included = [];
      for (const entry of filter.split(",").map((part) => part.trim()).filter(Boolean)) {
        if (entry.startsWith("-")) {
          excluded.push(entry.slice(1));
        } else {
          included.push(entry);
        }
      }
      if (!included.length) {
        included = ["*"];
      }
      const modes = flags.split(",").map((part) => part.trim()).filter(Boolean);
      if (modes.length > 1) {
        unsupported(`Tracing flags ${flags}`);
      }
      mode = modes[0] ?? "record-until-full";
    } else {
      included = options.included_categories ?? included;
      excluded = options.excluded_categories ?? excluded;
    }
    if (!["record-until-full", "record-continuously"].includes(mode)) {
      unsupported(`Tracing recording mode ${mode}`);
    }
    return JSON.stringify({
      schema: 1,
      continuous: mode === "record-continuously",
      included: categories(included, available),
      excluded: categories(excluded, available)
    });
  }
  return {
    async getCategories() {
      ready();
      return JSON.parse(native.traceCategories());
    },
    async startRecording(options) {
      ready();
      const wire = config(options);
      if (state === "recording") {
        return;
      }
      if (state !== "idle") {
        throw error("ERR_FIREFLY_TRACE_BUSY", "Finish saving the previous recording first");
      }
      native.traceStart(wire);
      state = "recording";
    },
    async getTraceBufferUsage() {
      ready();
      const value = native.traceUsage();
      return { value, percentage: value * 100 };
    },
    async stopRecording(resultFilePath) {
      ready();
      if (resultFilePath !== void 0 && typeof resultFilePath !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Trace output path must be a string");
      }
      if (resultFilePath && (resultFilePath.length > 32768 || resultFilePath.includes("\0"))) {
        throw error("ERR_INVALID_ARG_VALUE", "Invalid trace output path");
      }
      if (state === "idle") {
        throw error("ERR_FIREFLY_TRACE_NOT_RECORDING", "No recording to stop");
      }
      if (state === "saving") {
        throw error("ERR_FIREFLY_TRACE_BUSY", "Trace output is already being saved");
      }
      if (state === "recording") {
        pending = native.traceFinish();
        state = "pending";
      }
      state = "saving";
      try {
        if (!resultFilePath && !temporary) {
          temporary = native.traceTemporaryPath();
        }
        const output = resultFilePath ? paths.resolve(resultFilePath) : temporary;
        await files.promises.writeFile(output, pending, {
          encoding: "utf8",
          mode: 384,
          flag: resultFilePath ? "w" : "wx"
        });
        pending = void 0;
        temporary = void 0;
        state = "idle";
        return output;
      } catch (failure) {
        state = "pending";
        throw failure;
      }
    }
  };
}

// firefly/core/app/command-line.mjs
function createCommandLine(native, error, unsupported) {
  function name(value) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", "Switch name must be a string");
    }
    if (!/^[a-z0-9][a-z0-9-]{0,127}$/.test(value)) {
      throw error(
        "ERR_INVALID_ARG_VALUE",
        "Switch name must be lowercase ASCII without leading dashes"
      );
    }
    return value;
  }
  return Object.freeze({
    hasSwitch(key) {
      return native.commandLineRead(name(key), true);
    },
    getSwitchValue(key) {
      return native.commandLineRead(name(key), false);
    },
    appendSwitch(key, value = "") {
      name(key);
      if (typeof value !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Switch value must be a string");
      }
      if (value.length > 32768 || value.includes("\0")) {
        throw error("ERR_INVALID_ARG_VALUE", "Invalid switch value");
      }
      native.commandLineChange(key, value);
    },
    removeSwitch(key) {
      native.commandLineChange(name(key), null);
    },
    appendArgument() {
      unsupported("commandLine.appendArgument");
    }
  });
}

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
          Object.freeze({
            url: request.url,
            method: request.method,
            ...typeof request.origin === "string" ? { headers: Object.freeze({ origin: request.origin }) } : {}
          }),
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
        let cors = false;
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
          } else if (key === "corsEnabled") {
            cors = value;
          } else if (key === "supportFetchAPI") {
            fetch = value;
          } else if (value) {
            unsupported(`protocol privilege ${key}`);
          }
        }
        if (secure && !standard) {
          unsupported("secure nonstandard protocol origins");
        }
        if (cors && !standard) {
          unsupported("CORS for nonstandard custom protocols");
        }
        if (fetch && !standard) {
          unsupported("fetch for nonstandard custom protocols");
        }
        if (fetch) {
          fetchSchemes.push(scheme);
        }
        schemes.push(
          cors ? { scheme, standard, secure, cors: true } : { scheme, standard, secure }
        );
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
    constructor() {
      super();
      this.on("error", () => {
      });
    }
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
  function senderEvent(window, data) {
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
    return { sender: window.webContents, senderFrame: frame };
  }
  function dispatchEvent(window, data) {
    let args;
    let channel;
    try {
      args = native.decodeIpc(data.wire);
      if (!Array.isArray(args) || args.length < 1 || args.length > 65) {
        throw Error("Invalid IPC event arguments");
      }
      channel = channelName(args.shift());
    } catch {
      console.warn("ERR_FIREFLY_IPC_SERIALIZATION: invalid event arguments");
      return;
    }
    ipcMain.emit(channel, senderEvent(window, data), ...args);
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
      if (record.once) {
        ipcHandlers.delete(channel);
      }
      const event = senderEvent(window, data);
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
  return { ipcMain, cancel: cancelIpc, dispatch: dispatchIpc, dispatchEvent };
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

// firefly/core/message-ports/index.mjs
function createMessageChannels(native, EventEmitter, error) {
  const states = /* @__PURE__ */ new WeakMap();
  const apply = Reflect.apply;
  const weakSet = WeakMap.prototype.set;
  const weakGet = WeakMap.prototype.get;
  const define = Object.defineProperty;
  const shift = Array.prototype.shift;
  const isArray = Array.isArray;
  const descriptor = Object.getOwnPropertyDescriptor;
  const Weak = WeakRef;
  const deref = Weak.prototype.deref;
  const Registry = FinalizationRegistry;
  const register = Registry.prototype.register;
  const unregister = Registry.prototype.unregister;
  const records = /* @__PURE__ */ new Map();
  const token = Symbol("MessagePortMain constructor");
  const schedule = setImmediate;
  const tick = process.nextTick;
  const byteLength = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Uint8Array.prototype),
    "byteLength"
  ).get;
  let sequence = 0;
  let totalBytes = 0;
  let stopped = false;
  function append(array, value) {
    define(array, `${array.length}`, {
      value,
      writable: true,
      enumerable: true,
      configurable: true
    });
  }
  function state(port) {
    const value = apply(weakGet, states, [port]);
    if (!value) {
      throw error("ERR_INVALID_THIS", "Invalid MessagePortMain receiver");
    }
    return value;
  }
  function release(lease) {
    if (lease.released) {
      return;
    }
    lease.released = true;
    totalBytes -= lease.bytes;
    lease.bytes = 0;
    lease.count = 0;
    records.delete(lease.id);
  }
  const finalizers = new Registry(({ id, generation }) => {
    const entry = records.get(id);
    if (!entry || entry.lease.generation !== generation) {
      return;
    }
    const record = apply(deref, entry.weak, []);
    if (record) {
      close(record, true);
    } else {
      release(entry.lease);
    }
  });
  function shell() {
    const port = new MessagePortMain(token);
    return { port, owner: new Weak(port), gcToken: {} };
  }
  function attach(record, staged) {
    apply(weakSet, states, [staged.port, { record, detached: false }]);
    record.owner = staged.owner;
    record.gcToken = staged.gcToken;
    apply(register, finalizers, [
      staged.port,
      { id: record.lease.id, generation: record.lease.generation },
      staged.gcToken
    ]);
    return staged.port;
  }
  function notify(record, retainedOwner) {
    if (stopped || record.notified) {
      return;
    }
    record.notified = true;
    (retainedOwner ?? apply(deref, record.owner, []))?.emit("close");
  }
  function consume(record, entry) {
    record.lease.count--;
    record.lease.bytes -= entry.size;
    totalBytes -= entry.size;
  }
  function discard(record) {
    const pending = record.queue;
    record.queue = [];
    for (const entry of pending) {
      if (entry.close) {
        continue;
      }
      consume(record, entry);
      for (const port of entry.ports) {
        const owned = apply(weakGet, states, [port]);
        if (owned?.record) {
          close(owned.record, true);
        }
      }
    }
  }
  function close(record, local) {
    if (record.closed) {
      return;
    }
    record.closed = true;
    const peer = record.peer;
    record.peer = null;
    if (peer && peer.peer === record) {
      peer.peer = null;
    }
    apply(unregister, finalizers, [record.gcToken]);
    discard(record);
    release(record.lease);
    if (!stopped) {
      if (local) {
        const owner = apply(deref, record.owner, []);
        tick(() => notify(record, owner));
      } else {
        notify(record);
      }
      if (peer && !peer.closed) {
        append(peer.queue, { close: true });
        pump(peer);
      }
    }
  }
  function pump(record) {
    if (stopped || record.closed || !record.started || record.scheduled || !record.queue.length) {
      return;
    }
    record.scheduled = true;
    const generation = record.lease.generation;
    schedule(() => {
      if (record.lease.generation !== generation) {
        return;
      }
      record.scheduled = false;
      if (stopped || record.closed || !record.started || !record.queue.length) {
        return;
      }
      const owner = apply(deref, record.owner, []);
      if (!owner) {
        close(record, true);
        return;
      }
      const entry = apply(shift, record.queue, []);
      if (entry.close) {
        close(record, false);
        return;
      }
      consume(record, entry);
      let data;
      try {
        data = native.decodeIpc(entry.wire);
      } catch (failure) {
        for (const port of entry.ports) {
          const owned = apply(weakGet, states, [port]);
          if (owned?.record) {
            close(owned.record, true);
          }
        }
        close(record, true);
        throw failure;
      }
      try {
        owner.emit("message", { data, ports: entry.ports });
      } finally {
        if (record.lease.generation === generation) {
          pump(record);
        }
      }
    });
  }
  function sweep() {
    for (const entry of [...records.values()]) {
      const record = apply(deref, entry.weak, []);
      if (!record) {
        release(entry.lease);
      } else if (!apply(deref, record.owner, [])) {
        close(record, true);
      }
    }
  }
  function endpoint() {
    if (sequence === Number.MAX_SAFE_INTEGER) {
      throw error("ERR_FIREFLY_PORT_LIMIT", "Port identity space exhausted");
    }
    const lease = { id: ++sequence, generation: 1, bytes: 0, count: 0, released: false };
    const record = {
      lease,
      queue: [],
      peer: null,
      owner: null,
      gcToken: null,
      started: false,
      closed: false,
      notified: false,
      scheduled: false
    };
    records.set(lease.id, { weak: new Weak(record), lease });
    return record;
  }
  function transfers(value, source, destination) {
    if (value === void 0) {
      return [];
    }
    if (!isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", "Port transfer list must be an array");
    }
    const length = value.length;
    if (!Number.isSafeInteger(length) || length < 0 || length > 16) {
      throw error("ERR_FIREFLY_PORT_LIMIT", "At most 16 ports may be transferred");
    }
    const moves = [];
    for (let index = 0; index < length; index++) {
      const itemDescriptor = descriptor(value, String(index));
      if (!itemDescriptor || !Object.hasOwn(itemDescriptor, "value")) {
        throw error("ERR_INVALID_ARG_TYPE", "Port transfer entries must be own data values");
      }
      const port = itemDescriptor.value;
      const owned = apply(weakGet, states, [port]);
      if (!owned || owned.detached || !owned.record) {
        throw error("ERR_INVALID_ARG_TYPE", "Transfer entry is not an owned MessagePortMain");
      }
      const record = owned.record;
      if (record === source) {
        throw error("ERR_FIREFLY_PORT_TRANSFER", "Cannot transfer the source port");
      }
      if (record === destination || record.closed) {
        throw error(
          "ERR_FIREFLY_UNSUPPORTED_API",
          "Closed or receiving endpoint transfer is not implemented"
        );
      }
      let duplicate = false;
      for (let slot = 0; slot < moves.length; slot++) {
        if (moves[slot].record === record) {
          duplicate = true;
        }
      }
      if (duplicate) {
        throw error("ERR_FIREFLY_PORT_TRANSFER", "Duplicate port transfer");
      }
      if (record.lease.generation === Number.MAX_SAFE_INTEGER) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "Port ownership generation exhausted");
      }
      append(moves, { port, owned, record, generation: record.lease.generation });
    }
    return moves;
  }
  class MessagePortMain extends EventEmitter {
    constructor(key) {
      super();
      if (key !== token) {
        throw error("ERR_ILLEGAL_CONSTRUCTOR", "MessagePortMain cannot be constructed directly");
      }
    }
    postMessage(message, transfer) {
      const owned = state(this);
      const record = owned.record;
      if (stopped || owned.detached || !record || record.closed) {
        return;
      }
      const destination = record.peer;
      const generation = record.lease.generation;
      const moves = transfers(transfer, record, destination);
      let wire;
      try {
        wire = native.encodeIpc(message);
      } catch {
        throw error(
          "ERR_FIREFLY_PORT_SERIALIZATION",
          "Port message requires the supported bounded data/binary graph"
        );
      }
      const size = apply(byteLength, wire, []);
      if (destination && !destination.closed && (destination.lease.count >= 64 || destination.lease.bytes + size > 1048576 || totalBytes + size > 4194304)) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "Port queue budget exceeded");
      }
      const staged = [];
      for (let slot = 0; slot < moves.length; slot++) {
        append(staged, shell());
      }
      if (owned.record !== record || owned.detached || record.closed || record.lease.generation !== generation || record.peer !== destination) {
        throw error("ERR_FIREFLY_PORT_TRANSFER", "Source changed while preparing a message");
      }
      for (const move of moves) {
        if (move.owned.record !== move.record || move.owned.detached || move.record.closed || move.record.lease.generation !== move.generation) {
          throw error(
            "ERR_FIREFLY_PORT_TRANSFER",
            "Transfer ownership changed while preparing a message"
          );
        }
      }
      if (!destination || destination.closed) {
        if (moves.length) {
          throw error(
            "ERR_FIREFLY_UNSUPPORTED_API",
            "Transfer to a disconnected endpoint is not implemented"
          );
        }
        return;
      }
      if (destination.lease.count >= 64 || destination.lease.bytes + size > 1048576 || totalBytes + size > 4194304) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "Port queue budget exceeded");
      }
      const received = [];
      for (let index = 0; index < moves.length; index++) {
        const move = moves[index];
        const target = move.record;
        apply(unregister, finalizers, [target.gcToken]);
        move.owned.detached = true;
        move.owned.record = null;
        target.lease.generation++;
        target.started = false;
        target.scheduled = false;
        append(received, attach(target, staged[index]));
      }
      append(destination.queue, { wire, size, ports: received });
      destination.lease.count++;
      destination.lease.bytes += size;
      totalBytes += size;
      pump(destination);
    }
    start() {
      const owned = state(this);
      if (!stopped && !owned.detached && owned.record && !owned.record.closed) {
        owned.record.started = true;
        pump(owned.record);
      }
    }
    close() {
      const owned = state(this);
      if (!owned.detached && owned.record) {
        close(owned.record, true);
      }
    }
  }
  class MessageChannelMain {
    constructor() {
      if (new.target !== MessageChannelMain) {
        throw error(
          "ERR_FIREFLY_UNSUPPORTED_API",
          "MessageChannelMain subclassing is not implemented"
        );
      }
      if (stopped) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application has exited");
      }
      const firstOwner = shell();
      const secondOwner = shell();
      if (stopped) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application exited during port construction");
      }
      sweep();
      if (records.size + 2 > 256) {
        throw error("ERR_FIREFLY_PORT_LIMIT", "At most 256 live main endpoints are supported");
      }
      const first = endpoint();
      const second = endpoint();
      first.peer = second;
      second.peer = first;
      Object.defineProperties(this, {
        port1: { value: attach(first, firstOwner), enumerable: true },
        port2: { value: attach(second, secondOwner), enumerable: true }
      });
    }
  }
  function shutdown() {
    stopped = true;
    for (const entry of [...records.values()]) {
      const record = apply(deref, entry.weak, []);
      if (record) {
        close(record, true);
      } else {
        release(entry.lease);
      }
    }
  }
  return { MessageChannelMain, shutdown };
}

// firefly/core/utility-process/index.mjs
function createUtilityProcesses(native, codec, EventEmitter, Readable, app, paths) {
  const owned = /* @__PURE__ */ new Set();
  let stopped = false;
  function fail(code, message) {
    throw Object.assign(new Error(message), { code });
  }
  function unsupported(name) {
    fail("ERR_FIREFLY_UNSUPPORTED_API", `${name} is not implemented`);
  }
  function text(value, name) {
    if (typeof value !== "string" || value.includes("\0")) {
      fail("ERR_INVALID_ARG_TYPE", `${name} must be a string without NUL`);
    }
    return value;
  }
  function strings(value, name) {
    if (!Array.isArray(value) || value.length > 1024) {
      fail("ERR_INVALID_ARG_TYPE", `${name} must be a bounded string array`);
    }
    return value.map((item) => text(item, name));
  }
  function modes(value = "inherit") {
    const result = typeof value === "string" ? ["ignore", value, value] : value;
    if (!Array.isArray(result) || result.length !== 3 || result[0] !== "ignore") {
      unsupported("utilityProcess stdin/additional descriptors");
    }
    for (const mode of result) {
      if (!["ignore", "inherit", "pipe"].includes(mode)) {
        unsupported(`utilityProcess stdio ${mode}`);
      }
    }
    return result.slice();
  }
  function configuration(modulePath, args, options) {
    text(modulePath, "modulePath");
    if (!modulePath) {
      fail("ERR_INVALID_ARG_VALUE", "modulePath must not be empty");
    }
    if (!options || typeof options !== "object" || Array.isArray(options)) {
      fail("ERR_INVALID_ARG_TYPE", "Utility options must be an object");
    }
    const supported = /* @__PURE__ */ new Set([
      "env",
      "cwd",
      "stdio",
      "execArgv",
      "serviceName",
      "allowLoadingUnsignedLibraries",
      "respondToAuthRequestsFromMainProcess"
    ]);
    for (const key of Object.keys(options)) {
      if (!supported.has(key)) {
        unsupported(`utilityProcess option ${key}`);
      }
    }
    if (options.execArgv !== void 0 && strings(options.execArgv, "execArgv").length) {
      unsupported("utilityProcess engine command-line flags");
    }
    for (const key of ["allowLoadingUnsignedLibraries", "respondToAuthRequestsFromMainProcess"]) {
      if (options[key] !== void 0 && options[key] !== false) {
        unsupported(`utilityProcess ${key}`);
      }
    }
    if (options.serviceName !== void 0) {
      unsupported("Utility service naming and app process metrics");
    }
    const env = options.env ?? process.env;
    if (!env || typeof env !== "object" || Array.isArray(env)) {
      fail("ERR_INVALID_ARG_TYPE", "env must be an object");
    }
    const environment = [];
    for (const key of Object.keys(env)) {
      text(key, "environment key");
      if (!key || key.includes("=")) {
        fail("ERR_INVALID_ARG_VALUE", "Invalid environment key");
      }
      if (env[key] !== void 0) {
        environment.push(`${key}=${text(env[key], "environment value")}`);
      }
    }
    return {
      file: process.execPath,
      entry: paths.resolve(modulePath),
      args: strings(args, "args"),
      cwd: options.cwd === void 0 ? "" : text(options.cwd, "cwd"),
      env: environment,
      stdio: modes(options.stdio)
    };
  }
  function output() {
    return new Readable({ read() {
    } });
  }
  const authority = /* @__PURE__ */ new WeakMap();
  const token = {};
  class UtilityProcess extends EventEmitter {
    constructor(key, spec) {
      super();
      if (key !== token) {
        unsupported("Direct UtilityProcess construction");
      }
      const state = {
        id: 0,
        pid: void 0,
        exited: false,
        killed: false,
        stdout: null,
        stderr: null
      };
      authority.set(this, state);
      state.stdout = spec.stdio[1] === "pipe" ? output() : null;
      state.stderr = spec.stdio[2] === "pipe" ? output() : null;
      owned.add(this);
      let result;
      try {
        result = native.spawn(spec, (kind, id, value) => {
          if (stopped || id !== state.id) {
            return;
          }
          if (kind === "spawn") {
            state.pid = value;
            this.emit("spawn");
          } else if (kind === "stdout" || kind === "stderr") {
            const stream = state[kind];
            if (stream && !stream.destroyed && !stream.push(value) && stream.readableLength > 1048576) {
              stream.destroy();
              native.kill(state.id);
              this.emit(
                "error",
                Object.assign(new Error("Utility output exceeds its queue budget"), {
                  code: "ERR_FIREFLY_UTILITY_LIMIT"
                })
              );
            }
          } else if (kind === "stdoutEnd" || kind === "stderrEnd") {
            const name = kind === "stdoutEnd" ? "stdout" : "stderr";
            state[name]?.push(null);
            state[name] = null;
            if (state.exited && !state.stdout && !state.stderr) {
              owned.delete(this);
            }
          } else if (kind === "data") {
            this.emit("message", codec.decodeIpc(value));
          } else if (kind === "exit") {
            state.exited = true;
            state.pid = void 0;
            this.emit("exit", value);
            if (!state.stdout && !state.stderr) {
              owned.delete(this);
            }
          } else if (kind === "error") {
            this.emit(
              "error",
              Object.assign(new Error(String(value)), {
                code: "ERR_FIREFLY_UTILITY_CHANNEL"
              })
            );
          }
        });
      } catch (failure) {
        owned.delete(this);
        throw failure;
      }
      state.id = result.id;
      if (result.status) {
        owned.delete(this);
        state.exited = true;
        state.stdout?.destroy();
        state.stderr?.destroy();
        state.stdout = state.stderr = null;
        fail("ERR_FIREFLY_UTILITY_SPAWN", `Utility spawn failed (${result.status})`);
      }
    }
    get pid() {
      return authority.get(this).pid;
    }
    get stdout() {
      return authority.get(this).stdout;
    }
    get stderr() {
      return authority.get(this).stderr;
    }
    postMessage(message, transfer) {
      const state = authority.get(this);
      if (stopped || state.exited) {
        return;
      }
      if (transfer !== void 0 && (!Array.isArray(transfer) || transfer.length)) {
        unsupported("Utility native MessagePort transfer");
      }
      const wire = codec.encodeIpc(message);
      const status = native.send(state.id, wire);
      if (status) {
        fail("ERR_FIREFLY_UTILITY_CHANNEL", `Utility channel send failed (${status})`);
      }
    }
    kill() {
      const state = authority.get(this);
      if (stopped || state.exited || state.killed) {
        return false;
      }
      state.killed = native.kill(state.id);
      return state.killed;
    }
  }
  function fork(modulePath, args = [], options = {}) {
    if (!app.isReady()) {
      fail("ERR_FIREFLY_APP_NOT_READY", "utilityProcess.fork requires app ready");
    }
    if (stopped) {
      fail("ERR_FIREFLY_APP_EXITED", "Application has exited");
    }
    if (!native || !Readable) {
      unsupported("Native utility processes in this host");
    }
    return new UtilityProcess(token, configuration(modulePath, args, options));
  }
  function shutdown() {
    if (stopped) {
      return;
    }
    stopped = true;
    native?.shutdown();
    for (const child of owned) {
      const state = authority.get(child);
      state.exited = true;
      state.pid = void 0;
      state.stdout?.destroy();
      state.stderr?.destroy();
      state.stdout = state.stderr = null;
    }
    owned.clear();
  }
  return { utilityProcess: { fork }, shutdown };
}

// firefly/core/safe-storage/index.mjs
function createSafeStorage(native, app, error, unsupported) {
  const BufferClass = globalThis.Buffer;
  const isBuffer = BufferClass.isBuffer.bind(BufferClass);
  const byteLength = BufferClass.byteLength.bind(BufferClass);
  const from = BufferClass.from.bind(BufferClass);
  const limit = 1024 * 1024;
  const apply = Reflect.apply;
  const viewLength = Object.getOwnPropertyDescriptor(
    Object.getPrototypeOf(Uint8Array.prototype),
    "byteLength"
  ).get;
  function invalid(message) {
    return Object.assign(new TypeError(message), { code: "ERR_INVALID_ARG_TYPE" });
  }
  function ready() {
    if (!app.isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "safeStorage requires app readiness");
    }
  }
  function available() {
    return app.isReady() && typeof native.safeStorageAvailable === "function" && native.safeStorageAvailable();
  }
  function requireEncryption() {
    ready();
    if (!available()) {
      throw error("ERR_FIREFLY_SAFE_STORAGE_UNAVAILABLE", "OS-backed encryption is unavailable");
    }
  }
  const storage = {
    isEncryptionAvailable: available,
    encryptString(plainText) {
      if (typeof plainText !== "string") {
        throw invalid("safeStorage.encryptString requires a string");
      }
      if (plainText.length > limit || byteLength(plainText, "utf8") > limit) {
        throw error("ERR_OUT_OF_RANGE", "safeStorage plaintext exceeds its byte budget");
      }
      requireEncryption();
      return from(native.safeStorageEncrypt(from(plainText, "utf8")));
    },
    decryptString(encrypted) {
      requireEncryption();
      if (!isBuffer(encrypted)) {
        throw invalid("safeStorage.decryptString requires a Buffer");
      }
      const length = apply(viewLength, encrypted, []);
      if (length > limit + 64) {
        throw error("ERR_OUT_OF_RANGE", "safeStorage ciphertext exceeds its byte budget");
      }
      if (length === 0) {
        return "";
      }
      return from(native.safeStorageDecrypt(encrypted)).toString("utf8");
    },
    setUsePlainTextEncryption(value) {
      if (typeof value !== "boolean") {
        throw invalid("Plaintext policy must be a boolean");
      }
      if (value) {
        unsupported("Plaintext key storage");
      }
    },
    isAsyncEncryptionAvailable() {
      return Promise.resolve(false);
    },
    encryptStringAsync() {
      return Promise.reject(
        error("ERR_FIREFLY_UNSUPPORTED_API", "Async OS encryption is not implemented")
      );
    },
    decryptStringAsync() {
      return Promise.reject(
        error("ERR_FIREFLY_UNSUPPORTED_API", "Async OS decryption is not implemented")
      );
    }
  };
  if (process.platform === "linux") {
    storage.getSelectedStorageBackend = () => "unknown";
  }
  return storage;
}

// firefly/core/views/index.mjs
function createViews(native, EventEmitter, makeContents, validatePreferences, requireReady, error, unsupported) {
  const records = /* @__PURE__ */ new Map();
  const endpoints = /* @__PURE__ */ new WeakMap();
  const contentsOwners = /* @__PURE__ */ new WeakMap();
  const roots = /* @__PURE__ */ new WeakMap();
  let stopped = false;
  function alive(host) {
    if (stopped || host._destroyed) {
      throw error("ERR_FIREFLY_OBJECT_DESTROYED", "View has been destroyed");
    }
  }
  function owned(view) {
    const host = endpoints.get(view);
    if (!host) {
      throw error("ERR_INVALID_THIS", "An owned WebContentsView is required");
    }
    alive(host);
    return host;
  }
  function windowAlive(win) {
    if (stopped || win._destroyed || win._closing) {
      throw error("ERR_FIREFLY_OBJECT_DESTROYED", "Parent window has been destroyed");
    }
  }
  function shape(value, allowed, name) {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", `${name} must be an object`);
    }
    for (const key of Reflect.ownKeys(value)) {
      if (!allowed.includes(key)) {
        unsupported(`${name} option ${String(key)}`);
      }
      const descriptor = Object.getOwnPropertyDescriptor(value, key);
      if (!descriptor || !Object.hasOwn(descriptor, "value")) {
        throw error("ERR_INVALID_ARG_TYPE", `${name} requires own data properties`);
      }
    }
  }
  function rectangle(value) {
    const keys = ["x", "y", "width", "height"];
    shape(value, keys, "View bounds");
    const result = {};
    for (const key of keys) {
      const item = Object.getOwnPropertyDescriptor(value, key)?.value;
      if (typeof item !== "number" || !Number.isFinite(item)) {
        throw error("ERR_INVALID_ARG_TYPE", `View bounds ${key} must be finite`);
      }
      result[key] = Math.round(key === "width" || key === "height" ? Math.max(0, item) : item);
    }
    if (result.width > 16384 || result.height > 16384 || result.x < -2147483648 || result.y < -2147483648 || result.x > 2147483647 || result.y > 2147483647 || result.x + result.width > 2147483647 || result.y + result.height > 2147483647) {
      throw error("ERR_INVALID_ARG_VALUE", "View bounds exceed native coordinate limits");
    }
    return result;
  }
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
        unsupported("WebContentsView subclassing");
      }
      requireReady();
      if (stopped) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application has exited");
      }
      shape(options, ["webPreferences"], "WebContentsView");
      const prefs = validatePreferences(options.webPreferences ?? {});
      const host = new EventEmitter();
      requireReady();
      if (stopped) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application has exited");
      }
      Object.defineProperty(host, "id", {
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
        native.closeContents(host.id);
        throw failure;
      }
      records.set(host.id, host);
      endpoints.set(this, host);
      contentsOwners.set(host.webContents, host);
      Object.defineProperty(this, "webContents", {
        enumerable: true,
        get: () => host._destroyed ? void 0 : host.webContents
      });
    }
    setBounds(bounds, options) {
      const host = owned(this);
      if (options !== void 0) {
        unsupported("View bounds animation/options");
      }
      const value = rectangle(bounds);
      native.setViewBounds(host.id, value.x, value.y, value.width, value.height);
    }
    getBounds() {
      const host = owned(this);
      const raw = native.viewBounds(host.id);
      const value = typeof raw === "string" ? JSON.parse(raw) : raw;
      return { x: value[0], y: value[1], width: value[2], height: value[3] };
    }
    setVisible(value) {
      const host = owned(this);
      if (typeof value !== "boolean") {
        throw error("ERR_INVALID_ARG_TYPE", "View visibility must be boolean");
      }
      native.setViewVisible(host.id, value);
    }
    getVisible() {
      return native.viewVisible(owned(this).id);
    }
    setBackgroundColor() {
      owned(this);
      unsupported("View background color");
    }
    setBorderRadius() {
      owned(this);
      unsupported("View border radius");
    }
    addChildView() {
      owned(this);
      unsupported("Nested child views");
    }
    removeChildView() {
      owned(this);
      unsupported("Nested child views");
    }
    get children() {
      owned(this);
      return [];
    }
  }
  function contentView(win) {
    const existing = roots.get(win);
    if (existing) {
      return existing.root;
    }
    const state = { root: new EventEmitter(), children: [] };
    const root = state.root;
    Object.defineProperty(root, "children", {
      enumerable: true,
      get() {
        windowAlive(win);
        if (typeof native.childViewIds === "function") {
          const ids = JSON.parse(native.childViewIds(win.id));
          return ids.map((id) => {
            const child = records.get(id);
            if (!child || child._destroyed || child.owner !== win) {
              throw error("ERR_FIREFLY_VIEW_STATE", "Native child ownership is inconsistent");
            }
            return child.view;
          });
        }
        return state.children.slice();
      }
    });
    root.addChildView = (view, index) => {
      windowAlive(win);
      const host = owned(view);
      if (host._closing) {
        throw error("ERR_FIREFLY_OBJECT_DESTROYED", "Child contents are closing");
      }
      if (index !== void 0 && (!Number.isInteger(index) || index < 0)) {
        throw error("ERR_INVALID_ARG_VALUE", "Child index must be a nonnegative integer");
      }
      const count = state.children.length - (host.owner === win ? 1 : 0);
      const position = index === void 0 ? count : Math.min(index, count);
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
    for (const method of ["addChildView", "removeChildView"]) {
      Object.defineProperty(root, method, { value: root[method], enumerable: true });
    }
    for (const method of [
      "setBounds",
      "getBounds",
      "setVisible",
      "getVisible",
      "setBackgroundColor",
      "setBorderRadius"
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
    return !stopped && host && !host._destroyed && !host._closing && win && !win._destroyed && !win._closing ? win : null;
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
    host.webContents.emit("destroyed");
    host.emit("closed");
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

// firefly/core/window/preferences.mjs
function createWebPreferences(paths, isSandboxRequired, error, unsupported) {
  return function validate(prefs = {}) {
    if (!prefs || typeof prefs !== "object" || Array.isArray(prefs)) {
      throw error("ERR_INVALID_ARG_TYPE", "webPreferences must be an object");
    }
    for (const key of Object.keys(prefs)) {
      if (key === "preload" || key === "additionalArguments") {
        continue;
      }
      if (!["contextIsolation", "nodeIntegration", "sandbox"].includes(key)) {
        unsupported(`webPreferences.${key}`);
      }
      if (key === "sandbox" && isSandboxRequired() && typeof prefs[key] === "boolean") {
        continue;
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
    const additionalArguments = prefs.additionalArguments === void 0 ? [] : prefs.additionalArguments;
    if (!Array.isArray(additionalArguments) || additionalArguments.some((value) => typeof value !== "string")) {
      throw error("ERR_INVALID_ARG_TYPE", "additionalArguments must be an array of strings");
    }
    if (additionalArguments.length > 64) {
      throw error("ERR_INVALID_ARG_VALUE", "additionalArguments exceeds 64 entries");
    }
    return { preload: prefs.preload, additionalArguments };
  };
}

// firefly/core/native-image/index.mjs
function createNativeImages(error, unsupported) {
  const alloc = globalThis.Buffer.alloc.bind(globalThis.Buffer);
  const token = Symbol("NativeImage");
  class NativeImage {
    #valid = true;
    #check() {
      return this.#valid;
    }
    constructor(key) {
      if (key !== token) {
        throw error("ERR_ILLEGAL_CONSTRUCTOR", "Use nativeImage factories");
      }
    }
    isEmpty() {
      this.#check();
      return true;
    }
    getSize() {
      this.#check();
      return { width: 0, height: 0 };
    }
    getAspectRatio() {
      this.#check();
      return 1;
    }
    getScaleFactors() {
      this.#check();
      return [];
    }
    toPNG() {
      this.#check();
      return alloc(0);
    }
    toJPEG() {
      this.#check();
      return alloc(0);
    }
    toBitmap() {
      this.#check();
      return alloc(0);
    }
    getBitmap() {
      this.#check();
      return alloc(0);
    }
    toDataURL() {
      this.#check();
      return "data:image/png;base64,";
    }
    crop() {
      this.#check();
      return new NativeImage(token);
    }
    resize() {
      this.#check();
      return new NativeImage(token);
    }
    isTemplateImage() {
      this.#check();
      return false;
    }
    setTemplateImage() {
      this.#check();
    }
    addRepresentation() {
      this.#check();
      unsupported("NativeImage.addRepresentation");
    }
    getNativeHandle() {
      this.#check();
      unsupported("NativeImage.getNativeHandle");
    }
  }
  const createEmpty = () => new NativeImage(token);
  const nativeImage = { createEmpty };
  for (const name of [
    "createFromPath",
    "createFromBuffer",
    "createFromDataURL",
    "createFromBitmap",
    "createThumbnailFromPath",
    "createFromNamedImage"
  ]) {
    nativeImage[name] = () => unsupported(`nativeImage.${name}`);
  }
  nativeImage.createThumbnailFromPath = async () => unsupported("nativeImage.createThumbnailFromPath");
  return { nativeImage, createEmpty };
}

// firefly/core/desktop-capture/index.mjs
function createDesktopCapture(native, isReady, isExited, error, unsupported, createEmpty) {
  const pending = /* @__PURE__ */ new Map();
  const limit = 8;
  const deadline = 3e4;
  const byteLength = globalThis.Buffer.byteLength.bind(globalThis.Buffer);
  let next = 0;
  let stopped = false;
  function active() {
    if (stopped || isExited()) {
      throw error("ERR_FIREFLY_APP_EXITED", "Desktop capture application has exited");
    }
    if (!isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "desktopCapturer requires app readiness");
    }
  }
  function options(value) {
    if (!value || typeof value !== "object") {
      throw new Error("Invalid options");
    }
    const types = value.types;
    if (!Array.isArray(types)) {
      throw new Error("Invalid options");
    }
    const length = types.length;
    if (length > 64) {
      throw error("ERR_OUT_OF_RANGE", "Too many desktop capture types");
    }
    const selected = /* @__PURE__ */ new Set();
    for (let index = 0; index < length; ++index) {
      const type = types[index];
      if (type !== "screen" && type !== "window") {
        throw new Error("Invalid options");
      }
      selected.add(type);
    }
    const size = value.thumbnailSize;
    let width = 150;
    let height = 150;
    if (size !== void 0) {
      if (!size || typeof size !== "object") {
        throw new Error("Invalid options");
      }
      width = size.width;
      height = size.height;
      if (!Number.isInteger(width) || !Number.isInteger(height) || width < 0 || height < 0 || width > 16384 || height > 16384) {
        throw new Error("Invalid options");
      }
    }
    if (width !== 0 && height !== 0) {
      unsupported("desktopCapturer thumbnails");
    }
    const icons = value.fetchWindowIcons;
    if (icons !== void 0 && typeof icons !== "boolean") {
      throw new Error("Invalid options");
    }
    if (icons) {
      unsupported("desktopCapturer window icons");
    }
    for (const key of Object.keys(value)) {
      if (!["types", "thumbnailSize", "fetchWindowIcons"].includes(key)) {
        unsupported(`desktopCapturer option ${key}`);
      }
    }
    return selected;
  }
  function cancel(id) {
    try {
      native.desktopCaptureCancel(id);
    } catch {
    }
  }
  async function getSources(value) {
    const types = options(value);
    active();
    if (pending.size >= limit) {
      throw error("ERR_FIREFLY_CAPTURE_LIMIT", "Too many pending desktop capture requests");
    }
    if (next >= 2147483647) {
      throw error("ERR_FIREFLY_CAPTURE_LIMIT", "Desktop capture request IDs exhausted");
    }
    const id = ++next;
    return new Promise((resolve, reject) => {
      const timer = setTimeout(() => {
        if (!pending.delete(id)) {
          return;
        }
        cancel(id);
        reject(error("ERR_FIREFLY_CAPTURE_TIMEOUT", "OS source enumeration timed out"));
      }, deadline);
      pending.set(id, { resolve, reject, timer, types });
      try {
        native.desktopCaptureStart(id);
      } catch (failure) {
        pending.delete(id);
        clearTimeout(timer);
        reject(failure);
      }
    });
  }
  function snapshot(records, types) {
    if (!Array.isArray(records) || records.length > 1024) {
      throw error("ERR_FIREFLY_CAPTURE_DATA", "Invalid native desktop source list");
    }
    const sources = [];
    const seen = /* @__PURE__ */ new Set();
    let budget = 0;
    for (const record of records) {
      const id = record?.id;
      const name = record?.name;
      const display = record?.display_id;
      if (typeof id !== "string" || !/^(screen|window):[1-9][0-9]{0,19}:0$/.test(id) || typeof name !== "string" || name.includes("\0") || byteLength(name, "utf8") > 4096 || typeof display !== "string" || !/^(?:[1-9][0-9]{0,19})?$/.test(display) || seen.has(id)) {
        throw error("ERR_FIREFLY_CAPTURE_DATA", "Invalid native desktop source");
      }
      const kind = id.split(":")[0];
      if (kind === "screen" && (display === "" || display !== id.split(":")[1] || !id.endsWith(":0")) || kind === "window" && display !== "") {
        throw error("ERR_FIREFLY_CAPTURE_DATA", "Native source/display identity mismatch");
      }
      seen.add(id);
      budget += byteLength(name, "utf8") + id.length + display.length;
      if (budget > 1048576) {
        throw error("ERR_FIREFLY_CAPTURE_DATA", "Native desktop metadata exceeds its budget");
      }
      if (types.has(kind)) {
        sources.push({ id, name, display_id: display, thumbnail: createEmpty(), appIcon: null });
      }
    }
    return sources;
  }
  function dispatch(value) {
    const request = pending.get(value?.id);
    if (!request) {
      return;
    }
    pending.delete(value.id);
    clearTimeout(request.timer);
    try {
      active();
      if (value.code) {
        throw error(value.code, value.message || "Native source enumeration failed");
      }
      const sources = snapshot(value.sources, request.types);
      active();
      request.resolve(sources);
    } catch (failure) {
      request.reject(failure);
    }
  }
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const requests = [...pending];
    pending.clear();
    for (const [id, request] of requests) {
      clearTimeout(request.timer);
      cancel(id);
      request.reject(
        error("ERR_FIREFLY_APP_EXITED", "Application exited during source enumeration")
      );
    }
  }
  return { desktopCapturer: { getSources }, dispatch, stop };
}

// firefly/core/notifications/index.mjs
function createNotifications(native, EventEmitter, isReady, isExited, error, unsupported) {
  const records = /* @__PURE__ */ new WeakMap();
  const active = /* @__PURE__ */ new Map();
  const bytes = globalThis.Buffer.byteLength.bind(globalThis.Buffer);
  const stringify = JSON.stringify;
  let next = 0;
  let stopped = false;
  function running() {
    if (stopped || isExited()) {
      throw error("ERR_FIREFLY_APP_EXITED", "Notifications cannot run after app exit");
    }
    if (!isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "Notification requires app readiness");
    }
  }
  function invalid(message, code = "ERR_INVALID_ARG_TYPE") {
    return Object.assign(new TypeError(message), { code });
  }
  function text(value, name, limit) {
    if (typeof value !== "string" || value.includes("\0")) {
      throw invalid(`${name} must be a string without NUL`);
    }
    if (bytes(value, "utf8") > limit) {
      throw invalid(`${name} exceeds its UTF-8 byte limit`, "ERR_OUT_OF_RANGE");
    }
    return value;
  }
  function actions(value) {
    if (!Array.isArray(value)) {
      throw invalid("actions must be an array");
    }
    const length = value.length;
    if (length > 4) {
      throw invalid("At most four notification actions are supported", "ERR_OUT_OF_RANGE");
    }
    const result = [];
    for (let index = 0; index < length; ++index) {
      const action = value[index];
      if (!action || typeof action !== "object") {
        throw invalid("Each action must be an object");
      }
      const type = action.type;
      const label = action.text;
      if (type !== "button") {
        unsupported("Notification non-button actions");
      }
      for (const key of Object.keys(action)) {
        if (!["type", "text"].includes(key)) {
          unsupported(`Notification action option ${key}`);
        }
      }
      result.push({ type, text: text(label, "action.text", 1024) });
    }
    return result;
  }
  function properties(value) {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      throw invalid("Notification options must be an object");
    }
    for (const key of Object.keys(value)) {
      if (!["title", "body", "silent", "actions"].includes(key)) {
        unsupported(`Notification option ${key}`);
      }
    }
    const title = value.title;
    const body = value.body;
    const silent = value.silent;
    const buttons = value.actions;
    if (silent !== void 0 && typeof silent !== "boolean") {
      throw invalid("silent must be a boolean");
    }
    return {
      title: text(title === void 0 ? "" : title, "title", 4096),
      body: text(body === void 0 ? "" : body, "body", 65536),
      silent: silent ?? false,
      actions: buttons === void 0 ? [] : actions(buttons)
    };
  }
  function record(object) {
    const value = records.get(object);
    if (!value) {
      throw invalid("Illegal Notification receiver");
    }
    return value;
  }
  function retire(token, notify) {
    const entry = active.get(token);
    if (!entry) {
      return;
    }
    active.delete(token);
    if (entry.record.token === token) {
      entry.record.token = null;
    }
    const version = entry.record.version;
    try {
      native.notificationClose(token);
    } finally {
      if (notify && !stopped && !isExited() && entry.record.version === version) {
        entry.object.emit("close", {});
      }
    }
  }
  class Notification extends EventEmitter {
    constructor(options = {}) {
      super();
      running();
      const value = properties(options);
      running();
      records.set(this, { value, token: null, version: 0 });
    }
    static isSupported() {
      if (stopped || isExited() || !isReady() || typeof native.notificationSupported !== "function") {
        return false;
      }
      const supported = native.notificationSupported() === true;
      return supported && !stopped && !isExited() && isReady();
    }
    get title() {
      return record(this).value.title;
    }
    set title(value) {
      record(this).value.title = text(value, "title", 4096);
    }
    get body() {
      return record(this).value.body;
    }
    set body(value) {
      record(this).value.body = text(value, "body", 65536);
    }
    get silent() {
      return record(this).value.silent;
    }
    set silent(value) {
      if (typeof value !== "boolean") {
        throw invalid("silent must be a boolean");
      }
      record(this).value.silent = value;
    }
    get actions() {
      return record(this).value.actions.map((action) => ({ ...action }));
    }
    set actions(value) {
      record(this).value.actions = actions(value);
    }
    show() {
      running();
      const state = record(this);
      const version = ++state.version;
      if (state.token !== null) {
        retire(state.token, true);
      }
      running();
      if (state.version !== version) {
        return;
      }
      if (active.size >= 64 || next >= 2147483647) {
        throw error("ERR_FIREFLY_NOTIFICATION_LIMIT", "Notification quota or IDs exhausted");
      }
      const token = ++next;
      const value = {
        ...state.value,
        actions: state.value.actions.map((action) => ({ ...action }))
      };
      state.token = token;
      active.set(token, {
        object: this,
        record: state,
        shown: false,
        actionCount: value.actions.length
      });
      try {
        native.notificationShow(token, stringify(value));
      } catch (failure) {
        queueMicrotask(
          () => dispatch({ token, type: "failed", message: failure?.message ?? String(failure) })
        );
      }
    }
    close() {
      const state = record(this);
      ++state.version;
      if (state.token !== null) {
        retire(state.token, true);
      }
    }
  }
  for (const name of [
    "icon",
    "subtitle",
    "hasReply",
    "replyPlaceholder",
    "sound",
    "urgency",
    "timeoutType",
    "closeButtonText",
    "toastXml"
  ]) {
    Object.defineProperty(Notification.prototype, name, {
      get() {
        return unsupported(`Notification.${name}`);
      },
      set() {
        unsupported(`Notification.${name}`);
      }
    });
  }
  function dispatch(value) {
    const entry = active.get(value?.token);
    if (!entry || stopped || isExited()) {
      return;
    }
    const type = value.type;
    if (type === "show") {
      if (!entry.shown) {
        entry.shown = true;
        entry.object.emit("show", {});
      }
      return;
    }
    let event = type;
    let message = value.message;
    const index = value.index;
    if (!["click", "action", "close", "failed"].includes(type) || type === "action" && (!Number.isInteger(index) || index < 0 || index >= entry.actionCount) || type === "failed" && (typeof message !== "string" || bytes(message, "utf8") > 4096)) {
      event = "failed";
      message = "Invalid native notification event";
    }
    const version = entry.record.version;
    retire(value.token, false);
    if (stopped || isExited() || entry.record.version !== version) {
      return;
    }
    if (event === "action") {
      entry.object.emit("action", { actionIndex: index }, index);
    } else if (event === "failed") {
      entry.object.emit("failed", {}, message);
    } else {
      entry.object.emit(event, {});
    }
  }
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    for (const [token, entry] of [...active]) {
      ++entry.record.version;
      try {
        retire(token, false);
      } catch {
      }
    }
  }
  return { Notification, dispatch, stop };
}

// firefly/core/power-monitor/index.mjs
function createPowerMonitor(native, EventEmitter, isReady, isExited, error, unsupported) {
  const known = /* @__PURE__ */ new Set([
    "suspend",
    "resume",
    "on-ac",
    "on-battery",
    "thermal-state-change",
    "speed-limit-change",
    "shutdown",
    "lock-screen",
    "unlock-screen",
    "user-did-become-active",
    "user-did-resign-active"
  ]);
  const meta = /* @__PURE__ */ new Set(["newListener", "removeListener", "error"]);
  if (typeof EventEmitter.errorMonitor === "symbol") {
    meta.add(EventEmitter.errorMonitor);
  }
  const thermal = /* @__PURE__ */ new Set(["unknown", "nominal", "fair", "serious", "critical"]);
  const idle = /* @__PURE__ */ new Set(["active", "idle", "locked", "unknown"]);
  const parse = JSON.parse;
  const token = Symbol("powerMonitor");
  let capabilities;
  let stopped = false;
  let epoch = null;
  let next = 0;
  let transitioning = false;
  let cleaning = false;
  function alive() {
    if (stopped || isExited()) {
      throw error("ERR_FIREFLY_APP_EXITED", "Power monitor application has exited");
    }
  }
  function ready() {
    alive();
    if (!isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "Power queries require app readiness");
    }
  }
  function receiver(value) {
    if (value !== powerMonitor) {
      throw error("ERR_INVALID_ARG_TYPE", "Illegal powerMonitor receiver");
    }
  }
  function data(message) {
    return error("ERR_FIREFLY_POWER_MONITOR_DATA", message);
  }
  function decode(value) {
    if (typeof value !== "string" || value.length > 16384) {
      throw data("Invalid native power snapshot");
    }
    try {
      return parse(value);
    } catch {
      throw data("Invalid native power JSON");
    }
  }
  function events() {
    if (!capabilities) {
      const value = typeof native.powerMonitorCapabilities === "function" ? decode(native.powerMonitorCapabilities()) : [];
      alive();
      if (!Array.isArray(value) || value.length > known.size || value.some((name) => !known.has(name)) || new Set(value).size !== value.length) {
        throw data("Invalid native power event capabilities");
      }
      capabilities = new Set(value);
    }
    return capabilities;
  }
  function validate(name, listener) {
    alive();
    if (typeof listener !== "function") {
      throw error("ERR_INVALID_ARG_TYPE", "Power listener must be a function");
    }
    if (!meta.has(name) && (!known.has(name) || !events().has(name))) {
      unsupported(`powerMonitor event ${String(name)}`);
    }
    alive();
  }
  function interested() {
    return [...known].some((name) => powerMonitor.listenerCount(name) > 0);
  }
  function clearNativeListeners() {
    cleaning = true;
    try {
      for (const name of known) {
        EventEmitter.prototype.removeAllListeners.call(powerMonitor, name);
      }
    } finally {
      cleaning = false;
    }
  }
  function synchronize() {
    if (transitioning) {
      return;
    }
    transitioning = true;
    try {
      for (let count = 0; ; ++count) {
        if (count >= 32) {
          const id = epoch;
          epoch = null;
          try {
            if (id !== null) {
              native.powerMonitorStop(id);
            }
          } catch {
          } finally {
            clearNativeListeners();
          }
          throw error(
            "ERR_FIREFLY_POWER_MONITOR_REENTRY",
            "Power listener transitions did not settle"
          );
        }
        const wanted = !stopped && !isExited() && isReady() && interested();
        if (wanted && epoch === null) {
          if (next >= 2147483647) {
            throw error("ERR_FIREFLY_POWER_MONITOR_LIMIT", "Power observation epochs exhausted");
          }
          const id = ++next;
          epoch = id;
          try {
            native.powerMonitorStart(id);
          } catch (failure) {
            if (epoch === id) {
              epoch = null;
            }
            try {
              native.powerMonitorStop(id);
            } catch {
            }
            clearNativeListeners();
            throw failure;
          }
        } else if (!wanted && epoch !== null) {
          const id = epoch;
          epoch = null;
          native.powerMonitorStop(id);
        } else {
          break;
        }
      }
    } finally {
      transitioning = false;
    }
  }
  function add(object, name, listener, prepend) {
    receiver(object);
    validate(name, listener);
    const method = prepend ? EventEmitter.prototype.prependListener : EventEmitter.prototype.on;
    method.call(powerMonitor, name, listener);
    if (stopped || isExited()) {
      clearNativeListeners();
      alive();
    }
    synchronize();
    alive();
    return powerMonitor;
  }
  function query(kind, threshold) {
    ready();
    const snapshot = native.powerMonitorQuery(kind, threshold);
    ready();
    const value = decode(snapshot);
    if (!value || typeof value !== "object" || Array.isArray(value) || !Object.hasOwn(value, "value")) {
      throw data("Native power query has no value");
    }
    const result = value.value;
    if (kind === "idle-time" && (!Number.isSafeInteger(result) || result < 0) || kind === "idle-state" && !idle.has(result) || kind === "thermal-state" && !thermal.has(result) || kind === "battery-power" && typeof result !== "boolean") {
      throw data("Invalid native power query value");
    }
    return result;
  }
  class PowerMonitor extends EventEmitter {
    constructor(key) {
      super();
      if (key !== token) {
        throw error("ERR_ILLEGAL_CONSTRUCTOR", "Use the shared powerMonitor object");
      }
    }
    on(name, listener) {
      return add(this, name, listener, false);
    }
    addListener(name, listener) {
      return add(this, name, listener, false);
    }
    prependListener(name, listener) {
      return add(this, name, listener, true);
    }
    removeListener(name, listener) {
      receiver(this);
      super.removeListener(name, listener);
      synchronize();
      return this;
    }
    off(name, listener) {
      return this.removeListener(name, listener);
    }
    removeAllListeners(...args) {
      receiver(this);
      super.removeAllListeners(...args);
      synchronize();
      return this;
    }
    emit(name, ...args) {
      if (cleaning && (name === "newListener" || name === "removeListener")) {
        return false;
      }
      return super.emit(name, ...args);
    }
    getSystemIdleTime() {
      receiver(this);
      return query("idle-time");
    }
    getSystemIdleState(threshold) {
      receiver(this);
      if (typeof threshold !== "number" || !Number.isInteger(threshold)) {
        throw error("ERR_INVALID_ARG_TYPE", "Idle threshold must be an integer");
      }
      if (threshold <= 0 || threshold > 2147483647) {
        throw error("ERR_OUT_OF_RANGE", "Idle threshold must be positive and within INTMAX");
      }
      return query("idle-state", threshold);
    }
    getCurrentThermalState() {
      receiver(this);
      return query("thermal-state");
    }
    isOnBatteryPower() {
      receiver(this);
      return query("battery-power");
    }
    get onBatteryPower() {
      receiver(this);
      return query("battery-power");
    }
  }
  const powerMonitor = new PowerMonitor(token);
  function dispatch(value) {
    if (stopped || isExited() || epoch === null || value?.epoch !== epoch) {
      return;
    }
    if (transitioning) {
      queueMicrotask(() => dispatch(value));
      return;
    }
    const name = value.type;
    if (!known.has(name) || !events().has(name)) {
      throw data("Unknown native power event");
    }
    if (name === "thermal-state-change") {
      if (!thermal.has(value.state)) {
        throw data("Invalid native thermal state");
      }
      powerMonitor.emit(name, { state: value.state });
    } else if (name === "speed-limit-change") {
      if (!Number.isInteger(value.limit) || value.limit < 0 || value.limit > 100) {
        throw data("Invalid native CPU speed limit");
      }
      powerMonitor.emit(name, { limit: value.limit });
    } else if (name === "shutdown") {
      powerMonitor.emit(
        name,
        Object.freeze({
          defaultPrevented: false,
          preventDefault() {
            unsupported("powerMonitor shutdown cancellation");
          }
        })
      );
    } else {
      powerMonitor.emit(name);
    }
  }
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const id = epoch;
    epoch = null;
    clearNativeListeners();
    if (id !== null) {
      try {
        native.powerMonitorStop(id);
      } catch {
      }
    }
  }
  return { powerMonitor, ready: synchronize, dispatch, stop };
}

// firefly/core/power-save-blocker/index.mjs
function createPowerSaveBlocker(native, isExited, error, unsupported) {
  const minimumId = -2147483648;
  const maximumId = 2147483647;
  let stopped = false;
  function alive() {
    if (stopped || isExited()) {
      stop();
      throw error("ERR_FIREFLY_APP_EXITED", "Power save blocker application has exited");
    }
  }
  function receiver(value) {
    if (value !== powerSaveBlocker) {
      throw error("ERR_INVALID_ARG_TYPE", "Illegal powerSaveBlocker receiver");
    }
    alive();
  }
  function id(value) {
    if (typeof value !== "number" || !Number.isInteger(value) || Object.is(value, -0)) {
      throw error("ERR_INVALID_ARG_TYPE", "Power save blocker ID must be an int32");
    }
    if (value < minimumId || value > maximumId) {
      throw error("ERR_OUT_OF_RANGE", "Power save blocker ID is outside int32 range");
    }
  }
  function call(name, value) {
    const method = native[name];
    alive();
    if (typeof method !== "function") {
      const suffix = name.replace("powerSaveBlocker", "");
      return unsupported("powerSaveBlocker." + suffix[0].toLowerCase() + suffix.slice(1));
    }
    const result = method.call(native, value);
    if (name === "powerSaveBlockerStart" && (stopped || isExited())) {
      if (stopped) {
        cleanup();
      } else {
        stop();
      }
    }
    alive();
    return result;
  }
  function boolean(value) {
    if (typeof value !== "boolean") {
      stop();
      throw error("ERR_FIREFLY_POWER_SAVE_DATA", "Invalid native power save blocker result");
    }
    return value;
  }
  const powerSaveBlocker = {
    start(type) {
      receiver(this);
      if (typeof type !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Power save blocker type must be a string");
      }
      if (type !== "prevent-app-suspension" && type !== "prevent-display-sleep") {
        throw error("ERR_INVALID_ARG_VALUE", "Unknown power save blocker type");
      }
      const result = call("powerSaveBlockerStart", type);
      if (!Number.isInteger(result) || Object.is(result, -0) || result < 0 || result > maximumId) {
        stop();
        throw error("ERR_FIREFLY_POWER_SAVE_DATA", "Invalid native power save blocker ID");
      }
      return result;
    },
    stop(value) {
      receiver(this);
      id(value);
      return boolean(call("powerSaveBlockerStop", value));
    },
    isStarted(value) {
      receiver(this);
      id(value);
      return boolean(call("powerSaveBlockerIsStarted", value));
    }
  };
  function cleanup() {
    try {
      native.powerSaveBlockerShutdown?.();
    } catch {
    }
  }
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    cleanup();
  }
  return { powerSaveBlocker, stop };
}

// firefly/core/shell/index.mjs
function createShell(native, isExited, error, unsupported) {
  const pending = /* @__PURE__ */ new Map();
  const byteLength = Buffer.byteLength.bind(Buffer);
  let stopped = false;
  let next = 0;
  function alive() {
    if (stopped || isExited()) {
      stop();
      throw error("ERR_FIREFLY_APP_EXITED", "Shell application has exited");
    }
  }
  function receiver(value) {
    if (value !== shell) {
      throw error("ERR_INVALID_ARG_TYPE", "Illegal shell receiver");
    }
    alive();
  }
  function wellFormed(value) {
    for (let index = 0; index < value.length; index++) {
      const unit = value.charCodeAt(index);
      if (unit >= 55296 && unit <= 56319) {
        const low = value.charCodeAt(++index);
        if (!(low >= 56320 && low <= 57343)) {
          return false;
        }
      } else if (unit >= 56320 && unit <= 57343) {
        return false;
      }
    }
    return true;
  }
  function text(value, label, maximum) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", label + " must be a string");
    }
    if (!value) {
      throw error("ERR_INVALID_ARG_VALUE", label + " must not be empty");
    }
    if (value.includes("\0")) {
      throw error("ERR_INVALID_ARG_VALUE", label + " contains NUL");
    }
    if (byteLength(value, "utf8") > maximum) {
      throw error("ERR_OUT_OF_RANGE", label + " exceeds its UTF-8 budget");
    }
    if (!wellFormed(value)) {
      throw error("ERR_INVALID_ARG_VALUE", label + " contains an unpaired surrogate");
    }
    return value;
  }
  function options(value) {
    if (value === void 0) {
      return true;
    }
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", "Shell options must be an object");
    }
    for (const key of Reflect.ownKeys(value)) {
      if (key !== "activate") {
        unsupported("shell.openExternal option " + String(key));
      }
    }
    const activate = value.activate;
    if (activate !== void 0 && typeof activate !== "boolean") {
      throw error("ERR_INVALID_ARG_TYPE", "Shell activate must be a boolean");
    }
    return activate === void 0 ? true : activate;
  }
  function cancel(id) {
    try {
      native.shellCancel?.(id);
    } catch {
    }
  }
  function request(kind, payload) {
    alive();
    if (pending.size >= 64 || next >= 2147483647) {
      throw error("ERR_FIREFLY_SHELL_LIMIT", "Shell request budget exhausted");
    }
    const id = ++next;
    const json = JSON.stringify(payload);
    return new Promise((resolve, reject) => {
      const entry = { kind, resolve, reject, admitted: false, early: void 0 };
      pending.set(id, entry);
      try {
        const start = native.shellRequest;
        alive();
        if (typeof start !== "function") {
          unsupported("shell." + kind);
        }
        start.call(native, id, kind, json);
        alive();
        if (pending.get(id) !== entry) {
          return;
        }
        entry.admitted = true;
        if (entry.early !== void 0) {
          const result = entry.early;
          queueMicrotask(() => complete(id, entry, result));
        }
      } catch (failure) {
        if (pending.get(id) !== entry) {
          return;
        }
        pending.delete(id);
        cancel(id);
        if (stopped || isExited()) {
          reject(error("ERR_FIREFLY_APP_EXITED", "Application exited during shell admission"));
        } else {
          reject(failure);
        }
      }
    });
  }
  function complete(id, entry, result) {
    if (pending.get(id) !== entry) {
      return;
    }
    let failure;
    try {
      alive();
      if (typeof result.ok !== "boolean" || result.message !== void 0 && typeof result.message !== "string" || result.code !== void 0 && typeof result.code !== "string" || result.ok && (result.message !== void 0 || result.code !== void 0) || result.message !== void 0 && (result.message.includes("\0") || byteLength(result.message, "utf8") > 65536 || !wellFormed(result.message)) || result.code !== void 0 && !/^[A-Z][A-Z0-9_]{0,127}$/.test(result.code) || !result.ok && !result.message) {
        throw error("ERR_FIREFLY_SHELL_DATA", "Invalid native shell completion");
      }
      alive();
    } catch (caught) {
      failure = caught;
    }
    if (pending.get(id) !== entry) {
      return;
    }
    pending.delete(id);
    if (failure) {
      cancel(id);
      entry.reject(failure);
    } else if (!result.ok && [
      "ERR_FIREFLY_SHELL_UNAVAILABLE",
      "ERR_FIREFLY_SHELL_LIMIT",
      "ERR_FIREFLY_SHELL_DATA",
      "ERR_FIREFLY_APP_EXITED",
      "ERR_FIREFLY_UNSUPPORTED_API"
    ].includes(result.code)) {
      entry.reject(error(result.code, result.message));
    } else if (entry.kind === "openPath") {
      entry.resolve(result.ok ? "" : result.message);
    } else if (result.ok) {
      entry.resolve();
    } else {
      entry.reject(error(result.code || "ERR_FIREFLY_SHELL_FAILED", result.message));
    }
  }
  function dispatch(value) {
    const id = value?.id;
    const entry = pending.get(id);
    if (!entry) {
      return;
    }
    if (entry.early !== void 0) {
      return;
    }
    let result;
    try {
      result = { ok: value.ok, message: value.message, code: value.code };
    } catch {
      result = {};
    }
    if (!entry.admitted) {
      if (entry.early === void 0) {
        entry.early = result;
      }
    } else {
      complete(id, entry, result);
    }
  }
  const shell = {
    showItemInFolder(path) {
      receiver(this);
      const value = text(path, "Shell path", 32768);
      const show = native.shellShowItemInFolder;
      alive();
      if (typeof show !== "function") {
        return unsupported("shell.showItemInFolder");
      }
      show.call(native, value);
      alive();
    },
    async openPath(path) {
      receiver(this);
      return request("openPath", { path: text(path, "Shell path", 32768) });
    },
    async openExternal(url, value) {
      receiver(this);
      const snapshot = { url: text(url, "Shell URL", 65536), activate: options(value) };
      alive();
      return request("openExternal", snapshot);
    },
    async trashItem(path) {
      receiver(this);
      return request("trashItem", { path: text(path, "Shell path", 32768) });
    },
    beep() {
      receiver(this);
      return unsupported("shell.beep");
    },
    writeShortcutLink() {
      receiver(this);
      return unsupported("shell.writeShortcutLink");
    },
    readShortcutLink() {
      receiver(this);
      return unsupported("shell.readShortcutLink");
    }
  };
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const requests = [...pending];
    pending.clear();
    for (const [id, entry] of requests) {
      entry.reject(error("ERR_FIREFLY_APP_EXITED", "Application exited during shell request"));
      cancel(id);
    }
    try {
      native.shellShutdown?.();
    } catch {
    }
  }
  return { shell, dispatch, stop };
}

// firefly/core/system-preferences/index.mjs
function createSystemPreferences(native, isExited, error, unsupported) {
  const subscriptions = /* @__PURE__ */ new Map();
  const byteLength = Buffer.byteLength.bind(Buffer);
  let next = 0;
  let stopped = false;
  function data(message) {
    return error("ERR_FIREFLY_SYSTEM_PREFERENCES_DATA", message);
  }
  function alive() {
    if (stopped || isExited()) {
      stop();
      throw error("ERR_FIREFLY_APP_EXITED", "System preferences application has exited");
    }
  }
  function receiver(value) {
    if (value !== systemPreferences) {
      throw error("ERR_INVALID_ARG_TYPE", "Illegal systemPreferences receiver");
    }
    alive();
  }
  function text(value, maximum, empty = false, allowNul = false) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", "System preference text must be a string");
    }
    if (!empty && !value || !allowNul && value.includes("\0")) {
      throw error("ERR_INVALID_ARG_VALUE", "Invalid system preference text");
    }
    if (byteLength(value, "utf8") > maximum) {
      throw error("ERR_OUT_OF_RANGE", "System preference text exceeds its UTF-8 budget");
    }
    for (let index = 0; index < value.length; index++) {
      const unit = value.charCodeAt(index);
      if (unit >= 55296 && unit <= 56319) {
        const low = value.charCodeAt(++index);
        if (!(low >= 56320 && low <= 57343)) {
          throw error("ERR_INVALID_ARG_VALUE", "Unpaired system preference surrogate");
        }
      } else if (unit >= 56320 && unit <= 57343) {
        throw error("ERR_INVALID_ARG_VALUE", "Unpaired system preference surrogate");
      }
    }
    return value;
  }
  function type(value) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", "System preference type must be a string");
    }
    if (value !== "string" && value !== "boolean") {
      unsupported("systemPreferences default type " + value);
    }
    return value;
  }
  function call(name, ...args) {
    const method = native[name];
    alive();
    if (typeof method !== "function") {
      return unsupported("systemPreferences native " + name);
    }
    const result = method.call(native, ...args);
    alive();
    return result;
  }
  function snapshot(raw) {
    try {
      if (typeof raw !== "string" || byteLength(raw, "utf8") > 524288) {
        throw data("Invalid preference snapshot size");
      }
      const result = JSON.parse(raw);
      if (!result || typeof result !== "object" || Array.isArray(result) || !Object.hasOwn(result, "value")) {
        throw data("Invalid preference snapshot shape");
      }
      return result.value;
    } catch {
      throw data("Invalid native preference snapshot");
    }
  }
  function release(id, entry) {
    if (subscriptions.get(id) !== entry) {
      return;
    }
    subscriptions.delete(id);
    entry.callback = null;
    entry.early.length = 0;
    try {
      native.preferencesUnsubscribe?.(id, entry.kind);
    } catch {
    }
  }
  function subscribe(kind, name, callback) {
    if (name !== null) {
      text(name, 4096);
    }
    if (typeof callback !== "function") {
      throw error("ERR_INVALID_ARG_TYPE", "System preference callback must be a function");
    }
    alive();
    if (subscriptions.size >= 64 || next > 2147483647) {
      throw error(
        "ERR_FIREFLY_SYSTEM_PREFERENCES_LIMIT",
        "Preference subscription budget exhausted"
      );
    }
    const id = next++;
    const entry = { kind, name, callback, admitted: false, early: [] };
    subscriptions.set(id, entry);
    try {
      call("preferencesSubscribe", id, kind, name);
      if (subscriptions.get(id) !== entry) {
        throw data("Native preference subscription retired during admission");
      }
      entry.admitted = true;
      if (entry.early.length) {
        schedule(id, entry);
      }
      return id;
    } catch (failure) {
      if (subscriptions.get(id) === entry) {
        release(id, entry);
      } else if (stopped || isExited()) {
        try {
          native.preferencesUnsubscribe?.(id, kind);
        } catch {
        }
      }
      throw failure;
    }
  }
  function schedule(id, entry) {
    queueMicrotask(() => {
      while (subscriptions.get(id) === entry && entry.early.length) {
        deliver(id, entry, entry.early.shift());
      }
    });
  }
  function unsubscribe(kind, id) {
    if (typeof id !== "number" || !Number.isInteger(id) || Object.is(id, -0)) {
      throw error("ERR_INVALID_ARG_TYPE", "Preference subscription ID must be an int32");
    }
    if (id < -2147483648 || id > 2147483647) {
      throw error("ERR_OUT_OF_RANGE", "Preference subscription ID exceeds int32 range");
    }
    const entry = subscriptions.get(id);
    if (entry?.kind === kind) {
      subscriptions.delete(id);
      entry.callback = null;
      entry.early.length = 0;
      call("preferencesUnsubscribe", id, kind);
    }
  }
  function userInfo(value) {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      throw data("Notification userInfo must be a dictionary");
    }
    let nodes = 0;
    const seen = /* @__PURE__ */ new Set();
    function clone(item, depth) {
      if (++nodes > 1024 || depth > 8) {
        throw data("Notification JSON graph exceeds its budget");
      }
      if (item === null || typeof item === "boolean" || typeof item === "number" && Number.isFinite(item)) {
        return item;
      }
      if (typeof item === "string") {
        return text(item, 65536, true, true);
      }
      if (!item || typeof item !== "object" || seen.has(item)) {
        throw data("Notification userInfo is not JSON data");
      }
      const array = Array.isArray(item);
      const prototype = Object.getPrototypeOf(item);
      if (!array && prototype !== Object.prototype && prototype !== null) {
        throw data("Notification userInfo has an unsupported prototype");
      }
      seen.add(item);
      const output = array ? [] : {};
      const keys = Reflect.ownKeys(item);
      for (const key of keys) {
        if (array && key === "length") {
          continue;
        }
        if (typeof key !== "string" || array && !/^(0|[1-9][0-9]*)$/.test(key)) {
          throw data("Notification userInfo has an invalid key");
        }
        text(key, 65536, true, true);
        const descriptor = Object.getOwnPropertyDescriptor(item, key);
        if (!descriptor || !Object.hasOwn(descriptor, "value") || !descriptor.enumerable) {
          throw data("Notification userInfo must contain data properties");
        }
        Object.defineProperty(output, key, {
          value: clone(descriptor.value, depth + 1),
          enumerable: true,
          writable: true,
          configurable: true
        });
      }
      if (array && output.length !== keys.length - 1) {
        throw data("Notification userInfo arrays must be dense");
      }
      seen.delete(item);
      return output;
    }
    const result = clone(value, 0);
    if (byteLength(JSON.stringify(result), "utf8") > 65536) {
      throw data("Notification userInfo exceeds its JSON byte budget");
    }
    return result;
  }
  function packet(value, entry) {
    try {
      const code = value.code;
      const event = value.event;
      const info = value.userInfo;
      const object = value.object;
      if (code !== void 0) {
        throw data("Native preference notification delivery failed");
      }
      text(event, 4096);
      text(object, 4096, true);
      if (entry.name !== null && event !== entry.name) {
        throw data("Native preference notification name mismatch");
      }
      return { event, userInfo: userInfo(info), object };
    } catch {
      throw data("Invalid native preference notification");
    }
  }
  function deliver(id, entry, value) {
    alive();
    if (subscriptions.get(id) !== entry) {
      return;
    }
    entry.callback(value.event, value.userInfo, value.object);
  }
  function dispatch(value) {
    const id = value?.id;
    const entry = subscriptions.get(id);
    if (!entry) {
      return;
    }
    let result;
    try {
      result = packet(value, entry);
      alive();
      if (subscriptions.get(id) !== entry) {
        return;
      }
      if (!entry.admitted || entry.early.length) {
        if (entry.early.length >= 64) {
          throw data("Too many early preference notifications");
        }
        entry.early.push(result);
        return;
      }
    } catch (failure) {
      release(id, entry);
      throw failure;
    }
    deliver(id, entry, result);
  }
  const systemPreferences = {
    getUserDefault(name, kind) {
      receiver(this);
      text(name, 4096);
      type(kind);
      const value = snapshot(call("preferencesGetDefault", name, kind));
      try {
        if (kind === "string") {
          text(value, 65536, true);
        } else if (typeof value !== "boolean") {
          throw data("Native boolean default has invalid type");
        }
      } catch {
        throw data("Invalid native default value");
      }
      return value;
    },
    setUserDefault(name, kind, value) {
      receiver(this);
      text(name, 4096);
      type(kind);
      if (kind === "string") {
        text(value, 65536, true);
      } else if (typeof value !== "boolean") {
        throw error("ERR_INVALID_ARG_TYPE", "Boolean default requires a boolean");
      }
      call("preferencesSetDefault", name, kind, JSON.stringify(value));
    },
    getMediaAccessStatus(kind) {
      receiver(this);
      if (typeof kind !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Media type must be a string");
      }
      if (!["camera", "microphone", "screen"].includes(kind)) {
        throw error("ERR_INVALID_ARG_VALUE", "Invalid media type");
      }
      const value = snapshot(call("preferencesMediaStatus", kind));
      if (!["not-determined", "granted", "denied", "restricted", "unknown"].includes(value)) {
        throw data("Invalid native media status");
      }
      return value;
    }
  };
  for (const [suffix, kind] of [
    ["", "distributed"],
    ["Local", "local"],
    ["Workspace", "workspace"]
  ]) {
    systemPreferences["subscribe" + suffix + "Notification"] = function(name, callback) {
      receiver(this);
      return subscribe(kind, name, callback);
    };
    systemPreferences["unsubscribe" + suffix + "Notification"] = function(id) {
      receiver(this);
      unsubscribe(kind, id);
    };
  }
  for (const name of [
    "postNotification",
    "postLocalNotification",
    "postWorkspaceNotification",
    "registerDefaults",
    "removeUserDefault",
    "askForMediaAccess",
    "isSwipeTrackingFromScrollEventsEnabled",
    "getAccentColor",
    "getColor",
    "getSystemColor",
    "getEffectiveAppearance",
    "canPromptTouchID",
    "promptTouchID",
    "isTrustedAccessibilityClient",
    "getAnimationSettings",
    "on",
    "once",
    "addListener",
    "removeListener",
    "off",
    "emit",
    "removeAllListeners",
    "prependListener",
    "prependOnceListener"
  ]) {
    systemPreferences[name] = function() {
      receiver(this);
      return unsupported("systemPreferences." + name);
    };
  }
  for (const name of ["effectiveAppearance", "accessibilityDisplayShouldReduceTransparency"]) {
    Object.defineProperty(systemPreferences, name, {
      get() {
        receiver(this);
        return unsupported("systemPreferences." + name);
      }
    });
  }
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const entries = [...subscriptions];
    subscriptions.clear();
    for (const [, entry] of entries) {
      entry.callback = null;
      entry.early.length = 0;
    }
    for (const [id, entry] of entries) {
      try {
        native.preferencesUnsubscribe?.(id, entry.kind);
      } catch {
      }
    }
    try {
      native.preferencesShutdown?.();
    } catch {
    }
  }
  return { systemPreferences, dispatch, stop };
}

// firefly/core/net/index.mjs
function createNet(native, isReady, isExited, error, unsupported, transport) {
  if (!transport) {
    const fail = () => unsupported("net captured transport");
    let stopped2 = false;
    return {
      net: {
        fetch: async () => fail(),
        request: fail,
        isOnline: fail,
        resolveHost: fail,
        get online() {
          return fail();
        }
      },
      stop() {
        if (!stopped2) {
          stopped2 = true;
          try {
            native.networkStop?.();
          } catch {
          }
        }
      }
    };
  }
  const {
    fetch: fetchTransport,
    Request,
    Response,
    Headers,
    URL: URL2,
    AbortController,
    ReadableStream,
    Readable,
    Writable,
    Buffer: Buffer2,
    abortHooks
  } = transport;
  const active = /* @__PURE__ */ new Set();
  const requests = /* @__PURE__ */ new Set();
  const states = /* @__PURE__ */ new WeakMap();
  const redirects = /* @__PURE__ */ new Set([301, 302, 303, 307, 308]);
  const responseClone = Response.prototype.clone;
  const fromWeb = Readable.fromWeb.bind(Readable);
  const from = Readable.from.bind(Readable);
  const destroyWritable = Writable.prototype.destroy;
  const destroyReadable = Readable.prototype.destroy;
  const abortController = AbortController.prototype.abort;
  let stopped = false;
  function alive(ready = true) {
    if (stopped || isExited()) {
      stop();
      throw error("ERR_FIREFLY_APP_EXITED", "Network application has exited");
    }
    if (ready && !isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "net requires app readiness");
    }
  }
  function proxy() {
    alive();
    if (typeof native.networkHasProxy !== "function") {
      unsupported("net native proxy snapshot");
    }
    const value = native.networkHasProxy();
    alive();
    if (typeof value !== "boolean") {
      throw error("ERR_FIREFLY_NET_DATA", "Invalid native proxy snapshot");
    }
    if (value) {
      unsupported("net configured proxy/PAC");
    }
  }
  function url(value, base) {
    const result = new URL2(value, base);
    if (!["http:", "https:"].includes(result.protocol)) {
      unsupported("net URL scheme " + result.protocol);
    }
    if (result.username || result.password) {
      unsupported("net URL authentication");
    }
    result.hash = "";
    return result;
  }
  function policy(value) {
    if (value === void 0 || value === null) {
      return {};
    }
    if (typeof value !== "object") {
      throw error("ERR_INVALID_ARG_TYPE", "Network options must be an object");
    }
    const result = { ...value };
    for (const name of [
      "session",
      "partition",
      "useSessionCookies",
      "auth",
      "proxy",
      "proxyRules",
      "dispatcher",
      "origin",
      "priority",
      "priorityIncremental"
    ]) {
      if (result[name] !== void 0) {
        unsupported("net option " + name);
      }
    }
    if (result.bypassCustomProtocolHandlers !== void 0 && result.bypassCustomProtocolHandlers !== true) {
      unsupported("net custom protocol interception");
    }
    if (result.agent !== void 0 && result.agent !== null) {
      unsupported("net agent");
    }
    if (result.rejectUnauthorized !== void 0 && result.rejectUnauthorized !== true) {
      unsupported("net disabled TLS verification");
    }
    if (result.credentials !== void 0 && result.credentials !== "omit") {
      unsupported("net session credentials");
    }
    if (result.cache !== void 0 && !["default", "no-store"].includes(result.cache)) {
      unsupported("net cache policy");
    }
    if (result.keepalive) {
      unsupported("net keepalive");
    }
    if (result.referrerPolicy || result.referrer || result.integrity) {
      unsupported("net referrer/integrity policy");
    }
    return result;
  }
  function acquire(signal) {
    alive();
    if (active.size >= 64) {
      throw error("ERR_FIREFLY_NET_LIMIT", "Network request budget exhausted");
    }
    const controller = new AbortController();
    const entry = { controller, unsubscribe: null };
    active.add(entry);
    if (signal) {
      entry.unsubscribe = abortHooks.subscribe(
        signal,
        () => abortController.call(controller, abortHooks.reason(signal))
      );
    }
    return entry;
  }
  function release(entry) {
    active.delete(entry);
    entry.unsubscribe?.();
    entry.unsubscribe = null;
  }
  function annotate(response, endpoint, redirected, headers = response.headers) {
    Object.defineProperties(response, {
      url: { value: endpoint, configurable: true },
      redirected: { value: redirected, configurable: true },
      headers: { value: headers, configurable: true },
      clone: {
        value() {
          return annotate(responseClone.call(this), endpoint, redirected, headers);
        },
        configurable: true
      }
    });
    return response;
  }
  function ownedResponse(response, entry, endpoint, redirected) {
    if (!response.body) {
      release(entry);
      return annotate(response, endpoint, redirected);
    }
    const reader = response.body.getReader();
    const body = new ReadableStream({
      type: "bytes",
      async pull(controller) {
        try {
          const result = await reader.read();
          if (result.done) {
            release(entry);
            controller.close();
            controller.byobRequest?.respond(0);
          } else {
            controller.enqueue(result.value);
          }
        } catch (failure) {
          release(entry);
          controller.error(failure);
        }
      },
      async cancel(reason) {
        try {
          await reader.cancel(reason);
        } finally {
          release(entry);
        }
      }
    });
    return annotate(
      new Response(body, {
        status: response.status,
        statusText: response.statusText,
        headers: response.headers
      }),
      endpoint,
      redirected,
      response.headers
    );
  }
  function nextHop(endpoint, method, headers, body, status, location) {
    const target = url(location, endpoint);
    const fields = new Headers(headers);
    if (target.origin !== endpoint.origin) {
      for (const name of ["authorization", "proxy-authorization", "cookie"]) {
        fields.delete(name);
      }
    }
    if (status === 303 && method !== "GET" && method !== "HEAD" || [301, 302].includes(status) && method === "POST") {
      method = "GET";
      body = null;
      for (const name of [
        "content-length",
        "content-type",
        "content-encoding",
        "content-language",
        "content-location"
      ]) {
        fields.delete(name);
      }
    }
    return { endpoint: target, method, headers: fields, body };
  }
  async function fetch(input, init) {
    alive();
    const selected = policy(init);
    const original = new Request(input, selected);
    if (original.credentials === "include") {
      unsupported("net session credentials");
    }
    let endpoint = url(original.url);
    let method = original.method;
    let fields = new Headers(original.headers);
    let body = original.body;
    const mode = original.redirect;
    const entry = acquire(original.signal);
    try {
      for (let count = 0; ; count++) {
        proxy();
        const response = await fetchTransport(endpoint.href, {
          method,
          headers: fields,
          body,
          duplex: "half",
          redirect: "manual",
          signal: entry.controller.signal,
          credentials: "omit",
          cache: original.cache
        });
        alive();
        const location = response.headers.get("location");
        if (!redirects.has(response.status) || !location || mode === "manual") {
          return ownedResponse(response, entry, endpoint.href, count > 0);
        }
        await response.body?.cancel();
        if (mode === "error") {
          throw error("ERR_FIREFLY_NET_REDIRECT", "Redirect policy forbids redirect");
        }
        if (count >= 20) {
          throw error("ERR_FIREFLY_NET_REDIRECT", "Too many redirects");
        }
        const next = nextHop(endpoint, method, fields, body, response.status, location);
        if (next.body !== null) {
          unsupported("net.fetch replay of redirected streaming body");
        }
        ({ endpoint, method, headers: fields, body } = next);
      }
    } catch (failure) {
      abortController.call(entry.controller, failure);
      release(entry);
      throw failure;
    }
  }
  function responseHeaders(response, arrays = false) {
    const result = /* @__PURE__ */ Object.create(null);
    for (const [name, value] of response.headers) {
      result[name] = arrays ? [value] : value;
    }
    const cookies = response.headers.getSetCookie?.() ?? [];
    if (cookies.length) {
      result["set-cookie"] = [...cookies];
    }
    return result;
  }
  function requestOptions(value) {
    const selected = typeof value === "string" ? { url: value } : policy(value);
    let address = selected.url ?? selected.href;
    if (!address) {
      const protocol = selected.protocol ?? "http:";
      let authority = selected.host ?? selected.hostname ?? "localhost";
      if (!selected.host && selected.port) {
        authority += ":" + selected.port;
      }
      const pathname = selected.path ?? "/";
      if (String(pathname).includes(" ")) {
        throw error("ERR_INVALID_ARG_VALUE", "Request path contains unescaped spaces");
      }
      address = protocol + "//" + authority + pathname;
    }
    const endpoint = url(address);
    const method = String(selected.method ?? "GET").toUpperCase();
    if (!/^[!#$%&'*+.^_`|~0-9A-Z-]+$/.test(method) || ["CONNECT", "TRACE", "TRACK"].includes(method)) {
      throw error("ERR_INVALID_ARG_VALUE", "Invalid request method");
    }
    const redirect = selected.redirect ?? "follow";
    if (!["manual", "follow", "error"].includes(redirect)) {
      throw error("ERR_INVALID_ARG_VALUE", "Invalid redirect policy");
    }
    return {
      endpoint,
      method,
      redirect,
      headers: selected.headers ?? {},
      cache: selected.cache ?? "default"
    };
  }
  class ClientRequest extends Writable {
    constructor(spec, callback) {
      super({ autoDestroy: false });
      const state = {
        spec,
        headers: /* @__PURE__ */ new Map(),
        chunks: [],
        size: 0,
        locked: false,
        started: false,
        aborted: false,
        entry: null,
        response: null,
        waiting: false,
        follow: false,
        chunked: void 0
      };
      states.set(this, state);
      if (callback) {
        this.once("response", callback);
      }
      for (const [name, value] of Object.entries(spec.headers)) {
        this.setHeader(name, value);
      }
      alive();
      if (requests.size >= 64) {
        throw error("ERR_FIREFLY_NET_LIMIT", "ClientRequest budget exhausted");
      }
      requests.add(this);
    }
    setHeader(name, value) {
      const state = states.get(this);
      if (state.locked) {
        throw error("ERR_FIREFLY_NET_HEADERS_SENT", "Request headers have already been sent");
      }
      if (typeof name !== "string" || value === void 0 || value === null) {
        throw error("ERR_INVALID_ARG_TYPE", "Invalid request header");
      }
      const values = Array.isArray(value) ? value.map(String) : String(value);
      new Headers([[name, Array.isArray(values) ? values.join(",") : values]]);
      state.headers.set(name.toLowerCase(), { name, value: values });
    }
    getHeader(name) {
      if (typeof name !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Header name must be a string");
      }
      const value = states.get(this).headers.get(name.toLowerCase())?.value;
      return Array.isArray(value) ? [...value] : value;
    }
    removeHeader(name) {
      const state = states.get(this);
      if (state.locked) {
        throw error("ERR_FIREFLY_NET_HEADERS_SENT", "Request headers have already been sent");
      }
      if (typeof name !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Header name must be a string");
      }
      state.headers.delete(name.toLowerCase());
    }
    get chunkedEncoding() {
      return false;
    }
    set chunkedEncoding(value) {
      const state = states.get(this);
      if (state.locked || state.chunked !== void 0) {
        throw error("ERR_INVALID_STATE", "chunkedEncoding may only be set once before writing");
      }
      if (value) {
        unsupported("net.request chunked upload");
      }
      state.chunked = false;
    }
    _write(chunk, encoding, callback) {
      const state = states.get(this);
      state.locked = true;
      if (state.size + chunk.byteLength > 16777216) {
        callback(error("ERR_FIREFLY_NET_LIMIT", "Request body exceeds 16 MiB"));
        return;
      }
      state.chunks.push(Buffer2.from(chunk));
      state.size += chunk.byteLength;
      callback();
    }
    _final(callback) {
      const state = states.get(this);
      state.locked = true;
      state.started = true;
      callback();
      run(this).catch((failure) => {
        if (!this.destroyed && !state.aborted) {
          this.destroy(failure);
        }
      });
    }
    _destroy(failure, callback) {
      const state = states.get(this);
      if (state.entry) {
        abortController.call(state.entry.controller, failure);
      }
      if (state.entry) {
        release(state.entry);
      }
      if (state.response) {
        destroyReadable.call(state.response, failure);
      }
      state.chunks.length = 0;
      requests.delete(this);
      callback(failure);
    }
    abort() {
      const state = states.get(this);
      if (!state.aborted) {
        state.aborted = true;
        queueMicrotask(() => this.emit("abort"));
        this.destroy();
      }
    }
    followRedirect() {
      const state = states.get(this);
      if (!state.waiting) {
        throw error("ERR_INVALID_STATE", "Not waiting for a redirect");
      }
      state.follow = true;
    }
    getUploadProgress() {
      return unsupported("net.request upload progress");
    }
  }
  async function run(request) {
    const state = states.get(request);
    if (request.destroyed) {
      return;
    }
    let { endpoint, method } = state.spec;
    let fields = new Headers();
    for (const { name, value } of state.headers.values()) {
      fields.set(name, Array.isArray(value) ? value.join(",") : value);
    }
    let body = state.size ? Buffer2.concat(state.chunks) : null;
    state.chunks.length = 0;
    const entry = state.entry = acquire();
    try {
      for (let count = 0; ; count++) {
        proxy();
        if (request.destroyed) {
          return;
        }
        const response = await fetchTransport(endpoint.href, {
          method,
          headers: fields,
          body,
          redirect: "manual",
          signal: entry.controller.signal,
          credentials: "omit",
          cache: state.spec.cache
        });
        alive();
        if (request.destroyed) {
          await response.body?.cancel();
          return;
        }
        const location = response.headers.get("location");
        if (redirects.has(response.status) && location) {
          const next = nextHop(endpoint, method, fields, body, response.status, location);
          await response.body?.cancel();
          if (state.spec.redirect === "error" || count >= 20) {
            throw error("ERR_FIREFLY_NET_REDIRECT", "Redirect policy forbids redirect");
          }
          state.waiting = true;
          state.follow = state.spec.redirect === "follow";
          try {
            request.emit(
              "redirect",
              response.status,
              next.method,
              next.endpoint.href,
              responseHeaders(response, true)
            );
          } finally {
            state.waiting = false;
          }
          if (request.destroyed) {
            return;
          }
          if (!state.follow) {
            throw error("ERR_FIREFLY_NET_REDIRECT", "Redirect was cancelled");
          }
          ({ endpoint, method, headers: fields, body } = next);
          continue;
        }
        const incoming = response.body ? fromWeb(response.body) : from([]);
        state.response = incoming;
        incoming.statusCode = response.status;
        incoming.statusMessage = response.statusText;
        incoming.headers = responseHeaders(response);
        for (const name of [
          "rawHeaders",
          "httpVersion",
          "httpVersionMajor",
          "httpVersionMinor",
          "trailers",
          "rawTrailers"
        ]) {
          Object.defineProperty(incoming, name, {
            get() {
              return unsupported("net response " + name);
            }
          });
        }
        incoming.once("end", () => {
          release(entry);
          request.destroy();
        });
        incoming.once("error", (failure) => {
          if (!request.destroyed) {
            request.destroy(failure);
          }
        });
        incoming.once("close", () => {
          if (!request.destroyed) {
            request.destroy();
          }
        });
        request.emit("response", incoming);
        return;
      }
    } catch (failure) {
      abortController.call(entry.controller, failure);
      release(entry);
      throw failure;
    }
  }
  const net = {
    fetch,
    request(value, callback) {
      alive();
      if (requests.size >= 64) {
        throw error("ERR_FIREFLY_NET_LIMIT", "ClientRequest budget exhausted");
      }
      if (callback !== void 0 && typeof callback !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "Response callback must be a function");
      }
      const spec = requestOptions(value);
      alive();
      return new ClientRequest(spec, callback);
    },
    isOnline() {
      alive(false);
      if (typeof native.networkIsOnline !== "function") {
        return unsupported("net native online query");
      }
      const value = native.networkIsOnline();
      alive(false);
      if (typeof value !== "boolean") {
        throw error("ERR_FIREFLY_NET_DATA", "Invalid native network status");
      }
      return value;
    },
    resolveHost() {
      return unsupported("net.resolveHost");
    }
  };
  Object.defineProperty(net, "online", { get: () => net.isOnline() });
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    try {
      for (const entry of [...active]) {
        try {
          abortController.call(
            entry.controller,
            error("ERR_FIREFLY_APP_EXITED", "Application exited during network request")
          );
        } catch {
        } finally {
          try {
            release(entry);
          } catch {
            active.delete(entry);
          }
        }
      }
      for (const request of [...requests]) {
        const state = states.get(request);
        state.chunks.length = 0;
        try {
          destroyWritable.call(request);
        } catch {
        } finally {
          requests.delete(request);
          if (state.response) {
            try {
              destroyReadable.call(state.response);
            } catch {
            }
          }
        }
      }
    } finally {
      try {
        native.networkStop?.();
      } catch {
      }
    }
  }
  return { net, stop };
}

// firefly/core/global-shortcut/index.mjs
function createGlobalShortcuts(native, isReady, isExited, error, unsupported, platform) {
  const tokens = /* @__PURE__ */ new Map();
  const chords = /* @__PURE__ */ new Map();
  const retired = /* @__PURE__ */ new Set();
  const releasing = /* @__PURE__ */ new Set();
  const modifiers = /* @__PURE__ */ new Map([
    ["shift", 1],
    ["control", 2],
    ["ctrl", 2],
    ["alt", 4],
    ["option", 4],
    ["command", 8],
    ["cmd", 8],
    ["meta", 8],
    ["super", 8],
    ["commandorcontrol", platform === "darwin" ? 8 : 2],
    ["cmdorctrl", platform === "darwin" ? 8 : 2]
  ]);
  const names = /* @__PURE__ */ new Map([
    ["space", "Space"],
    ["tab", "Tab"],
    ["enter", "Enter"],
    ["return", "Enter"],
    ["escape", "Escape"],
    ["esc", "Escape"],
    ["backspace", "Backspace"],
    ["delete", "Delete"],
    ["insert", "Insert"],
    ["home", "Home"],
    ["end", "End"],
    ["pageup", "PageUp"],
    ["pagedown", "PageDown"],
    ["up", "Up"],
    ["down", "Down"],
    ["left", "Left"],
    ["right", "Right"],
    ["plus", "+"]
  ]);
  let next = 0;
  let stopped = false;
  function alive(ready = true) {
    if (stopped || isExited()) {
      stop();
      throw error("ERR_FIREFLY_APP_EXITED", "Global shortcuts application has exited");
    }
    if (ready && !isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "globalShortcut requires app readiness");
    }
  }
  function receiver(value, ready = true) {
    if (value !== globalShortcut) {
      throw error("ERR_INVALID_ARG_TYPE", "Illegal globalShortcut receiver");
    }
    alive(ready);
  }
  function parse(value) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", "Accelerator must be a string");
    }
    if (value.length > 4096) {
      throw error("ERR_OUT_OF_RANGE", "Accelerator exceeds its text budget");
    }
    if (!value || /[^\x20-\x7e]/.test(value)) {
      throw error("ERR_INVALID_ARG_VALUE", "Accelerator must contain ASCII key names");
    }
    let key;
    let flags = 0;
    for (const part of value.split("+").map((piece) => piece.trim()).filter(Boolean)) {
      const lower = part.toLowerCase();
      if (modifiers.has(lower)) {
        flags |= modifiers.get(lower);
        continue;
      }
      if (lower === "altgr" || /^(media|volume|num)/.test(lower) || ["capslock", "scrolllock", "printscreen"].includes(lower) || /^f(?:2[1-4])$/.test(lower)) {
        unsupported("globalShortcut key " + part);
      }
      let current = names.get(lower);
      if (!current && /^[a-z0-9]$/i.test(part)) {
        current = part.toUpperCase();
      }
      if (!current && /^f(?:[1-9]|1[0-9]|20)$/.test(lower)) {
        current = lower.toUpperCase();
      }
      if (!current && part.length === 1 && /^[!"#$%&'()*,\-./:;<=>?@[\]\\^_`{|}~]$/.test(part)) {
        current = part;
      }
      if (!current || key !== void 0) {
        throw error("ERR_INVALID_ARG_VALUE", "Accelerator must contain exactly one supported key");
      }
      key = current;
    }
    if (!key) {
      throw error("ERR_INVALID_ARG_VALUE", "Accelerator does not contain a key");
    }
    return { key, modifiers: flags, canonical: flags + ":" + key };
  }
  function release(token) {
    if (releasing.has(token)) {
      return;
    }
    if (!stopped) {
      retired.add(token);
    }
    releasing.add(token);
    try {
      native.globalShortcutUnregister(token);
      retired.delete(token);
    } finally {
      releasing.delete(token);
    }
  }
  function retryRetired() {
    if (releasing.size) {
      return;
    }
    let failure;
    for (const token of [...retired]) {
      if (stopped || isExited()) {
        break;
      }
      try {
        release(token);
      } catch (caught) {
        failure ??= caught;
      }
    }
    alive();
    if (failure) {
      throw failure;
    }
  }
  function remove(entry, notify = true) {
    if (tokens.get(entry.token) !== entry) {
      return;
    }
    tokens.delete(entry.token);
    if (chords.get(entry.chord.canonical) === entry) {
      chords.delete(entry.chord.canonical);
    }
    entry.callback = null;
    entry.early = 0;
    if (notify) {
      release(entry.token);
    }
  }
  function discard(entry) {
    try {
      remove(entry);
    } catch {
    }
  }
  function admit(chord, callback) {
    alive();
    retryRetired();
    if (releasing.size) {
      throw error("ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT", "Native shortcut release is in progress");
    }
    if (chords.has(chord.canonical)) {
      return null;
    }
    if (tokens.size + retired.size >= 64 || next >= 2147483647) {
      throw error("ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT", "Global shortcut budget exhausted");
    }
    const register = native.globalShortcutRegister;
    if (typeof register !== "function" || typeof native.globalShortcutUnregister !== "function" || typeof native.globalShortcutShutdown !== "function") {
      unsupported("globalShortcut native ownership");
    }
    alive();
    const entry = { token: ++next, chord, callback, admitted: false, early: 0 };
    tokens.set(entry.token, entry);
    chords.set(chord.canonical, entry);
    try {
      const result = register.call(native, entry.token, chord.key, chord.modifiers);
      if (stopped || isExited() || tokens.get(entry.token) !== entry) {
        try {
          release(entry.token);
        } catch {
        }
        alive();
        throw error(
          "ERR_FIREFLY_GLOBAL_SHORTCUT_DATA",
          "Registration was retired during native admission"
        );
      }
      alive();
      if (typeof result !== "boolean") {
        throw error("ERR_FIREFLY_GLOBAL_SHORTCUT_DATA", "Invalid native shortcut admission result");
      }
      if (!result) {
        remove(entry, false);
        return null;
      }
      entry.admitted = true;
      if (entry.early) {
        schedule(entry);
      }
      return entry;
    } catch (failure) {
      discard(entry);
      throw failure;
    }
  }
  function schedule(entry) {
    queueMicrotask(() => {
      while (tokens.get(entry.token) === entry && entry.early > 0) {
        entry.early--;
        invoke(entry);
      }
    });
  }
  function invoke(entry) {
    if (stopped || isExited() || tokens.get(entry.token) !== entry || !entry.admitted) {
      return;
    }
    const callback = entry.callback;
    callback();
  }
  function dispatch(value) {
    const entry = tokens.get(value?.token);
    if (!entry || stopped || isExited()) {
      return;
    }
    if (!entry.admitted) {
      if (entry.early >= 64) {
        discard(entry);
        throw error(
          "ERR_FIREFLY_GLOBAL_SHORTCUT_DATA",
          "Native shortcut admission event budget exhausted"
        );
      }
      entry.early++;
    } else {
      invoke(entry);
    }
  }
  const globalShortcut = {
    register(accelerator, callback) {
      receiver(this);
      const chord = parse(accelerator);
      if (typeof callback !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "Shortcut callback must be a function");
      }
      return admit(chord, callback) !== null;
    },
    registerAll(accelerators, callback) {
      receiver(this);
      if (!Array.isArray(accelerators)) {
        throw error("ERR_INVALID_ARG_TYPE", "Accelerators must be an array");
      }
      if (accelerators.length > 64) {
        throw error("ERR_FIREFLY_GLOBAL_SHORTCUT_LIMIT", "Too many accelerators");
      }
      if (typeof callback !== "function") {
        throw error("ERR_INVALID_ARG_TYPE", "Shortcut callback must be a function");
      }
      const selected = Array.from(accelerators, parse);
      alive();
      const admitted = [];
      try {
        for (const chord of selected) {
          const entry = admit(chord, callback);
          if (!entry) {
            for (const owned of admitted) {
              discard(owned);
            }
            return false;
          }
          admitted.push(entry);
        }
        return true;
      } catch (failure) {
        for (const entry of admitted) {
          discard(entry);
        }
        throw failure;
      }
    },
    isRegistered(accelerator) {
      receiver(this, false);
      return chords.get(parse(accelerator).canonical)?.admitted === true;
    },
    unregister(accelerator) {
      receiver(this);
      const entry = chords.get(parse(accelerator).canonical);
      if (entry) {
        remove(entry);
      }
      alive();
    },
    unregisterAll() {
      receiver(this);
      let failure;
      try {
        retryRetired();
      } catch (caught) {
        failure = caught;
      }
      for (const entry of [...tokens.values()]) {
        try {
          remove(entry);
        } catch (caught) {
          failure ??= caught;
        }
      }
      alive();
      if (failure) {
        throw failure;
      }
    },
    setSuspended() {
      receiver(this);
      return unsupported("globalShortcut.setSuspended");
    },
    isSuspended() {
      receiver(this);
      return unsupported("globalShortcut.isSuspended");
    }
  };
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    for (const entry of tokens.values()) {
      entry.callback = null;
      entry.early = 0;
    }
    tokens.clear();
    chords.clear();
    retired.clear();
    try {
      native.globalShortcutShutdown?.();
    } catch {
    }
  }
  return { globalShortcut, dispatch, stop };
}

// firefly/core/dialog/index.mjs
function createDialogs(native, isReady, isExited, resolveParent, error, unsupported) {
  const pending = /* @__PURE__ */ new Map();
  let next = 0;
  let stopped = false;
  const DATA = "ERR_FIREFLY_DIALOG_DATA";
  function alive(ready = true) {
    if (stopped || isExited()) {
      stop();
      throw error("ERR_FIREFLY_APP_EXITED", "Dialog application has exited");
    }
    if (ready && !isReady()) {
      throw error("ERR_FIREFLY_NOT_READY", "Dialogs require app readiness");
    }
  }
  function text(value, name, maximum = 32768) {
    if (typeof value !== "string") {
      throw error("ERR_INVALID_ARG_TYPE", name + " must be a string");
    }
    for (let i = 0; i < value.length; i++) {
      const unit = value.charCodeAt(i);
      if (unit >= 55296 && unit <= 56319) {
        const low = value.charCodeAt(++i);
        if (!(low >= 56320 && low <= 57343)) {
          throw error("ERR_INVALID_ARG_VALUE", name + " contains an unpaired surrogate");
        }
      } else if (unit >= 56320 && unit <= 57343) {
        throw error("ERR_INVALID_ARG_VALUE", name + " contains an unpaired surrogate");
      }
    }
    if (value.includes("\0")) {
      throw error("ERR_INVALID_ARG_VALUE", name + " contains NUL");
    }
    if (Buffer.byteLength(value, "utf8") > maximum) {
      throw error("ERR_OUT_OF_RANGE", name + " exceeds its UTF-8 budget");
    }
    return value;
  }
  function boolean(value, name) {
    if (typeof value !== "boolean") {
      throw error("ERR_INVALID_ARG_TYPE", name + " must be a boolean");
    }
    return value;
  }
  function integer(value, name, min, max) {
    if (!Number.isInteger(value) || value < min || value > max) {
      throw error("ERR_OUT_OF_RANGE", name + " is out of range");
    }
    return value;
  }
  function record(value, label) {
    if (!value || typeof value !== "object" || Array.isArray(value)) {
      throw error("ERR_INVALID_ARG_TYPE", label + " must be an object");
    }
  }
  function normalize(kind, source) {
    if (source === void 0 || source === null) {
      source = kind === "open" ? { title: "Open" } : kind === "save" ? { title: "Save" } : {};
    }
    record(source, "Dialog options");
    const common = ["title", "message"];
    const keys = kind === "message" ? [
      ...common,
      "type",
      "detail",
      "buttons",
      "defaultId",
      "cancelId",
      "checkboxLabel",
      "checkboxChecked",
      "noLink",
      "normalizeAccessKeys"
    ] : [
      ...common,
      "buttonLabel",
      "defaultPath",
      "filters",
      "properties",
      ...kind === "save" ? ["nameFieldLabel", "showsTagField"] : []
    ];
    for (const key of Reflect.ownKeys(source)) {
      if (!keys.includes(key)) {
        unsupported("dialog option " + String(key));
      }
    }
    const input = {};
    for (const key of keys) {
      input[key] = source[key];
    }
    const output = {};
    const strings = kind === "message" ? ["title", "message", "detail", "checkboxLabel"] : [
      "title",
      "message",
      "buttonLabel",
      "defaultPath",
      ...kind === "save" ? ["nameFieldLabel"] : []
    ];
    for (const key of strings) {
      output[key] = text(input[key] === void 0 ? "" : input[key], key);
    }
    if (kind === "message") {
      output.type = input.type === void 0 ? "none" : input.type;
      if (!["none", "info", "error", "question", "warning"].includes(output.type)) {
        throw error("ERR_INVALID_ARG_VALUE", "Invalid message box type");
      }
      const buttons = input.buttons === void 0 ? ["OK"] : input.buttons;
      if (!Array.isArray(buttons) || buttons.length > 32) {
        throw error("ERR_INVALID_ARG_TYPE", "buttons must be an array with at most 32 entries");
      }
      output.buttons = Array.from(buttons, (button) => text(button, "button", 1024));
      if (!output.buttons.length) {
        output.buttons.push("OK");
      }
      output.defaultId = integer(
        input.defaultId === void 0 ? -1 : input.defaultId,
        "defaultId",
        -1,
        output.buttons.length - 1
      );
      let cancelId = input.cancelId;
      if (cancelId === void 0 || cancelId === null) {
        cancelId = output.defaultId === 0 && output.buttons.length > 1 ? 1 : 0;
        const found = output.buttons.findIndex(
          (button) => ["cancel", "no"].includes(button.toLowerCase())
        );
        if (found !== -1) {
          cancelId = found;
        }
      }
      output.cancelId = integer(cancelId, "cancelId", 0, output.buttons.length - 1);
      for (const key of ["checkboxChecked", "noLink", "normalizeAccessKeys"]) {
        output[key] = boolean(input[key] === void 0 ? false : input[key], key);
      }
      if (output.checkboxChecked && !output.checkboxLabel) {
        throw error("ERR_INVALID_ARG_VALUE", "checkboxChecked requires checkboxLabel");
      }
    } else {
      const properties = input.properties === void 0 ? kind === "open" ? ["openFile"] : [] : input.properties;
      const allowed = kind === "open" ? [
        "openFile",
        "openDirectory",
        "multiSelections",
        "showHiddenFiles",
        "createDirectory",
        "noResolveAliases",
        "treatPackageAsDirectory"
      ] : ["showHiddenFiles", "createDirectory", "treatPackageAsDirectory"];
      if (!Array.isArray(properties) || properties.length > 16) {
        throw error("ERR_INVALID_ARG_TYPE", "properties must be an array");
      }
      output.properties = Array.from(properties, (property) => {
        if (!allowed.includes(property)) {
          unsupported("dialog property " + String(property));
        }
        return property;
      });
      if (kind === "open" && !output.properties.some(
        (property) => property === "openFile" || property === "openDirectory"
      )) {
        throw error("ERR_INVALID_ARG_VALUE", "Open dialog requires a file or directory selector");
      }
      const filters = input.filters === void 0 ? [] : input.filters;
      if (!Array.isArray(filters) || filters.length > 64) {
        throw error("ERR_INVALID_ARG_TYPE", "filters must be an array with at most 64 entries");
      }
      output.filters = Array.from(filters, (filter) => {
        record(filter, "Filter");
        for (const key of Reflect.ownKeys(filter)) {
          if (key !== "name" && key !== "extensions") {
            unsupported("dialog filter " + String(key));
          }
        }
        const name = text(filter.name, "filter name");
        const extensions = filter.extensions;
        if (!Array.isArray(extensions) || !extensions.length || extensions.length > 64) {
          throw error("ERR_INVALID_ARG_TYPE", "extensions must contain 1 to 64 entries");
        }
        return {
          name,
          extensions: Array.from(extensions, (extension) => {
            const value = text(extension, "extension", 256);
            if (!value || value !== "*" && /[.*\\/]/.test(value)) {
              throw error("ERR_INVALID_ARG_VALUE", "Invalid file extension");
            }
            return value;
          })
        };
      });
      if (kind === "save") {
        output.showsTagField = boolean(
          input.showsTagField === void 0 ? true : input.showsTagField,
          "showsTagField"
        );
      }
    }
    const json = JSON.stringify(output);
    if (Buffer.byteLength(json, "utf8") > 262144) {
      throw error("ERR_OUT_OF_RANGE", "Dialog options exceed their JSON budget");
    }
    return { options: output, json };
  }
  function parentId(parent) {
    if (parent == null) {
      return 0;
    }
    const id = resolveParent(parent);
    if (!Number.isInteger(id) || id <= 0 || id > 2147483647) {
      throw error("ERR_INVALID_ARG_TYPE", "Parent must be a live BrowserWindow");
    }
    return id;
  }
  function prepare(kind, args, ready = true) {
    alive(ready);
    let parent;
    let options;
    if (args.length > 2) {
      throw error("ERR_INVALID_ARG_TYPE", "Too many dialog arguments");
    }
    if (args.length === 2) {
      parent = args[0];
      options = args[1];
    } else {
      const first = args[0];
      if (first != null) {
        const id2 = resolveParent(first);
        if (id2 === void 0) {
          options = first;
        } else {
          parent = first;
        }
      }
    }
    const snapshot = normalize(kind, options);
    const id = parentId(parent);
    alive(ready);
    return { ...snapshot, parent, parentId: id };
  }
  function cancel(id) {
    try {
      native.dialogCancel?.(id);
    } catch {
    }
  }
  function request(kind, args) {
    const snapshot = prepare(kind, args);
    if (pending.size >= 16 || next >= 2147483647) {
      throw error("ERR_FIREFLY_DIALOG_LIMIT", "Dialog request budget exhausted");
    }
    const id = ++next;
    return new Promise((resolve, reject) => {
      const entry = {
        kind,
        options: snapshot.options,
        resolve,
        reject,
        admitted: false,
        early: void 0
      };
      pending.set(id, entry);
      try {
        const start = native.dialogStart;
        if (typeof start !== "function" || typeof native.dialogCancel !== "function" || typeof native.dialogShutdown !== "function") {
          unsupported("dialog native ownership");
        }
        if (parentId(snapshot.parent) !== snapshot.parentId) {
          throw error("ERR_INVALID_ARG_VALUE", "Dialog parent identity changed");
        }
        alive();
        start.call(native, id, kind, snapshot.parentId, snapshot.json);
        if (pending.get(id) !== entry || stopped || isExited()) {
          cancel(id);
          alive();
          return;
        }
        if (parentId(snapshot.parent) !== snapshot.parentId) {
          throw error("ERR_INVALID_ARG_VALUE", "Dialog parent identity changed");
        }
        alive();
        entry.admitted = true;
        if (entry.early !== void 0) {
          queueMicrotask(() => dispatch(entry.early));
        }
      } catch (failure) {
        if (pending.get(id) === entry) {
          pending.delete(id);
          cancel(id);
          reject(failure);
        } else {
          cancel(id);
        }
      }
    });
  }
  function selectedPath(value) {
    const path = text(value, "Selected path");
    if (!path.startsWith("/") && !/^[a-z]:[\\/]/i.test(path) && !/^\\\\[^\\/]+[\\/][^\\/]+(?:[\\/]|$)/.test(path)) {
      throw Error("Selected path must be absolute");
    }
    return path;
  }
  function result(entry, value) {
    const snapshot = resultSnapshot(entry, value);
    if (Buffer.byteLength(JSON.stringify(snapshot), "utf8") > 262144) {
      throw Error("Dialog result exceeds its JSON budget");
    }
    return snapshot;
  }
  function resultSnapshot(entry, value) {
    record(value, "Dialog result");
    const keys = entry.kind === "message" ? ["response", "checkboxChecked"] : entry.kind === "open" ? ["canceled", "filePaths"] : ["canceled", "filePath"];
    if (Reflect.ownKeys(value).some((key) => !keys.includes(key))) {
      throw Error("Unknown dialog result field");
    }
    if (entry.kind === "message") {
      return {
        response: integer(value.response, "response", 0, entry.options.buttons.length - 1),
        checkboxChecked: boolean(value.checkboxChecked, "checkboxChecked")
      };
    }
    const canceled = boolean(value.canceled, "canceled");
    if (entry.kind === "open") {
      const paths = value.filePaths;
      if (!Array.isArray(paths) || paths.length > 256) {
        throw Error("Invalid selected paths");
      }
      const filePaths = Array.from(paths, selectedPath);
      if (canceled && filePaths.length || !canceled && !filePaths.length) {
        throw Error("Inconsistent open result");
      }
      if (!entry.options.properties.includes("multiSelections") && filePaths.length > 1) {
        throw Error("Unexpected multiple selection");
      }
      return { canceled, filePaths };
    }
    const filePath = canceled ? text(value.filePath, "Selected path") : selectedPath(value.filePath);
    if (canceled !== (filePath === "")) {
      throw Error("Inconsistent save result");
    }
    return { canceled, filePath };
  }
  function dispatch(packet) {
    const entry = pending.get(packet?.id);
    if (!entry || stopped || isExited()) {
      return;
    }
    if (!entry.admitted) {
      if (entry.early !== void 0) {
        pending.delete(packet.id);
        cancel(packet.id);
        entry.reject(error(DATA, "Duplicate early dialog completion"));
      } else {
        try {
          entry.early = JSON.parse(JSON.stringify(packet));
        } catch {
          pending.delete(packet.id);
          cancel(packet.id);
          entry.reject(error(DATA, "Invalid dialog completion"));
        }
      }
      return;
    }
    try {
      const hasError = Object.hasOwn(packet, "error");
      if (hasError === Object.hasOwn(packet, "value")) {
        throw Error("Ambiguous dialog completion");
      }
      let failure;
      let value;
      if (hasError) {
        record(packet.error, "Dialog error");
        const code = text(packet.error.code, "Error code", 256);
        const message = text(packet.error.message, "Error message");
        failure = error(code, message);
      } else {
        value = result(entry, packet.value);
      }
      alive();
      if (pending.get(packet.id) !== entry) {
        return;
      }
      pending.delete(packet.id);
      if (failure) {
        entry.reject(failure);
      } else {
        entry.resolve(value);
      }
    } catch {
      if (pending.get(packet.id) !== entry) {
        return;
      }
      pending.delete(packet.id);
      cancel(packet.id);
      entry.reject(error(DATA, "Invalid native dialog completion"));
    }
  }
  function sync(args, ready) {
    const snapshot = prepare("message", args, ready);
    const show = native.dialogMessageSync;
    if (typeof show !== "function") {
      return unsupported("dialog.showMessageBoxSync native provider");
    }
    if (parentId(snapshot.parent) !== snapshot.parentId) {
      throw error("ERR_INVALID_ARG_VALUE", "Dialog parent identity changed");
    }
    alive(ready);
    const raw = show.call(native, snapshot.parentId, snapshot.json);
    alive(ready);
    if (parentId(snapshot.parent) !== snapshot.parentId) {
      throw error("ERR_INVALID_ARG_VALUE", "Dialog parent identity changed");
    }
    try {
      return result({ kind: "message", options: snapshot.options }, JSON.parse(raw)).response;
    } catch {
      throw error(DATA, "Invalid synchronous dialog result");
    }
  }
  function receiver(value) {
    if (value !== dialog) {
      throw error("ERR_INVALID_ARG_TYPE", "Illegal dialog receiver");
    }
  }
  const dialog = {
    showMessageBox(...args) {
      receiver(this);
      return request("message", args);
    },
    showOpenDialog(...args) {
      receiver(this);
      return request("open", args);
    },
    showSaveDialog(...args) {
      receiver(this);
      return request("save", args);
    },
    showMessageBoxSync(...args) {
      receiver(this);
      return sync(args, true);
    },
    showErrorBox(title, content) {
      receiver(this);
      sync(
        [{ type: "error", title: text(title, "title"), message: text(content, "content") }],
        false
      );
    },
    showOpenDialogSync() {
      receiver(this);
      return unsupported("dialog.showOpenDialogSync");
    },
    showSaveDialogSync() {
      receiver(this);
      return unsupported("dialog.showSaveDialogSync");
    },
    showCertificateTrustDialog() {
      receiver(this);
      return unsupported("dialog.showCertificateTrustDialog");
    }
  };
  function stop() {
    if (stopped) {
      return;
    }
    stopped = true;
    const entries = [...pending];
    pending.clear();
    for (const [id, entry] of entries) {
      entry.reject(error("ERR_FIREFLY_APP_EXITED", "Dialog application has exited"));
      cancel(id);
    }
    try {
      native.dialogShutdown?.();
    } catch {
    }
  }
  return { dialog, dispatch, stop };
}

// firefly/core/api.mjs
function createFireflyApi(native, EventEmitter, paths, urls, files, streams, transport) {
  const error = (code, message) => Object.assign(new Error(message), { code });
  const unsupported = (name) => {
    throw error("ERR_FIREFLY_UNSUPPORTED_API", `${name} is not implemented`);
  };
  const channels = createMessageChannels(native, EventEmitter, error);
  const windows = /* @__PURE__ */ new Map();
  const allContents = /* @__PURE__ */ new Map();
  const contentsToken = Symbol("WebContents constructor");
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
  class App extends EventEmitter {
    enableSandbox() {
      if (exited) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application has exited");
      }
      if (ready) {
        throw error(
          "ERR_FIREFLY_SANDBOX_TOO_LATE",
          "enableSandbox must be called before readiness"
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
        throw error("ERR_FIREFLY_NOT_READY", "getSystemLocale requires app readiness");
      }
      return native.appLocaleRead(1);
    }
    getLocale() {
      if (!ready) {
        throw error("ERR_FIREFLY_NOT_READY", "getLocale requires app readiness");
      }
      return native.appLocaleRead(2);
    }
    getLocaleCountryCode() {
      return native.appLocaleRead(3);
    }
    getPath(name) {
      if (typeof name !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Path name must be a string");
      }
      return name === "crashDumps" ? native.crashDirectory() : native.appPathGet(name);
    }
    setPath(name, value) {
      if (typeof name !== "string" || typeof value !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Path name/value must be strings");
      }
      if (name === "crashDumps") {
        native.crashDirectory(value);
      } else {
        native.appPathSet(name, value);
      }
    }
    getAppPath() {
      return native.appPath;
    }
    setAppLogsPath(value) {
      if (value !== void 0 && typeof value !== "string") {
        throw error("ERR_INVALID_ARG_TYPE", "Log path must be a string");
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
      systemPreferencesService.stop();
      network.stop();
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
      allContents.clear();
      try {
        for (const win of [...windows.values()]) {
          native.destroyNow(win.id);
          win._destroyed = true;
          win._closing = false;
          windows.delete(win.id);
          win.webContents._reject(error("ERR_ABORTED", "Application exiting"));
          win.webContents.emit("destroyed");
          win.emit("closed");
        }
        app.emit("quit", event(), exitCode);
      } finally {
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
      for (const [id, window] of windows) {
        if (window === value) {
          if (window._destroyed || window._closing) {
            throw error("ERR_FIREFLY_OBJECT_DESTROYED", "Dialog parent closed");
          }
          return id;
        }
      }
      if (value instanceof BrowserWindow) {
        throw error("ERR_FIREFLY_OBJECT_DESTROYED", "Dialog parent closed");
      }
      return void 0;
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
    transport
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
  Object.defineProperty(app, "commandLine", {
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
      return window && !window._closing && !window._destroyed ? window : void 0;
    }
  );
  const { Menu, MenuItem } = menus;
  const contentTracing = createTracing(native, () => ready, files, paths, error, unsupported);
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
    dispatch: dispatchIpc,
    dispatchEvent: dispatchIpcEvent
  } = createMainIpc(native, EventEmitter, error);
  function finishQuit() {
    const e = event();
    app.emit("will-quit", e);
    if (e.defaultPrevented) {
      quitting = false;
      return;
    }
    exited = true;
    systemPreferencesService.stop();
    network.stop();
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
    allContents.clear();
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
    constructor(win, token) {
      super();
      if (token !== contentsToken) {
        throw error("ERR_ILLEGAL_CONSTRUCTOR", "WebContents is created by its native owner");
      }
      Object.defineProperty(this, "id", { value: win.id, enumerable: true });
      this._window = win;
      this._url = "";
      this._title = "";
      this._pending = null;
      this._ipcFrame = null;
      allContents.set(win.id, this);
    }
    postMessage() {
      unsupported("webContents.postMessage port/data delivery");
    }
    send(channel, ...args) {
      alive(this._window);
      if (typeof channel !== "string" || channel.length > 256 || args.length > 64) {
        throw error(
          "ERR_INVALID_ARG_TYPE",
          "send requires a bounded channel and at most 64 arguments"
        );
      }
      native.sendIpc(this.id, native.encodeIpc([channel, ...args]));
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
      if (!options || typeof options !== "object" || Array.isArray(options)) {
        throw error("ERR_INVALID_ARG_TYPE", "webContents.close options must be an object");
      }
      for (const key of Object.keys(options)) {
        if (key !== "waitForBeforeUnload" || options[key] !== false) {
          unsupported(`webContents.close ${key}`);
        }
      }
      this._window.destroy();
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
          native.load(this.id, parsed.href);
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
      const prefs = preferences(options.webPreferences ?? {});
      const additionalArguments = prefs.additionalArguments;
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
      if (exited) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application exited during window preparation");
      }
      Object.defineProperty(this, "id", {
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
          additionalArguments
        )
      });
      this.webContents = new WebContents(this, contentsToken);
      Object.defineProperty(this, "contentView", {
        value: views.contentView(this),
        enumerable: true
      });
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
      if (!contents || allContents.get(contents.id) !== contents) {
        return null;
      }
      if (typeof native.ownerWindow === "function") {
        const owner = windows.get(native.ownerWindow(contents.id));
        return owner && !owner._closing && !owner._destroyed ? owner : null;
      }
      return [...windows.values()].find((win) => !win._closing && win.webContents === contents) ?? views.ownerOf(contents) ?? null;
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
  views = createViews(
    native,
    EventEmitter,
    (owner) => new WebContents(owner, contentsToken),
    preferences,
    () => {
      if (exited) {
        throw error("ERR_FIREFLY_APP_EXITED", "Application is exiting");
      }
      if (!ready) {
        throw error("ERR_FIREFLY_NOT_READY", "Cannot create a view before app readiness");
      }
    },
    error,
    unsupported
  );
  const webContents = {
    fromId(id) {
      return allContents.get(id) ?? void 0;
    },
    getAllWebContents() {
      return [...allContents.values()].filter((contents) => !contents.isDestroyed());
    },
    getFocusedWebContents() {
      return allContents.get(native.focusedContents()) ?? null;
    }
  };
  native.setHandler((id, type, data) => {
    if (type === "menu-click") {
      menus.dispatch(id, JSON.parse(data));
      return;
    }
    if (type === "protocol-request" || type === "protocol-cancel") {
      protocols.dispatch(type, JSON.parse(data));
      return;
    }
    if (type === "ready") {
      ready = true;
      screens.ready();
      power.ready();
      app.emit("ready");
      resolveReady();
      return;
    }
    if (type === "preferences-notification") {
      systemPreferencesService.dispatch(JSON.parse(data));
      return;
    }
    if (type === "shell-result") {
      desktopShell.dispatch(JSON.parse(data));
      return;
    }
    if (type === "dialog-result") {
      dialogs.dispatch(JSON.parse(data));
      return;
    }
    if (type === "global-shortcut-event") {
      shortcuts.dispatch(JSON.parse(data));
      return;
    }
    if (type === "power-monitor-event") {
      power.dispatch(JSON.parse(data));
      return;
    }
    if (type === "notification-result") {
      notifications.dispatch(JSON.parse(data));
      return;
    }
    if (type === "desktop-capture-result") {
      capture.dispatch(JSON.parse(data));
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
    const win = windows.get(id) ?? views.records.get(id);
    if (!win) {
      return;
    }
    const contents = win.webContents;
    if (type === "ipc-send") {
      dispatchIpcEvent(win, typeof data === "string" ? JSON.parse(data) : data);
      return;
    }
    if (type === "ipc-invoke") {
      dispatchIpc(win, typeof data === "string" ? JSON.parse(data) : data);
      return;
    }
    if (type === "close-request" || type === "contents-close-request") {
      return win._closeRequest();
    }
    if (type === "contents-destroyed") {
      cancelIpc(win);
      contents._reject(error("ERR_ABORTED", "Web contents closed"));
      allContents.delete(id);
      views.destroyed(id);
      return;
    }
    if (type === "closed") {
      views.detachWindow(win);
      allContents.delete(id);
      win._destroyed = true;
      win._closing = false;
      windows.delete(id);
      contents._reject(error("ERR_ABORTED", "Window closed"));
      contents.emit("destroyed");
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
      contents.emit("page-title-updated", event(), data, true);
      if (windows.has(id)) {
        const e = event();
        win.emit("page-title-updated", e, data, true);
        if (!e.defaultPrevented && !win._closing) {
          native.title(id, data);
        }
      }
    } else if (type === "paint" && windows.has(id)) {
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
    Menu,
    MenuItem,
    contentTracing,
    crashReporter,
    protocol,
    session,
    BrowserWindow,
    WebContentsView: views.WebContentsView,
    webContents,
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
return createFireflyApi;
})()
