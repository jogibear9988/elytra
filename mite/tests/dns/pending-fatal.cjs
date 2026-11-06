require('node:dns').lookup('localhost', () => console.log('unexpected callback'));

throw Error('fatal with pending resolver');
