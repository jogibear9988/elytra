console.log('start');

queueMicrotask(() => {
  console.log('microtask');

  Promise.resolve().then(() => console.log('nested'));
});

Promise.resolve().then(() => console.log('promise'));

queueMicrotask(() => console.log('last'));

console.log('end');
