/**
 * Spellchecker browser interaction. Native setup stays in main; this script uses
 * document/browser APIs only.
 */
const result = document.querySelector('#result');

const ready = () => {
  document.title = 'EXAMPLE_READY';
};

const fail = (error) => {
  result.textContent = error.message;
  document.title = 'EXAMPLE_ERROR: ' + error.message;
};

ready();
