/** Factory policy component only: controlled byte writer, no claim of native IPC acceptance. */
import { EventEmitter } from 'node:events';
import createIpcChannel from '../../modules/process/ipc-channel.mjs';

function check(value, message) {
  if (!value) {
    throw Error(message);
  }
}

function component() {
  const target = new EventEmitter();
  const references = [];
  const callbacks = [];
  const native = {
    ref(value) {
      references.push(value);
    },

    write(_bytes, callback) {
      callbacks.push(callback);
      return 0;
    },

    shutdown() {
      throw Error('Unexpected native shutdown in reference-only policy');
    },

    close() {}
  };
  const channel = createIpcChannel({
    target,
    Buffer,
    childMode: true,
    native,
    tick: process.nextTick,

    systemError: (status) => Error(String(status))
  });
  return { target, channel, references, callbacks };
}

const explicit = component();
explicit.target.channel.unref();
let calls = 0;

explicit.channel.send('owned-write', () => calls++);

check(explicit.references.at(-1) === false, 'Explicit unref survives pending IPC write');
explicit.callbacks.shift()(0);
check(explicit.references.at(-1) === false, 'Explicit unref survives completed IPC write');
await new Promise((resolve) => process.nextTick(resolve));
check(calls === 1, 'Controlled native completion schedules callback once');
explicit.target.channel.ref();

explicit.channel.send('owned-write', () => calls++);

check(explicit.references.at(-1) === true, 'Explicit ref survives pending write');
explicit.callbacks.shift()(0);
check(explicit.references.at(-1) === true, 'Explicit ref survives completed write');
const automatic = component();
check(automatic.references.at(-1) === false, 'Child IPC starts automatically unreferenced');

const listener = () => {};

automatic.target.on('message', listener);
check(automatic.references.at(-1) === true, 'Message listener retains automatic channel');
automatic.target.removeListener('message', listener);
check(automatic.references.at(-1) === false, 'Last message listener releases automatic channel');
automatic.channel.send('owned-write');
check(automatic.references.at(-1) === true, 'Default pending write temporarily retains channel');
automatic.callbacks.shift()(0);
check(automatic.references.at(-1) === false, 'Default completed write releases channel');
console.log('PASS controlled IPC factory automatic/explicit reference policy');
