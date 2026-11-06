/** Portable EventTarget listener identity, once/object callbacks and event cancellation. */
const target = new EventTarget();
const events = [];

const callback = (event) => events.push(event.type);

target.addEventListener('sample', callback);
target.addEventListener('sample', callback);
target.addEventListener(
  'sample',
  {
    handleEvent(event) {
      events.push(event.target === target);
    }
  },
  { once: true }
);
target.dispatchEvent(new Event('sample'));
target.removeEventListener('sample', callback);
target.dispatchEvent(new Event('sample'));
const event = new Event('cancel', { cancelable: true });

target.addEventListener('cancel', (event) => event.preventDefault());

console.log(
  JSON.stringify(events),
  target.dispatchEvent(event),
  event.defaultPrevented,
  event.isTrusted
);
