const result = document.querySelector('#result');
const ready = () => {
  document.title = 'EXAMPLE_READY';
};
const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};
const update = () => {
  result.textContent = navigator.onLine ? 'Browser reports online' : 'Browser reports offline';
};
if (typeof navigator.onLine !== 'boolean') throw Error('Network hint unavailable');
window.addEventListener('online', update);
window.addEventListener('offline', update);
update();
ready();
