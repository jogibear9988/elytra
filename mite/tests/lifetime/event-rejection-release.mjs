/** Pending captured listener results retain their receiver/arguments only until delivery. */
import E, { captureRejectionSymbol } from 'node:events';

const collect = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
const pending = [];
const refs = [];
let delivered = 0;

function start() {
  const emitter = new E({ captureRejections: true });
  const payload = {};
  const promise = new Promise((resolve, reject) => pending.push(reject));

  emitter[captureRejectionSymbol] = function (error, name, argument) {
    if (this !== emitter || error !== 'rejected' || name !== 'work' || argument !== payload) {
      throw new Error('captured receiver or arguments were lost before delivery');
    }
    delivered++;
  };

  emitter.on('work', () => promise);

  emitter.emit('work', payload);
  refs.push(new WeakRef(emitter), new WeakRef(payload));
}

for (let index = 0; index < 32; index++) {
  start();
}

setImmediate(() => {
  collect();
  console.log(
    'capture pending',
    refs.every((ref) => ref.deref() !== undefined),
    delivered
  );
  for (const reject of pending.splice(0)) {
    reject('rejected');
  }

  setImmediate(() => {
    collect();
    console.log(
      'capture released',
      refs.every((ref) => ref.deref() === undefined),
      delivered
    );
  });
});
