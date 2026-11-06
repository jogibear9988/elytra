/** Listener bookkeeping preserves once, duplicate, capture and signal-option semantics. */
const source = new AbortController();
const guard = new AbortController();
let count = 0;

const callback = () => count++;

source.signal.addEventListener('abort', callback);
source.signal.addEventListener('abort', callback);
source.signal.addEventListener('abort', callback, true);
source.signal.removeEventListener('abort', callback);
source.abort();
const target = new AbortController();

guard.signal.addEventListener('abort', (event) => event.stopImmediatePropagation());

target.signal.addEventListener(
  'abort',
  () => {
    throw Error('Cancelled guard listener fired');
  },
  { signal: guard.signal }
);

guard.abort();
target.abort();
console.log('bookkeeping', count === 1);
