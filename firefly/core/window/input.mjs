/** Admit first-click policy and one obsolete Electron flag without inventing a theme API. */
export default function createWindowInput(native, error, unsupported) {
  return function read(options) {
    // Electron 45 no longer reads this former Windows option. Keep the known
    // legacy boolean inert; actual appearance remains nativeTheme's concern.
    const legacyDarkMode = options.experimentalDarkMode;
    if (legacyDarkMode !== undefined && typeof legacyDarkMode !== 'boolean') {
      throw error('ERR_INVALID_ARG_TYPE', 'experimentalDarkMode must be a boolean');
    }
    const value = options.acceptFirstMouse;
    if (value === undefined) {
      return undefined;
    }
    if (typeof value !== 'boolean') {
      throw error('ERR_INVALID_ARG_TYPE', 'acceptFirstMouse must be a boolean');
    }
    if (typeof native.firstMouseAvailable !== 'function' || !native.firstMouseAvailable()) {
      unsupported('BrowserWindow acceptFirstMouse on this window backend');
    }
    return value;
  };
}
