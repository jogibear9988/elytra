const E = require('events');
const emitter = new E();
const symbol = Symbol('event');
function fn() {}
emitter.on(symbol, fn).on(symbol, fn).once('text', fn);
console.log(emitter.eventNames().map(String).join(','));
console.log(emitter.listenerCount(symbol, fn), E.listenerCount(emitter, symbol));
console.log(E.getEventListeners(emitter, 'text')[0] === fn);
console.log(emitter.off(symbol, fn) === emitter, emitter.listenerCount(symbol));
emitter.removeAllListeners(symbol);
console.log(emitter.eventNames().join(','));
