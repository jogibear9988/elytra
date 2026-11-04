import data from '../modules/data.json' with { type: 'json' };
import read from './read-json.cjs';
console.log(data === read(), data.count);
