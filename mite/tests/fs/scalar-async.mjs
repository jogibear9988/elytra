import fs from 'node:fs';
const fd = await new Promise((resolve, reject) =>
  fs.open('scalar-async.bin', 'w+', 0o600, (e, fd) => (e ? reject(e) : resolve(fd)))
);
const source = new Uint16Array([0x1234, 0xabcd, 0x5678]);
await new Promise((resolve, reject) =>
  fs.write(fd, source, { offset: 1, length: 3, position: 1 }, (error, count, original) => {
    if (error) {
      reject(error);
      return;
    }
    console.log(count, original === source);
    resolve();
  })
);
const destination = new DataView(new ArrayBuffer(8), 2, 4);
await new Promise((resolve, reject) =>
  fs.read(fd, destination, { offset: 1, length: 3, position: 1 }, (error, count, original) => {
    if (error) {
      reject(error);
      return;
    }
    console.log(count, original === destination, Buffer.from(destination.buffer).toString('hex'));
    resolve();
  })
);
await new Promise((resolve, reject) =>
  fs.read(fd, { length: 2, position: 0 }, (e, count, buffer) => {
    if (e) {
      reject(e);
      return;
    }
    console.log(count, buffer.length, Buffer.isBuffer(buffer));
    resolve();
  })
);
for (const method of ['fstat', 'fsync', 'fdatasync', 'ftruncate']) {
  await new Promise((resolve, reject) => {
    fs[method](fd, (e, result) => {
      if (e) {
        reject(e);
        return;
      }
      console.log(method, result?.size ?? String(result));
      resolve();
    });
  });
}
await new Promise((resolve, reject) => fs.close(fd, (e) => (e ? reject(e) : resolve())));
console.log('closed');
