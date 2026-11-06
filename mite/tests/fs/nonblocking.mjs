import fs from 'node:fs';
const events = [];
await new Promise((resolve, reject) => {
  fs.readFile('fifo', (error, data) => {
    if (error) {
      reject(error);
      return;
    }
    events.push(data.toString());
    resolve();
  });

  setTimeout(() => {
    events.push('timer');
    fs.writeFileSync('fifo', 'read');
  }, 20);
});
console.log(events.join(','));
