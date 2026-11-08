/** Numeric descriptors stay open after finish only when explicitly borrowed. */
import fs from 'node:fs';
import { check, observe, event, rejected } from './support.mjs';

for (const autoClose of [true, false]) {
  const filename = 'descriptor-' + autoClose + '.bin';
  const fd = fs.openSync(filename, 'w+');
  const stream = fs.createWriteStream('ignored-path.bin', { fd, autoClose });
  const events = observe(stream);
  check(
    stream.fd === fd && !stream.pending && stream.path === undefined,
    'Supplied fd identity and ignored pathname'
  );
  const finished = event(stream, 'finish');
  const closed = event(stream, 'close');
  stream.end('abc');
  await finished;
  if (!autoClose) {
    check(
      fs.fstatSync(fd).size === 3 && !stream.destroyed && !stream.closed,
      'Borrowed fd remains usable after finish'
    );
    fs.writeSync(fd, 'Z');
    stream.destroy();
  }
  await closed;
  check(
    rejected(() => fs.fstatSync(fd)) === 'EBADF',
    'Explicit destruction/autoClose release the actual fd'
  );
  console.log(
    'owner',
    autoClose,
    JSON.stringify(events),
    stream.fd,
    stream.pending,
    stream.bytesWritten,
    fs.readFileSync(filename, 'utf8')
  );
}
check(!fs.existsSync('ignored-path.bin'), 'Supplied fd never opens the ignored pathname');
