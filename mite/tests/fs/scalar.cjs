const fs = require('node:fs');
const fd = fs.openSync('scalar.bin', 'w+', 0o600);
console.log(typeof fd, fd >= 0);
console.log(fs.writeSync(fd, 'abcé', 'utf8'));
const bytes = new Uint8Array([10, 20, 30, 40, 50, 60]);
const view = new DataView(bytes.buffer, 1, 4);
console.log(fs.writeSync(fd, view, { offset: 1, length: 2, position: 1 }));
const target = Buffer.alloc(10, 0x2e);
console.log(fs.readSync(fd, target, 2, 5, 0), target.toString('hex'));
console.log(fs.writeSync(fd, Buffer.from('Z')));
console.log(fs.readSync(fd, new Uint8Array(2), 0, 2, null));
console.log(fs.readSync(fd, target, { offset: 0, length: 2, position: 1 }));
console.log(
  fs.fstatSync(fd).size,
  fs.fstatSync(fd).mode & 0o777,
  fs.fstatSync(fd, { bigint: true }).size === 6n
);
console.log(fs.fsyncSync(fd), fs.fdatasyncSync(fd), fs.ftruncateSync(fd, 3));
console.log(fs.readFileSync(fd).length, fs.readFileSync('scalar.bin').toString('hex'));
console.log(fs.closeSync(fd));
try {
  fs.closeSync(fd);
} catch (e) {
  console.log(e.code, e.syscall, e.path);
}
