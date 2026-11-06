const E = require('events');
const emitter = new E();
let calls = 0;

function listener() {
  ++calls;
  console.log(this === emitter);
}

emitter.once('event', listener);
const raw = emitter.rawListeners('event')[0];
console.log(raw.listener === listener, emitter.listeners('event')[0] === listener);
emitter.listeners('event').length = 0;
raw();
raw();
console.log(calls, emitter.listenerCount('event'));
