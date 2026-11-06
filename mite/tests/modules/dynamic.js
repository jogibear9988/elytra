console.log('before');

import('./dep.mjs').then((module) => console.log(module.default.label));

console.log('after');
