/** Only the fixed preload test action is visible to page code. */
try {
  if (typeof rawIpc !== 'undefined' || typeof ipcRenderer !== 'undefined') {
    throw Error('Raw native capability in page');
  }
  const result = await desktop.check();
  if (result !== true) {
    throw new Error(JSON.stringify(result));
  }
  // The exact native module now redacts to inert data; individual raw methods
  // must still fail publication in the source check above.
  if (
    Object.getPrototypeOf(rawIpc) !== Object.prototype ||
    Reflect.ownKeys(rawIpc).length ||
    !Object.isFrozen(rawIpc)
  ) {
    throw Error('Private IPC module was not redacted');
  }
  document.title = 'SECURITY_READY';
} catch (error) {
  document.title = 'FAIL:' + error.message;
}
