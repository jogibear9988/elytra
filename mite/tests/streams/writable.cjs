const { Writable } = require('node:stream');
const events = [];
const target = new Writable({
  highWaterMark: 3,

  write(chunk, encoding, callback) {
    events.push('write:' + chunk.toString() + ':' + encoding);
    setTimeout(callback, 1);
  },

  final(callback) {
    events.push('final');
    callback();
  }
});

target.on('drain', () => {
  events.push('drain');

  target.end('z', () => events.push('end callback'));
});

target.on('finish', () => events.push('finish'));

target.on('close', () => {
  events.push('close');
  console.log(events.join(','));
  console.log(target.destroyed, target.closed, target.writableFinished);
});

console.log(
  target.write('aa', () => events.push('first callback')),
  target.write(Buffer.from('bb'), () => events.push('second callback')),
  target.writableLength,
  target.writableNeedDrain
);
