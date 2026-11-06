const once = require('once');
const E = require('node:events');
let calls = 0;

function work(value) {
  ++calls;
  return value * 2;
}

work.annotation = 'preserved';
const wrapped = once(work);
const emitter = new E();
emitter.on('work', wrapped);
emitter.emit('work', 21);
emitter.emit('work', 100);
console.log(calls, wrapped.called, wrapped.value, wrapped.annotation);
const strict = once.strict(function action() {
  return 5;
});
console.log(strict());
try {
  strict();
} catch (error) {
  console.log(error.name, error.message.includes("shouldn't be called"));
}
