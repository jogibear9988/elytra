/** Bounded title-bar styles; native Cocoa owns buttons, appearance and full-size content geometry. */
export default function createTitleBar(native, error, unsupported) {
  return function read(value) {
    if (value === undefined || value === 'default') {
      return false;
    }
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', 'titleBarStyle must be a string');
    }
    if (value !== 'hidden') {
      unsupported(`BrowserWindow titleBarStyle ${value}`);
    }
    if (typeof native.titleBarAvailable !== 'function' || native.titleBarAvailable() !== true) {
      unsupported('BrowserWindow hidden title bar on this window backend');
    }
    return true;
  };
}
