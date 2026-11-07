/* Generated from mite/modules/core/module.mjs; edit the ESM source. */
(function () {
'use strict';
// mite/modules/core/module.mjs
function createModuleUtilities(host) {
  const { isAbsolute } = host.path;
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
  return { createRequire, isBuiltin, builtinModules: Object.freeze(host.builtinModules) };
}
return createModuleUtilities;
})()
