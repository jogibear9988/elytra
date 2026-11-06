/**
 * Multithreading browser interaction. Native setup stays in main; this script uses
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

try {
  if (typeof Worker === 'undefined') {
    throw Error('Worker unavailable');
  }
  const worker = new Worker(new URL('./worker.mjs', import.meta.url), { type: 'module' });

  worker.onmessage = (event) => {
    if (event.data !== 5050) {
      return fail(Error('Worker result mismatch'));
    }
    result.textContent = `Worker result: ${event.data}`;
    worker.terminate();
    ready();
  };

  worker.onerror = (event) => fail(Error(event.message));

  worker.postMessage(100);
} catch (error) {
  fail(error);
}
