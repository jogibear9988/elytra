const result = document.querySelector('#result');
const ready = () => {
  document.title = 'EXAMPLE_READY';
};
const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};
document.querySelector('#add').onclick = () =>
  desktop
    .add()
    .then(() => {
      result.textContent = 'Added';
    })
    .catch(fail);
