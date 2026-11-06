require('node:fs').readFile('fixture.txt', () => console.log('unexpected late callback'));

throw new Error('pending fs fixture');
