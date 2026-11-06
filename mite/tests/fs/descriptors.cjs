const fs = require('node:fs');
const input = Number(process.argv[2]);
const output = Number(process.argv[3]);
console.log(fs.readFileSync(input, 'utf8'), fs.readFileSync(input).length);
fs.writeFileSync(output, 'one');
fs.appendFileSync(output, 'two');

fs.readFile(input, (error, data) => {
  if (error) {
    throw error;
  }
  console.log('callback', data.length);

  fs.writeFile(output, Buffer.from('three'), (error) => {
    if (error) {
      throw error;
    }
    fs.writeFileSync(output, 'four');
    console.log('borrowed descriptors remain open');
  });
});
