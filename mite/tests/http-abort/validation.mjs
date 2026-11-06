/** Reject non-signal objects before native I/O; preserve Node's ignored falsy options. */
import { request, get } from 'node:http';
let rejected = 0;
for (const signal of [{}, true, 1, 'signal', Symbol('signal')]) {
  for (const create of [request, get]) {
    try {
      create({ host: '127.0.0.1', port: 9, agent: false, signal });
      throw Error('Invalid signal accepted');
    } catch (error) {
      if (error.code !== 'ERR_INVALID_ARG_TYPE') {
        throw error;
      }
      rejected++;
    }
  }
}
for (const signal of [undefined, null, false, 0, '']) {
  const pending = request({ host: '127.0.0.1', port: 9, agent: false, signal });

  pending.on('error', () => {});

  pending.destroy();
}
console.log('validation', rejected);
