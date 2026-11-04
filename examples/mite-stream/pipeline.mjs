import { Readable, Transform, Writable } from 'node:stream';
import { pipeline } from 'node:stream/promises';

const uppercase = new Transform({
  transform(chunk, encoding, callback) {
    callback(null, chunk.toString().toUpperCase());
  }
});
const lines = new Writable({
  write(chunk, encoding, callback) {
    console.log(chunk.toString().trimEnd());
    callback();
  }
});

await pipeline(Readable.from(['Mite streams\n', 'run on LibJS\n']), uppercase, lines);
