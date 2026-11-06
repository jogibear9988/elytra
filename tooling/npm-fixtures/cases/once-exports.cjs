const once = require('once');
console.log(Object.keys(once).sort().join(','));
console.log(typeof once.proto);
try {
  once.proto();
} catch (error) {
  console.log(error.name);
}
