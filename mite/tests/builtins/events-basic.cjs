const E = require('events');
console.log(E === require('node:events'), E.EventEmitter === E);
const emitter = new E();
console.log(emitter.emit('missing'));

emitter.on('sum', function (a, b) {
  console.log(this === emitter, a + b);
});

console.log(emitter.emit('sum', 2, 3), emitter.listenerCount('sum'));
