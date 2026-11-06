/** Record constructor failure ownership and terminal listener cleanup explicitly. */
import { Readable, Writable } from 'node:stream';
import { ReadableStream } from 'node:stream/web';

const source = new ReadableStream();
try {
  Readable.fromWeb(source, { highWaterMark: -1 });
} catch (error) {
  console.log('invalid hwm', error.code, source.locked);
}
const node = new Readable({ read() {} });
const closed = new Promise((resolve) => node.once('close', resolve));
await Readable.toWeb(node).cancel();
await closed;
console.log('remaining data listeners', node.listenerCount('data'));
const sink = new Writable({
  write(chunk, encoding, callback) {
    callback();
  }
});
await Writable.toWeb(sink).getWriter().close();
console.log('remaining drain listeners', sink.listenerCount('drain'));
console.log('DONE adapter policies');
