/** Reexport-name discovery must inspect dependencies without running either CommonJS body. */
require('node:fs').writeFileSync('early-executed.bin', 'incorrect reexport execution');
module.exports = require('./no-early.cjs');
