/** Abort state is synchronous, idempotent, branded and preserves arbitrary reason identity. */
const controller = new AbortController();
const signal = controller.signal;
const marker = { cause: 42 };
const events = [];

signal.onabort = function (event) {
  events.push([
    this === signal,
    event.target === signal,
    event.type,
    signal.aborted,
    signal.reason === marker
  ]);
};

signal.addEventListener('abort', () => events.push('once'), { once: true });

console.log(
  signal === controller.signal,
  signal.aborted,
  signal.reason,
  signal instanceof EventTarget
);
controller.abort(marker);
controller.abort('ignored');
let same;
try {
  signal.throwIfAborted();
} catch (error) {
  same = error === marker;
}
console.log(JSON.stringify(events), same, signal.reason === marker);
const unstarted = new AbortController().signal;
console.log(unstarted.throwIfAborted(), Object.prototype.toString.call(unstarted));
const manual = new AbortController();
manual.signal.dispatchEvent(new Event('abort'));
console.log(manual.signal.aborted, manual.signal.reason);
// Replacing the public dispatch method cannot prevent internal abort notification.
const replaced = new AbortController();

replaced.signal.dispatchEvent = () => {
  throw Error('public override');
};

let overridden = false;
try {
  replaced.abort();
} catch (error) {
  overridden = error.message === 'public override';
}
console.log(overridden);
console.log(
  replaced.signal.reason.name,
  replaced.signal.reason.code,
  replaced.signal.reason instanceof DOMException
);
