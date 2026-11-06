window.addEventListener('message', (event) => {
  if (event.source === parent && event.data === 'ELYTRA_EMBED_PING') {
    parent.postMessage('ELYTRA_EMBED_READY', '*');
  }
});
