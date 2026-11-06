console.log('before');

queueMicrotask(() => console.log('microtask'));

require('./order-dep.cjs');
console.log('after');
