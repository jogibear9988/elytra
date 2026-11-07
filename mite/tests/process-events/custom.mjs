/** Shared Node/Mite EventEmitter contract on the real process object. */
import processObject from 'node:process';
import { EventEmitter, errorMonitor } from 'node:events';

function check(value) {
  if (!value) {
    throw new Error('Process emitter contract');
  }
}

check(processObject === process && process instanceof EventEmitter);
check(process.on === process.addListener && process.off === process.removeListener);
const type = Symbol('owned event');
const calls = [];

function last(value) {
  check(this === process);
  calls.push(`last:${value}`);
}

check(process.on(type, last) === process);

process.prependOnceListener(type, function (value) {
  check(this === process);
  calls.push(`once:${value}`);
});

check(process.emit(type, 1) && process.emit(type, 2));
check(calls.join(',') === 'once:1,last:1,last:2');
check(process.listenerCount(type) === 1);
process.off(type, last);
check(!process.emit(type) && process.listenerCount(type) === 0);
let monitored = false;
const error = new Error('owned error');

process.once(errorMonitor, (value) => {
  check(value === error);
  monitored = true;
});

process.once('error', (value) => check(value === error && monitored));

process.emit('error', error);
process.on(type, last);
if (typeof collectHostGarbage === 'function') {
  collectHostGarbage();
} else if (typeof gc === 'function') {
  gc();
}
process.emit(type, 3);
check(calls.at(-1) === 'last:3');
process.removeAllListeners(type);
console.log('PASS process custom events identity/order/once/errors/retention');
