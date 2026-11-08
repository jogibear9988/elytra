/** Internal writer integration: shorten actual OS writes without exposing a public custom-fs option. */
import fs from 'node:fs';
import * as stream from 'node:stream';
import createWriteStreams from '../../modules/filesystem/write-stream.mjs';
import { check, event } from './support.mjs';

const validate = {
  path: (value) => value,

  flags: (value, fallback) => value ?? fallback,

  mode: (value, fallback) => value ?? fallback
};
const host = { stream, Buffer };
const nativeWrites = [];
const shortFs = {
  open: fs.open,
  close: fs.close,

  write(fd, bytes, offset, length, position, callback) {
    nativeWrites.push({ offset, length, position });
    // Every successful completion comes from a genuine syscall, never a fabricated count.
    fs.write(fd, bytes, offset, Math.min(length, 7), position, callback);
  }
};
const { createWriteStream } = createWriteStreams(shortFs, host, validate);
const input = Buffer.alloc(70031);
for (let index = 0; index < input.length; ++index) {
  input[index] = index % 251;
}
const expected = Buffer.alloc(80000, 46);
fs.writeFileSync('partial.bin', expected);
input.copy(expected, 3);
const writer = createWriteStream('partial.bin', { flags: 'r+', start: 3 });
const closed = event(writer, 'close');
let failure;

writer.on('error', (error) => {
  failure = error;
});

writer.end(input);
await closed;
check(!failure, failure?.message);
check(writer.bytesWritten === input.length, 'Count only completed native bytes');
check(
  fs.readFileSync('partial.bin').equals(expected),
  'Actual partial syscalls preserve exact offsets and surrounding bytes'
);
check(
  nativeWrites.length > 10000 && nativeWrites[0].length === 65536,
  'Exercise both native request cap and partial completions'
);
for (let index = 0; index < nativeWrites.length; ++index) {
  const request = nativeWrites[index];
  check(request.length > 0 && request.length <= 65536, 'Native requests stay bounded');
  check(
    request.offset === index * 7 && request.position === request.offset + 3,
    'Each native retry advances buffer and file positions'
  );
}
console.log('PASS internal writer actual short syscalls: bytes/offsets/65536-byte requests');

// The following are explicitly simulated failure paths, separate from native parity evidence.
for (const mode of ['zero', 'EAGAIN']) {
  let attempts = 0;
  let ownedFd;
  const failingFs = {
    open(filename, flags, mode, callback) {
      fs.open(filename, flags, mode, (error, fd) => {
        ownedFd = fd;
        callback(error, fd);
      });
    },

    close: fs.close,

    write(_fd, _bytes, _offset, _length, _position, callback) {
      ++attempts;

      queueMicrotask(() => {
        if (mode === 'zero') {
          callback(null, 0);
        } else {
          callback(Object.assign(new Error('simulated EAGAIN'), { code: 'EAGAIN' }));
        }
      });
    }
  };
  const factory = createWriteStreams(failingFs, host, validate);
  const filename = 'simulated-' + mode + '.bin';
  const failing = factory.createWriteStream(filename);
  const ended = event(failing, 'close');
  let error;

  failing.on('error', (value) => {
    error = value;
  });

  failing.end('unwritten');
  await ended;
  check(error?.code === 'ERR_SYSTEM_ERROR' && attempts === 6, 'No-progress retry loop is bounded');
  check(
    failing.bytesWritten === 0 && failing.fd === null && fs.readFileSync(filename).length === 0,
    'Failure never invents progress and releases native fd'
  );
  let descriptorError;
  try {
    fs.fstatSync(ownedFd);
  } catch (failure) {
    descriptorError = failure.code;
  }
  check(descriptorError === 'EBADF', 'Retry exhaustion closes the actual OS descriptor');
  console.log('PASS internal writer simulated retry exhaustion: ' + mode);
}
