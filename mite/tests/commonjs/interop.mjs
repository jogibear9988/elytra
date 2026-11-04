import './order-prior.mjs';
import value, * as namespace from './order-dep.cjs';
console.log('main', value.value, namespace.default === value, namespace['module.exports'] === value);
console.log(namespace === await import('./order-dep.cjs'));
