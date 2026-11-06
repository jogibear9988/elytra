/** Actual Node/Mite listener rejection ownership, ordering and thenable validation. */
import E, { captureRejectionSymbol, errorMonitor } from 'node:events';
import { setImmediate as checkpoint } from 'node:timers/promises';

function check(value, label) {
  if (!value) {
    throw new Error(label);
  }
}

const initialDefault = E.captureRejections;
const trace = [];
const emitter = new E({ captureRejections: true });
const failure = new Error('listener rejected');

emitter.on(errorMonitor, (error) => {
  check(error === failure, 'monitor error identity');
  trace.push('monitor');
});

emitter.on('error', (error) => {
  check(error === failure, 'error identity');
  trace.push('handled');
});

emitter.on('work', async function (value) {
  check(this === emitter && value === 42, 'listener receiver/arguments');
  trace.push('listener');
  throw failure;
});

check(emitter.emit('work', 42), 'emit return value');
trace.push('after');

queueMicrotask(() => trace.push('microtask'));

process.nextTick(() => trace.push('tick'));

await checkpoint();
console.log(JSON.stringify(trace));

const custom = new E({ captureRejections: true });
const name = Symbol('work');
const payload = {};
let hookCalls = 0;

custom[captureRejectionSymbol] = function (error, event, first, second) {
  check(
    this === custom && error === failure && event === name && first === payload && second === 7,
    'custom hook receiver/reason/name/arguments'
  );
  hookCalls++;
};

custom.once(name, async () => {
  throw failure;
});

custom.emit(name, payload, 7);
await checkpoint();
check(hookCalls === 1 && custom.listenerCount(name) === 0, 'once rejection hook lifetime');

let inspected = 0;
let handled = 0;
const thenableEmitter = new E({ captureRejections: true });

thenableEmitter.on('error', (error) => {
  check(error === failure, 'thenable failure identity');
  handled++;
});

const thenable = {
  get then() {
    inspected++;
    check(inspected === 1, 'then getter read twice');
    return function (resolve, reject) {
      check(this === thenable && resolve === undefined, 'then receiver/resolve');
      reject(failure);
    };
  }
};

thenableEmitter.on('work', () => thenable);

thenableEmitter.emit('work');
check(handled === 0, 'thenable rejection delivered synchronously');
await checkpoint();
check(inspected === 1 && handled === 1, 'thenable not observed');
for (const result of [undefined, null, 0, false, 'text', Symbol('value'), 42n, {}, { then: 42 }]) {
  const target = new E({ captureRejections: true });

  target.on('work', () => result);

  check(target.emit('work'), 'ordinary listener result rejected');
}

for (const mode of ['getter', 'call']) {
  const target = new E({ captureRejections: true });
  const sync = [];

  target.on('error', (error) => {
    check(error === failure, 'hostile thenable identity');
    sync.push('error');
  });

  target.on('work', () =>
    mode === 'getter'
      ? {
          get then() {
            throw failure;
          }
        }
      : {
          then() {
            throw failure;
          }
        }
  );

  target.on('work', () => sync.push('second'));

  target.emit('work');
  check(sync.join() === 'error,second', 'hostile thenable synchronous error order');
}

let forbidden = 0;
const disabled = new E();

disabled.on('work', () => ({
  get then() {
    forbidden++;
    throw failure;
  }
}));

disabled.emit('work');
check(forbidden === 0, 'capture inspected a disabled listener');

let defaults = 0;
E.captureRejections = true;
for (const option of [undefined, false, true, null, 0, '']) {
  const target = new E({ captureRejections: option });

  target.on('error', () => defaults++);

  target.on('work', () => ({
    then(resolve, reject) {
      reject(failure);
    }
  }));

  target.emit('work');
}
E.captureRejections = false;
await checkpoint();
check(
  defaults === 6,
  'default capture must be saved per instance, including falsey constructor options'
);
for (const option of [1, 'yes', {}, [], () => {}]) {
  try {
    new E({ captureRejections: option });
    throw new Error('invalid capture constructor option accepted');
  } catch (error) {
    check(error.code === 'ERR_INVALID_ARG_TYPE', 'constructor validation');
  }
  try {
    E.captureRejections = option;
    throw new Error('invalid global capture setting accepted');
  } catch (error) {
    check(error.code === 'ERR_INVALID_ARG_TYPE', 'default validation');
  }
}
E.captureRejections = initialDefault;
let repeated = 0;
const restoring = new E({ captureRejections: true });

restoring.on('error', () => {
  repeated++;
  return {
    get then() {
      throw new Error('error delivery recursively inspected a result');
    }
  };
});

restoring.on('work', async () => {
  throw failure;
});

restoring.emit('work');
await checkpoint();
restoring.emit('work');
await checkpoint();
check(repeated === 2, 'capture state was not restored after error delivery');

const monitored = new E({ captureRejections: true });
const secondary = new Error('monitor rejected');
const observed = [];

monitored.on(errorMonitor, async (error) => {
  if (error === failure) {
    throw secondary;
  }
});

monitored.on('error', (error) => observed.push(error));

monitored.emit('error', failure);
await checkpoint();
check(
  observed.length === 2 && observed[0] === failure && observed[1] === secondary,
  'async monitor rejection was not captured'
);
// Each thenable rejection owns a separate delivery record; clearing one must
// preserve later calls of the same retained rejection callback and arguments.
const multiple = new E({ captureRejections: true });
const received = [];
let later;

multiple[captureRejectionSymbol] = (reason, event, argument) => {
  check(event === 'work' && argument === payload, 'later thenable arguments were cleared');
  received.push(reason);
};

multiple.on('work', () => ({
  then(resolve, reject) {
    later = reject;
    reject(1);
    reject(2);
  }
}));

multiple.emit('work', payload);
await checkpoint();
later(3);
await checkpoint();
check(received.join() === '1,2,3', 'independent thenable rejection deliveries');
later = undefined;
console.log('PASS capture hook, thenables, sync errors, disabled mode, defaults and validation');
