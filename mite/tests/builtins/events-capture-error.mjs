/** Error-handler rejections remain unhandled instead of recursively re-entering capture. */
import E from 'node:events';
const emitter = new E({ captureRejections: true });

emitter.on('error', async () => {
  console.log('error once');
  throw new Error('capture error-handler failure');
});

emitter.on('work', async () => {
  throw new Error('original');
});

emitter.emit('work');

setImmediate(() => console.log('UNEXPECTED'));
