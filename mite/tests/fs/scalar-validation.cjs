const fs = require('node:fs');
const fd = fs.openSync('validation.bin', 'w+');
fs.writeFileSync(fd, 'abcdef');

function error(label, fn) {
  try {
    console.log(label, String(fn()));
  } catch (e) {
    console.log(label, e.name, e.code);
  }
}

for (const value of [-1, 1.5, null, '3', NaN]) {
  error('fd', () => fs.fstatSync(value));

  error('length', () => fs.ftruncateSync(fd, value));
}
for (const value of [null, {}, [], Buffer.alloc(0), new DataView(new ArrayBuffer(3))]) {
  error('buffer', () => fs.readSync(fd, value));
}
for (const value of [undefined, null, -1, -2, 0, 1.5, NaN, Infinity, 0n, -1n, '1']) {
  error('position', () => fs.readSync(fd, Buffer.alloc(1), 0, 1, value));
}
for (const value of [undefined, null, -1, -2, 0, 1.5, NaN, Infinity, 0n, 3n, '1']) {
  error('write position', () => fs.writeSync(fd, Buffer.from('x'), 0, 1, value));
}
for (const value of [-1, 1.5, null, '1', 4]) {
  error('offset', () => fs.readSync(fd, Buffer.alloc(3), value, 1, 0));
}
for (const value of [-1, 1.5, null, '1', 4]) {
  error('count', () => fs.writeSync(fd, Buffer.alloc(3), 0, value, 0));
}

error('missing buffer', () => fs.readSync(fd));

error('write invalid encoding', () => fs.writeSync(fd, 'abc', 0, 'invalid'));

fs.closeSync(fd);

error('closed read zero', () => fs.readSync(fd, Buffer.alloc(0)));

error('closed write zero', () => fs.writeSync(fd, Buffer.alloc(0)));

error('closed stat', () => fs.fstatSync(fd));

console.log('final bytes', fs.readFileSync('validation.bin').toString('hex'));
