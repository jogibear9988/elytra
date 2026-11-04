'use strict';
// Use the portable implementation, never the entry that imports native Node stream.
module.exports = require('readable-stream/lib/stream.js');
module.exports.setDefaultHighWaterMark(false, host.platform === 'win32' ? 16384 : 65536);
