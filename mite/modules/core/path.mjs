/**
 * Pure lexical POSIX and Windows path operations. Platform selection comes from the injected
 * process object; these functions do not perform filesystem I/O.
 *
 * Native dependencies are supplied by the embedding loader. One factory call
 * constructs the shared builtin instance for one runtime/realm.
 */
export default function createPathModule(host) {
  const process = host.process;

  function string(value, name = 'path') {
    if (typeof value !== 'string') {
      const error = new TypeError(`The ${name} argument must be a string`);
      error.code = 'ERR_INVALID_ARG_TYPE';
      throw error;
    }
    return value;
  }

  function separator(character, windows) {
    return character === '/' || (windows && character === '\\');
  }

  /** Parse lexical roots without consulting the filesystem or changing drive-relative semantics. */
  function rootOf(path, windows) {
    if (!windows) {
      return { end: path[0] === '/' ? 1 : 0, device: '', absolute: path[0] === '/' };
    }
    if (separator(path[0], true)) {
      if (separator(path[1], true) && path.length > 2 && !separator(path[2], true)) {
        let end = 2;
        while (end < path.length && !separator(path[end], true)) {
          ++end;
        }
        const server = path.slice(2, end);
        while (end < path.length && separator(path[end], true)) {
          ++end;
        }
        const shareStart = end;
        while (end < path.length && !separator(path[end], true)) {
          ++end;
        }
        if (end > shareStart) {
          const share = path.slice(shareStart, end);
          return {
            end: end < path.length ? end + 1 : end,
            device: '\\\\' + server + '\\' + share,
            absolute: true
          };
        }
      }
      return { end: 1, device: '', absolute: true };
    }
    if (/^[a-zA-Z]:/.test(path)) {
      const absolute = separator(path[2], true);
      return { end: absolute ? 3 : 2, device: path.slice(0, 2), absolute };
    }
    return { end: 0, device: '', absolute: false };
  }

  function tail(path, aboveRoot, windows) {
    const result = [];
    for (const component of path.split(windows ? /[\\/]+/ : /\/+/)) {
      if (!component || component === '.') {
        continue;
      }
      if (component === '..') {
        if (result.length && result[result.length - 1] !== '..') {
          result.pop();
        } else if (aboveRoot) {
          result.push(component);
        }
      } else {
        result.push(component);
      }
    }
    return result.join(windows ? '\\' : '/');
  }

  function extension(base) {
    const dot = base.lastIndexOf('.');
    return dot > 0 && base !== '..' ? base.slice(dot) : '';
  }

  function implementation(windows) {
    const sep = windows ? '\\' : '/';
    const api = {};

    function cwd() {
      const value = process.cwd();
      if (windows || host.platform !== 'win32') {
        return value;
      }
      const slashes = value.replaceAll('\\', '/');
      return /^[a-zA-Z]:/.test(slashes) ? slashes.slice(2) : slashes;
    }

    api.resolve = function resolve(...paths) {
      let device = '';
      let absolute = false;
      let combined = '';
      for (let index = paths.length - 1; index >= -1; --index) {
        let value;
        if (index >= 0) {
          value = string(paths[index], `paths[${index}]`);
        } else if (windows && device) {
          value = process.env['=' + device] || cwd();
          if (value.slice(0, 2).toLowerCase() !== device.toLowerCase() && value[2] === '\\') {
            value = device + '\\';
          }
        } else {
          value = cwd();
        }
        if (!value) {
          continue;
        }
        const root = rootOf(value, windows);
        if (windows && root.device) {
          if (device && device.toLowerCase() !== root.device.toLowerCase()) {
            continue;
          }
          if (!device) {
            device = root.device;
          }
        }
        if (!absolute) {
          combined = value.slice(root.end) + sep + combined;
          absolute = root.absolute;
        }
        if (absolute && (!windows || device)) {
          break;
        }
      }
      const normalized = tail(combined, !absolute, windows);
      return device + (absolute ? sep : '') + normalized || '.';
    };

    api.normalize = function normalize(path) {
      string(path);
      if (!path) {
        return '.';
      }
      const root = rootOf(path, windows);
      const trailing = separator(path[path.length - 1], windows);
      let normalized = tail(path.slice(root.end), !root.absolute, windows);
      if (!normalized && !root.absolute) {
        normalized = '.';
      }
      if (normalized && trailing) {
        normalized += sep;
      }
      // Keep a relative input relative after dot-segment removal reveals a
      // drive prefix or a colon before a separator/end on Windows.
      if (
        windows &&
        !root.absolute &&
        !root.device &&
        (/^[a-zA-Z]:/.test(normalized) || /:(?:\\|$)/.test(normalized))
      ) {
        normalized = '.\\' + normalized;
      }
      return root.device + (root.absolute ? sep : '') + normalized;
    };

    api.isAbsolute = function isAbsolute(path) {
      return rootOf(string(path), windows).absolute;
    };

    api.join = function join(...paths) {
      const values = [];
      for (const value of paths) {
        string(value);
        if (value) {
          values.push(value);
        }
      }
      if (!values.length) {
        return '.';
      }
      let combined = values.join(sep);
      if (windows) {
        const first = values[0];
        const unc =
          separator(first[0], true) &&
          separator(first[1], true) &&
          first.length > 2 &&
          !separator(first[2], true);
        if (!unc) {
          combined = combined.replace(/^[\\/]{2,}/, '\\');
        }
      }
      return api.normalize(combined);
    };

    api.relative = function relative(from, to) {
      string(from, 'from');
      string(to, 'to');
      if (from === to) {
        return '';
      }
      const source = api.resolve(from);
      const target = api.resolve(to);
      if (windows ? source.toLowerCase() === target.toLowerCase() : source === target) {
        return '';
      }
      const sourceParts = source.split(sep).filter(Boolean);
      const targetParts = target.split(sep).filter(Boolean);
      let common = 0;
      while (
        common < sourceParts.length &&
        common < targetParts.length &&
        (windows
          ? sourceParts[common].toLowerCase() === targetParts[common].toLowerCase()
          : sourceParts[common] === targetParts[common])
      ) {
        ++common;
      }
      if (windows && common === 0 && sourceParts.length && targetParts.length) {
        return target;
      }
      return [...Array(sourceParts.length - common).fill('..'), ...targetParts.slice(common)].join(
        sep
      );
    };

    api.basename = function basename(path, suffix) {
      string(path);
      if (suffix !== undefined) {
        string(suffix, 'suffix');
      }
      if (suffix === path) {
        return '';
      }
      const start = windows && /^[a-zA-Z]:/.test(path) ? 2 : 0;
      let end = path.length;
      while (end > start && separator(path[end - 1], windows)) {
        --end;
      }
      let index = end - 1;
      while (index >= start && !separator(path[index], windows)) {
        --index;
      }
      const base = path.slice(index + 1, end);
      if (!base && suffix && suffix.length <= path.length) {
        return path.slice(start);
      }
      return suffix && suffix.length < base.length && base.endsWith(suffix)
        ? base.slice(0, -suffix.length)
        : base;
    };

    api.extname = function extname(path) {
      return extension(api.basename(string(path)));
    };

    api.dirname = function dirname(path) {
      string(path);
      if (!path) {
        return '.';
      }
      const root = rootOf(path, windows);
      let end = path.length;
      while (end > root.end && separator(path[end - 1], windows)) {
        --end;
      }
      let index = end - 1;
      while (index >= root.end && !separator(path[index], windows)) {
        --index;
      }
      if (!windows && root.absolute && index === 1) {
        return '//';
      }
      if (index <= root.end) {
        return root.end ? path.slice(0, root.end) : '.';
      }
      return path.slice(0, index);
    };

    api.parse = function parse(path) {
      string(path);
      const root = rootOf(path, windows);
      let end = path.length;
      while (end > root.end && separator(path[end - 1], windows)) {
        --end;
      }
      let index = end - 1;
      while (index >= root.end && !separator(path[index], windows)) {
        --index;
      }
      const base = path.slice(Math.max(index + 1, root.end), end);
      const ext = extension(base);
      return {
        root: path.slice(0, root.end),
        dir: index <= root.end ? path.slice(0, root.end) : path.slice(0, index),
        base,
        ext,
        name: ext ? base.slice(0, -ext.length) : base
      };
    };

    api.format = function format(pathObject) {
      if (!pathObject || typeof pathObject !== 'object' || Array.isArray(pathObject)) {
        const error = new TypeError('The pathObject argument must be an object');
        error.code = 'ERR_INVALID_ARG_TYPE';
        throw error;
      }
      const dir = pathObject.dir || pathObject.root;
      let base = pathObject.base;
      if (!base) {
        const ext = pathObject.ext
          ? String(pathObject.ext).startsWith('.')
            ? pathObject.ext
            : '.' + pathObject.ext
          : '';
        base = `${pathObject.name || ''}${ext}`;
      }
      if (!dir) {
        return base;
      }
      return dir === pathObject.root ? `${dir}${base}` : `${dir}${sep}${base}`;
    };

    api.toNamespacedPath = function toNamespacedPath(path) {
      if (!windows || typeof path !== 'string' || path.length === 0) {
        return path;
      }
      const resolved = api.resolve(path);
      if (resolved.length <= 2) {
        return path;
      }
      if (resolved.startsWith('\\\\') && resolved[2] !== '?' && resolved[2] !== '.') {
        return '\\\\?\\UNC\\' + resolved.slice(2);
      }
      if (/^[a-zA-Z]:\\/.test(resolved)) {
        return '\\\\?\\' + resolved;
      }
      return resolved;
    };

    api._makeLong = api.toNamespacedPath;
    api.sep = sep;
    api.delimiter = windows ? ';' : ':';
    return api;
  }

  const posix = implementation(false);
  const win32 = implementation(true);
  posix.posix = win32.posix = posix;
  posix.win32 = win32.win32 = win32;
  return host.platform === 'win32' ? win32 : posix;
}
