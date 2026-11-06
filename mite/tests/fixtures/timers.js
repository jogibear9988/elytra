console.log('start');
const timer = setTimeout(
  function (word, count) {
    console.log(this === timer, word, count);

    queueMicrotask(() => console.log('microtask'));

    Promise.resolve().then(() => console.log('promise'));
  },
  1,
  'argument',
  42
);

setTimeout(() => console.log('second'), 1);

console.log('end');
