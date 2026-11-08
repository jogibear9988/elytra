/** Promise FileHandles and streams share real descriptor ownership and native close completion. */
import fs from 'node:fs';
import { open } from 'node:fs/promises';
import { check, observe, event } from './support.mjs';

for (const autoClose of [true, false]) {
  const filename = 'filehandle-' + autoClose + '.bin';
  const handle = await open(filename, 'w+');
  const stream = handle.createWriteStream({ autoClose, start: 1 });
  const events = observe(stream);
  check(
    stream.fd === handle.fd && stream.path === undefined && !stream.pending,
    'FileHandle stream borrows the same native fd'
  );
  const finished = event(stream, 'finish');
  const closed = event(stream, 'close');
  stream.end('ä');
  await finished;
  if (!autoClose) {
    check(
      handle.fd >= 0 && (await handle.stat()).size === 3,
      'FileHandle remains usable until explicit destroy'
    );
    stream.destroy();
  }
  await closed;
  check(handle.fd === -1 && stream.fd === null, 'Native handle and stream retire together');
  await handle.close();
  console.log(
    'filehandle',
    autoClose,
    JSON.stringify(events),
    stream.bytesWritten,
    handle.fd,
    fs.readFileSync(filename).toString('hex')
  );
}
