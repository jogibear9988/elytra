/** Page code has only the fixed actions published by its preload. */
if (
  typeof require !== 'undefined' ||
  typeof process !== 'undefined' ||
  typeof ipcRenderer !== 'undefined'
) {
  throw Error('Page privilege');
}

desktop.run().then(
  () => (document.title = 'IPC_READY'),
  (error) => {
    if (!document.title.startsWith('IPC_ERROR:SOURCE:')) {
      document.title = 'IPC_ERROR:' + error.message;
    }
  }
);
