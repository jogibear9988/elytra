const a = require('./cycle-a.cjs');
exports.a = a.value;
exports.value = 'B';
