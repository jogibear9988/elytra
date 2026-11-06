/** Only the fixed preload test action is visible to page code. */
try {
  if (typeof rawIpc !== 'undefined' || typeof ipcRenderer !== 'undefined') {
    throw Error('Raw native capability in page');
  }
  await desktop.check();
  document.title = 'SECURITY_READY';
} catch (error) {
  document.title = 'FAIL:' + error.message;
}
