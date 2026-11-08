/** Observe real process exit dispatch. Continuation markers must never run after synchronous exit. */
const mode = process.argv[2];
if (mode === 'natural') {
  process.on('exit', (code) => {
    if (process.exitCode !== undefined) {
      throw Error('Natural default exitCode was synthesized');
    }
    console.log('EXIT:' + code);
  });

  console.log('BODY');
} else if (mode === 'natural-code') {
  process.exitCode = 3;

  process.on('exit', (code) => {
    console.log('EXIT:' + code);
    process.exitCode = 7;
  });

  console.log('BODY');
} else if (mode === 'explicit') {
  process.exitCode = 2;

  process.on('exit', (code) => {
    console.log('EXIT:' + code);

    process.nextTick(() => console.log('FORBIDDEN_NEXT_TICK'));

    setTimeout(() => console.log('FORBIDDEN_TIMER'), 0);

    Promise.resolve().then(() => console.log('FORBIDDEN_MICROTASK'));
  });

  console.log('BODY');
  process.exit(6);
  console.log('FORBIDDEN_CONTINUATION');
} else if (mode === 'explicit-code') {
  process.exitCode = 4;

  process.on('exit', (code) => {
    console.log('EXIT:' + code);
    process.exitCode = 8;
  });

  process.exit();
  console.log('FORBIDDEN_CONTINUATION');
} else if (mode === 'explicit-undefined') {
  process.exitCode = 7;

  process.on('exit', (code) => console.log('EXIT:' + code));

  process.exit(undefined);
  console.log('FORBIDDEN_CONTINUATION');
} else if (mode === 'nested') {
  process.once('exit', (code) => {
    console.log('ONCE:' + code + ':remaining=' + process.listenerCount('exit'));
    process.exit(9);
    console.log('FORBIDDEN_NESTED_CONTINUATION');
  });

  process.on('exit', () => console.log('FORBIDDEN_SECOND_LISTENER'));

  process.exit(5);
} else if (mode === 'throw') {
  process.once('exit', (code) => {
    console.log('ONCE:' + code);
    throw Error('controlled-exit-listener-failure');
  });

  process.on('exit', () => console.log('FORBIDDEN_SECOND_LISTENER'));
} else if (mode === 'natural-async') {
  process.on('exit', (code) => {
    console.log('EXIT:' + code);

    process.nextTick(() => console.log('FORBIDDEN_NEXT_TICK'));

    setTimeout(() => console.log('FORBIDDEN_TIMER'), 0);
  });
} else {
  throw Error('Unknown exit fixture mode');
}
