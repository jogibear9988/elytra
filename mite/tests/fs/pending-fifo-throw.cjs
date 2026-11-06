require('node:fs').readFile('fifo', () => console.log('unexpected late callback'));

throw new Error('pending fifo fixture');
