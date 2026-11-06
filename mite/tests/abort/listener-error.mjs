/** Listener failures are reported asynchronously, after the dispatch finishes. */
const controller = new AbortController();

controller.signal.addEventListener('abort', () => {
  throw Error('abort listener failure fixture');
});

controller.signal.addEventListener('abort', () => console.log('second listener'));

controller.abort();
console.log('dispatch returned');
