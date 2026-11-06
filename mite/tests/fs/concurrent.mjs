import fs from 'node:fs';
const bytes = Buffer.alloc(1024 * 1024, 0x5a);
fs.writeFileSync('large.bin', bytes);
const results = await Promise.all(
  Array.from({ length: 16 }, () => fs.promises.readFile('large.bin'))
);
console.log(
  results.length,
  results.every((b) => b.length === bytes.length && b.equals(bytes))
);
let completed = 0;
let checkpoint = false;
await new Promise((resolve, reject) => {
  for (let i = 0; i < 16; ++i) {
    fs.readFile('fixture.txt', (error, data) => {
      if (error) {
        reject(error);
        return;
      }
      if (completed && !checkpoint) {
        reject(new Error('missing callback microtask checkpoint'));
        return;
      }
      checkpoint = false;
      ++completed;

      queueMicrotask(() => (checkpoint = true));

      if (completed === 16) {
        resolve();
      }
    });
  }
});
console.log(completed, checkpoint);
