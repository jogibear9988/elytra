/** Mite-only strict validation rejects unsupported writer capabilities before opening any file. */
import fs from 'node:fs';
import { check } from './support.mjs';

const cases = [
  [{ fs: {} }, 'ERR_MITE_UNSUPPORTED_API'],
  [{ flush: true }, 'ERR_MITE_UNSUPPORTED_API'],
  [{ flush: false }, 'ERR_MITE_UNSUPPORTED_API'],
  [{ unknownWriterOption: 1 }, 'ERR_MITE_UNSUPPORTED_API'],
  [{ decodeStrings: false }, 'ERR_MITE_UNSUPPORTED_API'],
  [{ highWaterMark: -1 }, 'ERR_INVALID_ARG_VALUE'],
  [{ highWaterMark: 1.5 }, 'ERR_INVALID_ARG_VALUE'],
  [{ highWaterMark: '1' }, 'ERR_INVALID_ARG_VALUE'],
  [{ highWaterMark: NaN }, 'ERR_INVALID_ARG_VALUE'],
  [{ highWaterMark: Infinity }, 'ERR_INVALID_ARG_VALUE'],
  [{ encoding: 'unknown-codec' }, 'ERR_INVALID_ARG_VALUE'],
  [{ encoding: 42 }, 'ERR_INVALID_ARG_VALUE'],
  [{ start: -1 }, 'ERR_OUT_OF_RANGE'],
  [{ start: 0.5 }, 'ERR_OUT_OF_RANGE'],
  [{ start: Infinity }, 'ERR_OUT_OF_RANGE'],
  [{ start: Number.MAX_SAFE_INTEGER + 1 }, 'ERR_OUT_OF_RANGE'],
  [{ start: '0' }, 'ERR_OUT_OF_RANGE'],
  [{ autoClose: 0 }, 'ERR_INVALID_ARG_TYPE'],
  [{ autoClose: 'false' }, 'ERR_INVALID_ARG_TYPE'],
  [{ emitClose: 1 }, 'ERR_INVALID_ARG_TYPE'],
  [{ emitClose: 'false' }, 'ERR_INVALID_ARG_TYPE'],
  [{ fd: -1 }, 'ERR_INVALID_ARG_TYPE'],
  [{ fd: 1.5 }, 'ERR_INVALID_ARG_TYPE'],
  [{ fd: '1' }, 'ERR_INVALID_ARG_TYPE'],
  [{ fd: NaN }, 'ERR_INVALID_ARG_TYPE'],
  [{ fd: {} }, 'ERR_INVALID_ARG_TYPE'],
  [null, 'ERR_INVALID_ARG_TYPE'],
  [42, 'ERR_INVALID_ARG_TYPE']
];
fs.writeFileSync('sentinel.bin', 'must remain unchanged');
for (const [index, [options, code]] of cases.entries()) {
  for (const filename of ['sentinel.bin', 'invalid-' + index + '.bin']) {
    let failure;
    try {
      const stream = fs.createWriteStream(filename, options);

      stream.on('error', () => {});

      stream.destroy();
    } catch (error) {
      failure = error;
    }
    check(
      failure instanceof Error && failure.code === code,
      'Writer validation case ' + index + ' expected ' + code
    );
  }
}
// A constructor that scheduled open() before throwing could mutate files on a later native tick.
await new Promise((resolve) => setTimeout(resolve, 20));
check(
  fs.readFileSync('sentinel.bin', 'utf8') === 'must remain unchanged',
  'Invalid options never truncate an existing file'
);
for (let index = 0; index < cases.length; ++index) {
  check(!fs.existsSync('invalid-' + index + '.bin'), 'Invalid options never create files');
}
console.log(
  'PASS Mite WriteStream validation: unsupported options, strict values, no native open side effects'
);
