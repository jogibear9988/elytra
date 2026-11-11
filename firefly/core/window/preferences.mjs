/** Shared isolation/preload policy for main windows and standalone WebContentsViews. */
import createZoomPolicy from './zoom.mjs';
const zoom = createZoomPolicy();

export default function createWebPreferences(
  paths,
  isSandboxRequired,
  error,
  unsupported,
  isSpellCheckDisabled
) {
  return function validate(prefs = {}) {
    if (!prefs || typeof prefs !== 'object' || Array.isArray(prefs)) {
      throw error('ERR_INVALID_ARG_TYPE', 'webPreferences must be an object');
    }
    let zoomFactor = 1;
    let autoplayPolicy;
    for (const key of Object.keys(prefs)) {
      if (key === 'preload' || key === 'additionalArguments') {
        continue;
      }
      if (key === 'enableBlinkFeatures') {
        const value = prefs[key];
        if (typeof value !== 'string') {
          throw error(
            'ERR_INVALID_ARG_TYPE',
            'webPreferences.enableBlinkFeatures must be a string'
          );
        }
        // LibWeb exposes CSS.highlights unconditionally in its native CSS IDL;
        // this exact Electron compatibility request needs no engine feature toggle.
        if (value !== 'HighlightAPI') {
          unsupported(`webPreferences.enableBlinkFeatures=${value}`);
        }
        continue;
      }
      if (key === 'autoplayPolicy') {
        const value = prefs[key];
        if (typeof value !== 'string') {
          throw error('ERR_INVALID_ARG_TYPE', 'webPreferences.autoplayPolicy must be a string');
        }
        if (!['no-user-gesture-required', 'user-gesture-required'].includes(value)) {
          unsupported(`webPreferences.autoplayPolicy=${value}`);
        }
        autoplayPolicy = value;
        continue;
      }
      if (key === 'zoomFactor') {
        zoomFactor = zoom.factor(prefs[key]);
        continue;
      }
      if (key === 'spellcheck') {
        const value = prefs[key];
        if (typeof value !== 'boolean') {
          throw error('ERR_INVALID_ARG_TYPE', 'webPreferences.spellcheck must be a boolean');
        }
        // LibWeb receives its actual availability from the native provider. Until
        // per-view providers exist, false is valid only while that provider is absent.
        if (
          value ||
          typeof isSpellCheckDisabled !== 'function' ||
          isSpellCheckDisabled() !== true
        ) {
          unsupported(`webPreferences.spellcheck=${value}`);
        }
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
    return {
      preload: prefs.preload,
      additionalArguments,
      zoomFactor,
      ...(autoplayPolicy === undefined ? {} : { autoplayPolicy })
    };
  };
}
