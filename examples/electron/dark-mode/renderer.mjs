const result = document.querySelector('#result');
const ready = () => {
  document.title = 'EXAMPLE_READY';
};
const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};
for (const action of ['toggle', 'system'])
  document.getElementById(action).onclick = () =>
    desktop[action]()
      .then((dark) => {
        result.textContent = dark ? 'Dark' : 'Light';
      })
      .catch(fail);
