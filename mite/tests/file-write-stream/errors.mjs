/** Native open/write failures, early destruction and abort must finish without descriptor leaks. */
import fs from 'node:fs';
import { check, observe, event, rejected } from './support.mjs';

for (const mode of ['early', 'abort']) {
  const controller = new AbortController();
  if (mode === 'abort') {
    controller.abort('fixture abort');
  }
  const filename = mode + '.bin';
  const stream = fs.createWriteStream(filename, { signal: controller.signal });
  const events = observe(stream);
  const closed = event(stream, 'close');
  if (mode === 'early') {
    stream.destroy();
  }
  await closed;
  console.log(
    'cancel',
    mode,
    JSON.stringify(events),
    fs.existsSync(filename),
    stream.fd,
    stream.pending,
    stream.bytesWritten
  );
}
// Node can finish a pre-aborted constructor's already-submitted open before or after close.
// Keep that actual close-time existsSync observation visible. The runner requires Mite
// to report false and permits only Node's boolean timing difference on this one line.
// Let Node's real file side effect settle before comparing the final native snapshots.
await new Promise((resolve) => setTimeout(resolve, 20));
fs.writeFileSync('existing.bin', 'preserved');
for (const [filename, options, code] of [
  ['missing-parent/output.bin', {}, 'ENOENT'],
  ['existing.bin', { flags: 'wx' }, 'EEXIST']
]) {
  const stream = fs.createWriteStream(filename, options);
  const events = observe(stream);
  let failure;

  stream.on('error', (error) => {
    failure = error;
  });

  const closed = event(stream, 'close');
  stream.end('unwritten');
  await closed;
  check(
    failure?.code === code && stream.bytesWritten === 0 && stream.fd === null,
    'Real native open error'
  );
  console.log('open-error', code, JSON.stringify(events), stream.bytesWritten);
}
check(
  fs.readFileSync('existing.bin', 'utf8') === 'preserved',
  'Exclusive-open failure never truncates'
);
const fd = fs.openSync('existing.bin', 'r');
const stream = fs.createWriteStream(null, { fd });
const events = observe(stream);
const closed = event(stream, 'close');
let failure;

stream.on('error', (error) => {
  failure = error;
});

stream.end('not writable');
await closed;
check(
  failure?.code === 'EBADF' && rejected(() => fs.fstatSync(fd)) === 'EBADF',
  'Write failure closes owned read-only fd'
);
console.log('write-error', failure.code, JSON.stringify(events), stream.bytesWritten);
