const result = document.querySelector('#result');
const ready = () => {
  document.title = 'EXAMPLE_READY';
};
const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};
document.querySelector('#add').onclick = () => {
  result.textContent = desktop.add(20, 22);
};
if (
  desktop.sample !== 'quick-start' ||
  desktop.add(20, 22) !== 42 ||
  !Object.isFrozen(desktop) ||
  typeof process !== 'undefined' ||
  typeof require !== 'undefined'
)
  throw Error('Preload boundary failed');
result.textContent = 'Isolated preload ready';
ready();
