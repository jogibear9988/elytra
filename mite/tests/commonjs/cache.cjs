const first = require('./dep');
const second = require('./dep.js');
const key = require.resolve('./dep');
console.log(first === second, first.instance, first.parentKnowsChild);
console.log(key.endsWith('dep.js'), require.cache[key].loaded);
console.log(module.children.filter((child) => child.filename === key).length);
delete require.cache[key];
console.log(require('./dep').instance);
