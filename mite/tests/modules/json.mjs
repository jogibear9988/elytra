import data from './data.json' with { type: 'json' };
console.log(data.name, data.nested[2]);
console.log(Object.getPrototypeOf(data) === Object.prototype, Object.hasOwn(data, '__proto__'));
data.count++;
const again = await import('./data.json', { with: { type: 'json' } });
const separate = await import('./data.json?copy', { with: { type: 'json' } });
console.log(again.default === data, again.default.count, separate.default.count);
