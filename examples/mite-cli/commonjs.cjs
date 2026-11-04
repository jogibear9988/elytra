const settings = require('./greeting.json');
console.log(settings.greeting);
console.log(require('./greeting.json') === settings);
console.log(require.main === module);
