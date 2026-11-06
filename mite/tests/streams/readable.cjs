const { Readable } = require('node:stream');
const manual = new Readable({ read() {} });
manual.push(Buffer.from('ab'));
manual.push(Buffer.from('cd'));
manual.push(null);
console.log(
  manual.readableLength,
  manual.read(3).toString(),
  manual.read().toString(),
  manual.read()
);
const source = new Readable({ read() {} });
source.setEncoding('utf8');
const output = [];

source.on('data', (chunk) => output.push(chunk));

source.on('end', () => console.log(output.join('|'), source.readableEnded));

source.push(Buffer.from([0xc3]));
source.push(Buffer.from([0xa9, 0x21]));
source.push(null);
