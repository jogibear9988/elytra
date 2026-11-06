/** Captured rejection without an error handler still follows the fatal error contract. */
import E from 'node:events';
const emitter = new E({ captureRejections: true });

emitter.on('work', async () => {
  throw new Error('unhandled captured rejection');
});

emitter.emit('work');
console.log('emitted');

setImmediate(() => console.log('UNEXPECTED'));
