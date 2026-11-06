/** Composition marks all dependents before public listeners and resists propagation stops. */
const source = new AbortController();
const other = new AbortController();
const combined = AbortSignal.any([source.signal, other.signal, source.signal]);
const nested = AbortSignal.any(new Set([combined]));
const log = [];

source.signal.addEventListener('abort', (event) => {
  log.push(['source', combined.aborted, nested.aborted]);
  event.stopImmediatePropagation();
});

combined.onabort = () => log.push('combined');

nested.onabort = () => log.push('nested');

const reason = { value: 42 };
source.abort(reason);
other.abort('too late');
console.log(JSON.stringify(log), combined.reason === reason, nested.reason === reason);
const first = AbortSignal.abort('first');
console.log(
  AbortSignal.any([first, AbortSignal.abort('second')]).reason,
  AbortSignal.any([]).aborted
);
const synthetic = new AbortController();
const dependent = AbortSignal.any([synthetic.signal]);
synthetic.signal.dispatchEvent(new Event('abort'));
console.log(dependent.aborted);
