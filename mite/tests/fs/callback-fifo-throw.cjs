const fs = require('node:fs');

fs.readFile('fixture.txt', () => {
  fs.readFile('fifo', () => console.log('unexpected late callback'));

  throw new Error('callback fifo fixture');
});
