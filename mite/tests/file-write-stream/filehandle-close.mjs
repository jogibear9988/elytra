/** Public FileHandle.close() waits for writes accepted by its stream before releasing the OS fd. */
import { open } from 'node:fs/promises';
import fs from 'node:fs';
import { check, event } from './support.mjs';

const handle = await open('handle-close.bin', 'w+');
const stream = handle.createWriteStream();
const events = [];

stream.on('error', (error) => events.push('error:' + error.code));

stream.on('close', () => events.push('stream-close'));

handle.on('close', () => events.push('handle-close'));

const closed = event(stream, 'close');

stream.write('pending', (error) => events.push('write-callback:' + (error?.code ?? 'ok')));

await handle.close();
await closed;
check(handle.fd === -1 && stream.fd === null, 'Both owners await native fd retirement');
check(
  fs.readFileSync('handle-close.bin', 'utf8') === 'pending',
  'Already accepted write survives handle close'
);
console.log('handle-close', JSON.stringify(events), handle.fd, stream.fd, stream.bytesWritten);
