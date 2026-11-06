/** Reject invalid signal shapes without opening native transports. */
import { Socket, createConnection, createServer } from 'node:net';
const codes = [];
for (const signal of [{}, true, 1, 'signal', Symbol('signal')]) {
  for (const operation of [
    () => new Socket({ signal }),
    () => createConnection({ port: 9, host: '127.0.0.1', signal }),
    () => createServer().listen({ port: 0, host: '127.0.0.1', signal })
  ]) {
    try {
      operation();
      throw Error('Invalid signal was accepted');
    } catch (error) {
      if (error.code !== 'ERR_INVALID_ARG_TYPE') {
        throw error;
      }
      codes.push(error.code);
    }
  }
}
for (const signal of [undefined, null, false, 0, '']) {
  new Socket({ signal }).destroy();
}
console.log('validation', codes.length);
