import fs from 'node:fs';
const h = await fs.promises.open('fixture.txt');
let events = 0;

h.on('close', () => {
  ++events;
  console.log('close pending', h.fd >= 0);
});

const b = Buffer.alloc(4);
const c = Buffer.alloc(3);
const read = h.read(b, 0, 4, 0);
const close = h.close();
console.log('still referenced', h.fd >= 0, close === h.close());
const extra = h.read(c, 0, 3, 1);
console.log((await read).bytesRead, b.toString());
console.log((await extra).bytesRead, c.toString());
await close;
console.log('closed', events, h.fd);
const fd = fs.openSync('new-descriptor', 'w');
await h.close();
fs.writeSync(fd, 'valid');
fs.closeSync(fd);
const idle = await fs.promises.open('fixture.txt');

idle.on('close', () => console.log('idle close', idle.fd));

const first = idle.close();
console.log(first === idle.close());
await first;
