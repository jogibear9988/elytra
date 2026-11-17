/* Generated from mite/modules/core/module.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/module-hooks.mjs
function createResolveHooks(host) {
  const hooks = [];
  function failure(code, message, Type = Error) {
    const error = new Type(message);
    error.code = code;
    return error;
  }
  function registerHooks(options) {
    if (options == null) {
      throw new TypeError("registerHooks requires an options object");
    }
    const { resolve, load } = options;
    for (const [name, value] of [
      ["resolve", resolve],
      ["load", load]
    ]) {
      if (value != null && typeof value !== "function") {
        throw failure("ERR_INVALID_ARG_TYPE", `${name} must be a function`, TypeError);
      }
    }
    if (load != null) {
      throw failure(
        "ERR_MITE_UNSUPPORTED_API",
        "Synchronous module load hooks are not implemented"
      );
    }
    const record = { resolve };
    if (resolve) {
      if (hooks.length >= 64) {
        throw failure(
          "ERR_MITE_RESOLVE_HOOK_LIMIT",
          "At most 64 active resolve hooks are supported"
        );
      }
      hooks.push(record);
    }
    return {
      deregister() {
        const index = hooks.indexOf(record);
        if (index !== -1) {
          hooks.splice(index, 1);
        }
      }
    };
  }
  function dispatch(specifier, parentURL, require2, importAttributes, lookupPaths) {
    if (!hooks.length) {
      return void 0;
    }
    const context = {
      parentURL,
      conditions: require2 ? ["require", "node", "node-addons"] : ["node", "import", "node-addons"]
    };
    if (!require2) {
      context.importAttributes = importAttributes;
    }
    const chain = hooks.map((record) => record.resolve);
    function invoke(index, request, input) {
      if (typeof request !== "string") {
        throw failure("ERR_INVALID_ARG_TYPE", "Module specifier must be a string", TypeError);
      }
      if (index < 0) {
        return host.defaultResolve(request, input, require2, lookupPaths);
      }
      let called = false;
      const nextResolve = (nextRequest, changes) => {
        called = true;
        return invoke(
          index - 1,
          nextRequest,
          changes === void 0 ? input : { ...input, ...changes }
        );
      };
      const result = Reflect.apply(chain[index], void 0, [request, input, nextResolve]);
      if (result === null || typeof result !== "object" || typeof result.then === "function") {
        throw failure(
          "ERR_INVALID_RETURN_VALUE",
          "Resolve hook must synchronously return an object",
          TypeError
        );
      }
      if (!called && result.shortCircuit !== true) {
        throw failure(
          "ERR_INVALID_RETURN_PROPERTY_VALUE",
          "Resolve hook must call nextResolve or shortCircuit",
          TypeError
        );
      }
      if (typeof result.url !== "string") {
        throw failure(
          "ERR_INVALID_RETURN_PROPERTY_VALUE",
          "Resolve hook url must be a string",
          TypeError
        );
      }
      return result;
    }
    return invoke(chain.length - 1, specifier, context);
  }
  host.setResolveDispatcher(dispatch);
  return registerHooks;
}

// mite/modules/core/module.mjs
function createModuleUtilities(host) {
  const { isAbsolute, dirname, resolve } = host.path;
  const { fileURLToPath } = host.url;
  function createRequire(filename) {
    let path = filename;
    if (typeof filename !== "string" || !isAbsolute(filename)) {
      try {
        path = fileURLToPath(filename);
      } catch {
        const error = new TypeError(
          "filename must be a file URL object, file URL string, or absolute path string"
        );
        error.code = "ERR_INVALID_ARG_VALUE";
        throw error;
      }
    }
    return host.createRequire(path);
  }
  function isBuiltin(name) {
    return typeof name === "string" && host.isBuiltin(name);
  }
  function failure(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    return error;
  }
  function requestString(request) {
    if (typeof request !== "string") {
      throw failure("ERR_INVALID_ARG_TYPE", "Module ID must be a string");
    }
    if (!request) {
      throw failure("ERR_INVALID_ARG_VALUE", "Module ID must not be empty");
    }
  }
  function relative(request) {
    return request === "." || request === ".." || request.startsWith("./") || request.startsWith("../") || host.path.sep === "\\" && request.startsWith(".\\") || request.startsWith("..\\");
  }
  function Module(id = "", parent) {
    if (typeof id !== "string") {
      throw failure("ERR_INVALID_ARG_TYPE", "Module id must be a string");
    }
    this.id = id;
    this.path = dirname(id);
    this.exports = {};
    this.parent = parent;
    this.filename = null;
    this.loaded = false;
    this.children = [];
    if (parent && Array.isArray(parent.children)) {
      parent.children.push(this);
    }
  }
  Module.prototype.require = function require2(id) {
    requestString(id);
    return Module._load(id, this, false);
  };
  Module._cache = host.cache;
  Module.globalPaths = [];
  Module._nodeModulePaths = function nodeModulePaths(from) {
    return host.nodeModulePaths(from);
  };
  Module._resolveLookupPaths = function resolveLookupPaths(request, parent) {
    requestString(request);
    if (isBuiltin(request)) {
      return null;
    }
    if (relative(request)) {
      return [parent?.id && parent.filename ? dirname(parent.filename) : "."];
    }
    const paths = parent?.paths === void 0 ? [] : parent.paths;
    if (!Array.isArray(paths) || !Array.isArray(Module.globalPaths)) {
      throw failure("ERR_INVALID_ARG_VALUE", "Module lookup paths must be arrays");
    }
    return [...paths, ...Module.globalPaths];
  };
  Module._resolveFilename = function resolveFilename(request, parent, isMain, options) {
    requestString(request);
    if (isBuiltin(request)) {
      return host.resolveFilename(request, parent?.filename || resolve("noop.js"), []);
    }
    if (options?.conditions !== void 0) {
      throw failure(
        "ERR_MITE_UNSUPPORTED_API",
        "Custom CommonJS resolution conditions are not implemented",
        Error
      );
    }
    let paths;
    if (options?.paths !== void 0) {
      if (!Array.isArray(options.paths)) {
        throw failure("ERR_INVALID_ARG_VALUE", "options.paths must be an array");
      }
      if (relative(request)) {
        paths = options.paths;
      } else {
        paths = [];
        for (const from of options.paths) {
          const candidates = Module._resolveLookupPaths(request, {
            paths: Module._nodeModulePaths(from)
          });
          if (candidates) {
            for (const path of candidates) {
              if (!paths.includes(path)) {
                paths.push(path);
              }
            }
          }
        }
      }
    } else {
      paths = Module._resolveLookupPaths(request, parent);
    }
    const filename = parent?.filename || resolve("noop.js");
    return host.resolveFilename(request, filename, paths ?? []);
  };
  Module._load = function load(request, parent, isMain) {
    requestString(request);
    const filename = request.startsWith("node:") ? host.resolveFilename(request, parent?.filename || resolve("noop.js"), [], true) : Module._resolveFilename(request, parent, isMain);
    return host.loadResolved(filename, parent, isMain);
  };
  Module.Module = Module;
  Module.createRequire = createRequire;
  Module.isBuiltin = isBuiltin;
  Module.builtinModules = Object.freeze(host.builtinModules);
  Module.registerHooks = createResolveHooks(host);
  host.setModuleConstructor(Module);
  return Module;
}
return createModuleUtilities;
})()
