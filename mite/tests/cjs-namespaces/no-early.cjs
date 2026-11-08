/** Linking a missing named import must fail before this native side effect executes. */
require('node:fs').writeFileSync('early-executed.bin', 'incorrect early execution');
exports.present = 1;
