/** once must honor overridden subscription hooks used by stream subclasses. */
import { EventEmitter } from 'node:events';
const calls = [];
class Observed extends EventEmitter {
  on(type, listener) {
    calls.push('on:' + type);
    return super.on(type, listener);
  }

  prependListener(type, listener) {
    calls.push('prepend:' + type);
    return super.prependListener(type, listener);
  }
}
const emitter = new Observed();

const listener = () => calls.push('listener');

console.log(
  'identity',
  emitter.once('value', listener) === emitter,
  emitter.listeners('value')[0] === listener
);

emitter.prependOnceListener('value', () => calls.push('first'));

emitter.emit('value');
emitter.emit('value');
console.log('hooks', calls.join(','), emitter.listenerCount('value'));
