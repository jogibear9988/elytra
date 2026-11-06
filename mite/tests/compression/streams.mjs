/** Decode independent Python fixtures and verify streaming/error/ownership semantics. */
import { readFileSync } from 'node:fs';
import { DecompressionStream as WebDecoder } from 'node:stream/web';
import { setTimeout as sleep } from 'node:timers/promises';
const fixtures = JSON.parse(readFileSync(process.argv[2], 'utf8'));

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

async function run(name, action) {
  await action();
  console.log('PASS', name);
}

async function rejected(promise, code) {
  try {
    await promise;
  } catch (error) {
    check(error instanceof TypeError && error.code === code, 'decompression error');
    return;
  }
  throw new Error('missing error');
}

await run('global and module identity', () => {
  check(WebDecoder === DecompressionStream, 'shared constructors');
  for (const format of ['invalid', '', undefined]) {
    let rejected = false;
    try {
      new DecompressionStream(format);
    } catch (error) {
      rejected = error instanceof TypeError;
    }
    check(rejected, 'invalid format');
  }
});

for (const format of ['gzip', 'deflate', 'deflate-raw']) {
  await run(format + ' fragmented header and expansion', async () => {
    const bytes = Buffer.from(fixtures[format], 'hex');
    let offset = 0;
    const source = new ReadableStream({
      pull(controller) {
        if (offset === bytes.length) {
          controller.close();
        } else {
          controller.enqueue(bytes.subarray(offset, ++offset));
        }
      }
    });
    const result = await new Response(source.pipeThrough(new DecompressionStream(format))).bytes();
    check(
      result.length === fixtures.text && result.every((value) => value === 97),
      'decompressed bytes'
    );
  });
}

await run('gzip members', async () => {
  const bytes = Buffer.from(fixtures.gzip, 'hex');
  const result = await new Response(
    ReadableStream.from([bytes, bytes]).pipeThrough(new DecompressionStream('gzip'))
  ).bytes();
  check(
    result.length === fixtures.text * 2 && result.every((value) => value === 97),
    'concatenated gzip'
  );
});

await run('checksum and truncation errors reach both endpoints', async () => {
  for (const [data, code] of [
    [Buffer.from('bad'), 'Z_DATA_ERROR'],
    [Buffer.from(fixtures.gzip, 'hex').subarray(0, 12), 'Z_BUF_ERROR']
  ]) {
    const decoder = new DecompressionStream('gzip');
    const consuming = rejected(new Response(decoder.readable).bytes(), code);
    const writing = rejected(ReadableStream.from([data]).pipeTo(decoder.writable), code);
    await Promise.all([consuming, writing]);
  }
});

await run('Web gzip rejects trailing padding', async () => {
  const bytes = Buffer.concat([Buffer.from(fixtures.gzip, 'hex'), Buffer.alloc(3)]);
  await rejected(
    new Response(ReadableStream.from([bytes]).pipeThrough(new DecompressionStream('gzip'))).bytes(),
    'ERR_TRAILING_JUNK_AFTER_STREAM_END'
  );
});

await run('writer abort cancels a stalled decoder', async () => {
  const decoder = new DecompressionStream('gzip');
  const reader = decoder.readable.getReader();
  const pending = reader.read().catch((error) => error);
  const reason = new Error('abort decoder');
  await decoder.writable.abort(reason);
  check((await pending) === reason, 'writer abort reason');
});
console.log('DONE compression streams');
