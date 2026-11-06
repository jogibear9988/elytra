const data = require('../modules/data.json');
data.count = 10;

import('../modules/data.json', { with: { type: 'json' } }).then(({ default: again }) => {
  console.log(again === data, again.count);
});
