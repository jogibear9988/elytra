const E = require('events');
const first = new E();
const second = new E();
console.log(first.getMaxListeners());
E.defaultMaxListeners = 20;
console.log(first.getMaxListeners());
console.log(
  E.setMaxListeners(3, first, second),
  E.getMaxListeners(first),
  second.getMaxListeners()
);
for (const value of [undefined, null, 'x', -1, NaN]) {
  try {
    first.setMaxListeners(value);
  } catch (error) {
    console.log(error.name, error.code);
  }
}
console.log(first.setMaxListeners(Infinity) === first, first.getMaxListeners());
for (const method of ['on', 'once', 'prependListener', 'prependOnceListener', 'off']) {
  try {
    first[method]('event', 42);
  } catch (error) {
    console.log(error.name, error.code);
  }
}
