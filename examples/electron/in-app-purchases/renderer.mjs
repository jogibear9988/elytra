/**
 * In-app purchases browser interaction. Privileged actions use fixed preload methods; the
 * document has no Node host.
 */
const result = document.querySelector('#result');

const ready = () => {
  document.title = 'EXAMPLE_READY';
};

const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};

document.querySelector('#lookup').onclick = () =>
  desktop
    .lookup(document.querySelector('#product').value)
    .then((products) => {
      result.textContent = JSON.stringify(products, null, 2);
    })
    .catch(fail);
