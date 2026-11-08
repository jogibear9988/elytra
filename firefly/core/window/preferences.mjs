/** Shared isolation/preload policy for main windows and standalone WebContentsViews. */
export default function createWebPreferences(paths, isSandboxRequired, error, unsupported) {
  return function validate(prefs = {}) {
    if (!prefs || typeof prefs !== 'object' || Array.isArray(prefs)) {
      throw error('ERR_INVALID_ARG_TYPE', 'webPreferences must be an object');
    }
    for (const key of Object.keys(prefs)) {
      if (key === 'preload' || key === 'additionalArguments') {
        continue;
      }
      if (!['contextIsolation', 'nodeIntegration', 'sandbox'].includes(key)) {
        unsupported(`webPreferences.${key}`);
      }
      if (key === 'sandbox' && isSandboxRequired() && typeof prefs[key] === 'boolean') {
        // Global enforcement overrides false without mutating caller preferences.
        continue;
      }
      if (prefs[key] !== (key !== 'nodeIntegration')) {
        unsupported(`webPreferences.${key}=${prefs[key]}`);
      }
    }
    if (prefs.preload !== undefined) {
      if (typeof prefs.preload !== 'string') {
        throw error('ERR_INVALID_ARG_TYPE', 'preload must be a string');
      }
      if (!paths.isAbsolute(prefs.preload)) {
        throw error('ERR_INVALID_ARG_VALUE', 'preload must be an absolute path');
      }
      if (paths.extname(prefs.preload) === '.mjs') {
        unsupported('ES module preloads');
      }
    }
    const additionalArguments =
      prefs.additionalArguments === undefined ? [] : prefs.additionalArguments;
    if (
      !Array.isArray(additionalArguments) ||
      additionalArguments.some((value) => typeof value !== 'string')
    ) {
      throw error('ERR_INVALID_ARG_TYPE', 'additionalArguments must be an array of strings');
    }
    if (additionalArguments.length > 64) {
      throw error('ERR_INVALID_ARG_VALUE', 'additionalArguments exceeds 64 entries');
    }
    return { preload: prefs.preload, additionalArguments };
  };
}
