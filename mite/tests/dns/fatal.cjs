const dns = require('node:dns');
for (let i = 0; i < 16; i++) {
  dns.lookup('localhost', () => {
    throw Error('resolver callback failed');
  });
}
