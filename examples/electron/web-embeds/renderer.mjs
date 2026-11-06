const result = document.querySelector('#result');
const ready = () => {
  document.title = 'EXAMPLE_READY';
};
const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};
const frame = document.querySelector('iframe');
window.addEventListener('message', (event) => {
  if (event.source === frame.contentWindow && event.data === 'ELYTRA_EMBED_READY') {
    result.textContent = 'Isolated local iframe responded';
    ready();
  }
});
const ping = () => frame.contentWindow.postMessage('ELYTRA_EMBED_PING', '*');
frame.addEventListener('load', ping);
ping();
