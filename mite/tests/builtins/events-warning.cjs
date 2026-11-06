const E = require('events');
const emitter = new E();
emitter.setMaxListeners(1);

emitter.on('work', () => {});

emitter.on('work', () => {});

console.log(emitter.listenerCount('work'));
