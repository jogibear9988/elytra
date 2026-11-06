/** The second window cannot impersonate the native owner. */
desktop
  .claim()
  .then((value) => (document.title = value ? 'IPC_FOREIGN_REJECTED' : 'IPC_ERROR:forged sender'));
