/** Node module utilities over the same native loader used by ordinary CommonJS. */
export default function createModuleUtilities(host) {
  const { isAbsolute } = host.path;
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

  // Enumerate available Mite modules, not the larger set of reserved Node names.
  return { createRequire, isBuiltin, builtinModules: Object.freeze(host.builtinModules) };
}
