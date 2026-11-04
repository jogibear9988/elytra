const once = require('once');
const wrappy = require('wrappy');
console.log(once === require('once'), typeof wrappy);
console.log(require('once/package.json').version, require('wrappy/package.json').version);
console.log(require.resolve('once').endsWith('once.js'), require.resolve('wrappy').endsWith('wrappy.js'));
