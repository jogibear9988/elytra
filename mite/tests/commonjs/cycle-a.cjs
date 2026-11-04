exports.value = 'A';
const b = require('./cycle-b.cjs');
exports.result = b.value + b.a;
