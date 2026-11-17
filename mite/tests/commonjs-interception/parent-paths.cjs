module.paths = globalThis.__ownedLookupPaths.slice();
module.exports = { value: require('owned-alternate'), paths: module.paths };
