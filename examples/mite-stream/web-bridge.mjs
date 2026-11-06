/** Use standard Web endpoints with a Node Transform, without collecting a body. */
import { Readable, Writable, Transform } from 'node:stream';
import { pipeline } from 'node:stream/promises';

const encoder = new TextEncoder();
const source = ReadableStream.from(['Mite web streams', 'run on LibJS']).pipeThrough(
  new TransformStream({
    transform(line, controller) {
      controller.enqueue(encoder.encode(line));
    }
  })
);
const uppercase = new Transform({
  transform(chunk, encoding, callback) {
    callback(null, Buffer.from(chunk.toString().toUpperCase()));
  }
});
const destination = new WritableStream({
  write(chunk) {
    console.log(new TextDecoder().decode(chunk));
  }
});

await pipeline(Readable.fromWeb(source), uppercase, Writable.fromWeb(destination));
