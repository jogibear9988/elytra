import createResolveHooks from './module-hooks.mjs';

/** Node module utilities over the same native loader used by ordinary CommonJS. */
export default function createModuleUtilities(host) {
  const { isAbsolute, dirname, resolve } = host.path;
  const { fileURLToPath } = host.url;

  function createRequire(filename) {
    let path = filename;
    if (typeof filename !== 'string' || !isAbsolute(filename)) {
      try {
        // The URL module checks native URL identity and rejects URL-shaped objects,
        // invalid schemes/hosts and encoded separators before any file lookup.
        path = fileURLToPath(filename);
      } catch {
        const error = new TypeError(
          'filename must be a file URL object, file URL string, or absolute path string'
        );
        error.code = 'ERR_INVALID_ARG_VALUE';
        throw error;
      }
    }
    return host.createRequire(path);
  }

  function isBuiltin(name) {
    return typeof name === 'string' && host.isBuiltin(name);
  }

  function failure(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    return error;
  }

  function requestString(request) {
    if (typeof request !== 'string') {
      throw failure('ERR_INVALID_ARG_TYPE', 'Module ID must be a string');
    }
    if (!request) {
      throw failure('ERR_INVALID_ARG_VALUE', 'Module ID must not be empty');
    }
  }

  function relative(request) {
    return (
      request === '.' ||
      request === '..' ||
      request.startsWith('./') ||
      request.startsWith('../') ||
      (host.path.sep === '\\' && request.startsWith('.\\')) ||
      request.startsWith('..\\')
    );
  }

  // This is the actual constructor/prototype of native-loaded CommonJS parents.
  // Native loading retains its cache/cycle/rollback machinery; mutable public
  // dispatch methods are consulted before those operations, like Node's loader.
  function Module(id = '', parent) {
    if (typeof id !== 'string') {
      throw failure('ERR_INVALID_ARG_TYPE', 'Module id must be a string');
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

  Module.prototype.require = function require(id) {
    requestString(id);
    return Module._load(id, this, false);
  };

  Module._cache = host.cache;
  // Mite has no Node installation's global module directories. Applications may
  // supply genuine lookup roots here or on their Module instance's paths array.
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
      return [parent?.id && parent.filename ? dirname(parent.filename) : '.'];
    }
    const paths = parent?.paths === undefined ? [] : parent.paths;
    if (!Array.isArray(paths) || !Array.isArray(Module.globalPaths)) {
      throw failure('ERR_INVALID_ARG_VALUE', 'Module lookup paths must be arrays');
    }
    return [...paths, ...Module.globalPaths];
  };

  Module._resolveFilename = function resolveFilename(request, parent, isMain, options) {
    requestString(request);
    // Builtins do not consult filesystem lookup paths or resolution options.
    // Keep the native hook chain in this existing runtime boundary so registered
    // redirects still compose with ordinary CommonJS requests.
    if (isBuiltin(request)) {
      return host.resolveFilename(request, parent?.filename || resolve('noop.js'), []);
    }
    if (options?.conditions !== undefined) {
      throw failure(
        'ERR_MITE_UNSUPPORTED_API',
        'Custom CommonJS resolution conditions are not implemented',
        Error
      );
    }
    let paths;
    if (options?.paths !== undefined) {
      if (!Array.isArray(options.paths)) {
        throw failure('ERR_INVALID_ARG_VALUE', 'options.paths must be an array');
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
    const filename = parent?.filename || resolve('noop.js');
    return host.resolveFilename(request, filename, paths ?? []);
  };

  Module._load = function load(request, parent, isMain) {
    requestString(request);
    // Node-prefixed loading is a distinct builtin operation: it bypasses the
    // mutable filename resolver and cache, but still uses registered native
    // resolve hooks. Unknown prefixed loads retain the builtin error rather
    // than the MODULE_NOT_FOUND diagnostic used by require.resolve.
    const filename = request.startsWith('node:')
      ? host.resolveFilename(request, parent?.filename || resolve('noop.js'), [], true)
      : Module._resolveFilename(request, parent, isMain);
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
