/** Within an I/O completion, the check phase precedes the newly scheduled timer. */
import fs from 'node:fs';
const order = [];

fs.readFile(new URL(import.meta.url), () => {
  setTimeout(() => {
    order.push('timer');
    console.log(order.join(','));
  }, 1);

  setImmediate(() => order.push('immediate'));
});
