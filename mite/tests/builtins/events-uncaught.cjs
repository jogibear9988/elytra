const E = require('events');
new E().emit('error', new Error('emitter fixture'));
