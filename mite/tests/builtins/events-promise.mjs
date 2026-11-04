import E, { once } from 'node:events';
const emitter = new E();
const value = once(emitter, 'work');
emitter.emit('work', 1, 2);
console.log(JSON.stringify(await value), emitter.listenerCount('work'), emitter.listenerCount('error'));
const failure = once(emitter, 'work');
emitter.emit('error', new Error('failure'));
try { await failure; } catch (error) { console.log(error.message, emitter.listenerCount('work')); }
const explicit = once(emitter, 'error');
emitter.emit('error', 'payload');
console.log(JSON.stringify(await explicit));
