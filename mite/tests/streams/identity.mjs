import stream, {
  Stream,
  Readable,
  Writable,
  Duplex,
  Transform,
  PassThrough,
  promises,
  getDefaultHighWaterMark
} from 'node:stream';
import p, { pipeline } from 'node:stream/promises';
import { EventEmitter } from 'node:events';
console.log(stream === Stream, stream.Stream === stream, promises === p, pipeline === p.pipeline);
console.log(
  new Readable({ read() {} }) instanceof Stream,
  new Writable({
    write(c, e, cb) {
      cb();
    }
  }) instanceof EventEmitter
);
console.log(
  new Duplex({
    read() {},

    write(c, e, cb) {
      cb();
    }
  }) instanceof Readable,
  new Transform() instanceof Duplex,
  new PassThrough() instanceof Transform
);
console.log(
  getDefaultHighWaterMark(false) === (process.platform === 'win32' ? 16384 : 65536),
  getDefaultHighWaterMark(true)
);
