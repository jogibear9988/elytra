const filename = require.resolve('./bad-syntax');
for (let attempt = 0; attempt < 2; ++attempt) {
    try { require('./bad-syntax'); }
    catch (error) { console.log(error.name, require.cache[filename] === undefined); }
}
try { require('../modules/broken.json'); }
catch (error) { console.log(error.name); }
