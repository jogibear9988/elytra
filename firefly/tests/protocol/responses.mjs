/** Private response controller exercised in real Node/Mite. No native protocol routing claimed. */
import create from '../../core/protocol/responses.mjs';

const transport = { Request, Response, Headers, AbortController, ReadableStream, Buffer };

const error = (code, message) => Object.assign(Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

const make = () => create(transport, error, unsupported);

const descriptor = {
  url: 'vscode-managed-remote-resource://window:1/a',
  method: 'GET',
  headers: { 'x-input': 'present' }
};

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

async function rejects(promise, code) {
  let caught;
  try {
    await promise;
  } catch (failure) {
    caught = failure;
  }
  check(caught?.code === code, 'Expected ' + code + ', got ' + caught?.code);
}

function deferred() {
  let resolve;
  const promise = new Promise((done) => {
    resolve = done;
  });
  return { promise, resolve };
}

async function body(api, id, generation) {
  const chunks = [];
  while (true) {
    const item = await api.pull(id, generation);
    check(item.chunk.byteLength <= 65536, 'Bounded wire chunk');
    if (item.done) {
      return Buffer.concat(chunks);
    }
    chunks.push(Buffer.from(item.chunk));
  }
}

{
  const api = make();
  let request;
  const metadata = await api.start(1, 1, descriptor, async (value) => {
    request = value;
    await Promise.resolve();
    return new Response(Uint8Array.of(0, 255, 1), {
      status: 404,
      headers: { 'content-type': 'application/octet-stream', 'content-length': '3' }
    });
  });
  check(
    request instanceof Request &&
      request.url === descriptor.url &&
      request.headers.get('x-input') === 'present',
    'Genuine Request metadata'
  );
  check(
    metadata.status === 404 && metadata.contentLength === 3 && !metadata.bodyNull,
    'Asynchronous Response status and length'
  );
  check(
    metadata.headers.some(
      ([key, value]) => key === 'content-type' && value === 'application/octet-stream'
    ),
    'MIME metadata'
  );
  check((await body(api, 1, 1)).equals(Buffer.from([0, 255, 1])), 'Binary response bytes');
  await rejects(api.pull(1, 1), 'ERR_FIREFLY_PROTOCOL_CANCELED');
  const captured = make();
  const response = new Response(Uint8Array.of(4, 5));
  const pending = captured.start(20, 1, descriptor, () => response);
  const status = Object.getOwnPropertyDescriptor(Response.prototype, 'status');
  const entries = Headers.prototype.entries;
  const getReader = ReadableStream.prototype.getReader;
  const savedRequest = globalThis.Request;
  const savedResponse = globalThis.Response;
  try {
    Object.defineProperty(Response.prototype, 'status', {
      configurable: true,

      get() {
        throw Error('Public status replaced');
      }
    });

    Headers.prototype.entries = () => {
      throw Error('Public entries replaced');
    };

    ReadableStream.prototype.getReader = () => {
      throw Error('Public reader replaced');
    };

    globalThis.Request = function () {
      throw Error('Global Request replaced');
    };

    globalThis.Response = function () {
      throw Error('Global Response replaced');
    };

    check((await pending).status === 200, 'Captured accessors and constructors remain genuine');
    check((await captured.pull(20, 1)).chunk[0] === 4, 'Captured stream adapter remains owned');

    captured.cancel(20, 1);
  } finally {
    globalThis.Request = savedRequest;
    globalThis.Response = savedResponse;
    Object.defineProperty(Response.prototype, 'status', status);
    Headers.prototype.entries = entries;
    ReadableStream.prototype.getReader = getReader;
    captured.stop();
  }
  api.stop();
  console.log(
    'PASS genuine asynchronous Request Response metadata MIME binary and terminal ownership'
  );
}

{
  const api = make();
  let pulls = 0;
  const source = new Uint8Array(150000).fill(7);
  const stream = new ReadableStream(
    {
      pull(controller) {
        pulls++;
        controller.enqueue(source);
        controller.close();
      }
    },
    { highWaterMark: 0 }
  );
  await api.start(2, 1, descriptor, () => new Response(stream));
  check(pulls === 0, 'No reader pull without native demand credit');
  const first = await api.pull(2, 1, 1);
  check(first.chunk.length === 1 && pulls === 1, 'One credit byte does one underlying read');
  source.fill(9);
  check(
    (await api.pull(2, 1, 65536)).chunk.every((value) => value === 7),
    'Retained remainder owns a snapshot'
  );
  const rest = await body(api, 2, 1);
  check(
    rest.length === 150000 - 65537 && rest.every((value) => value === 7) && pulls === 1,
    'Credit splits bounded remainder without speculative pulls'
  );
  await rejects(api.pull(3, 1), 'ERR_FIREFLY_PROTOCOL_CANCELED');
  api.stop();
  console.log('PASS demand-only reads bounded credit splitting and owned binary remainder');
}

{
  const api = make();
  for (const status of [204, 205, 304]) {
    const metadata = await api.start(status, 1, descriptor, () => new Response(null, { status }));
    check(metadata.bodyNull && (await api.pull(status, 1)).done, 'Null-body HTTP status');
  }
  let canceled = 0;
  let pulled = 0;
  const metadata = await api.start(
    4,
    1,
    { ...descriptor, method: 'HEAD' },
    () =>
      new Response(
        new ReadableStream(
          {
            pull() {
              pulled++;
            },

            cancel() {
              canceled++;
            }
          },
          { highWaterMark: 0 }
        ),
        { headers: { 'content-length': '99' } }
      )
  );
  check(
    metadata.bodyNull && metadata.contentLength === 99 && pulled === 0 && canceled === 1,
    'HEAD retains metadata but cancels body without reading'
  );
  check((await api.pull(4, 1)).done, 'HEAD EOF');
  await rejects(
    api.start(5, 1, descriptor, () => Response.error()),
    'ERR_FIREFLY_PROTOCOL_NETWORK'
  );
  await rejects(
    api.start(5, 2, descriptor, () => Object.create(Response.prototype)),
    'ERR_FIREFLY_PROTOCOL_DATA'
  );
  await rejects(
    api.start(5, 3, descriptor, () => ({ status: 200 })),
    'ERR_FIREFLY_PROTOCOL_DATA'
  );
  api.stop();
  console.log('PASS HEAD null-body status cancellation network error and genuine Response brand');
}

{
  const api = make();
  const held = deferred();
  let request;
  let canceledReason;
  const reason = error('OWN_ABORT', 'Explicit cancellation');
  const pending = api.start(6, 1, descriptor, (value) => {
    request = value;
    return held.promise;
  });
  await Promise.resolve();
  api.cancel(6, 1, reason);
  await rejects(pending, 'OWN_ABORT');
  check(
    request.signal.aborted && request.signal.reason === reason,
    'Request signal owns original abort reason'
  );
  held.resolve(
    new Response(
      new ReadableStream(
        {
          cancel(value) {
            canceledReason = value;
          }
        },
        { highWaterMark: 0 }
      )
    )
  );
  await Promise.resolve();
  await Promise.resolve();
  await Promise.resolve();
  check(canceledReason === reason, 'Late handler response is canceled');
  await api.start(6, 2, descriptor, () => new Response('new'));
  api.cancel(6, 1);
  check((await body(api, 6, 2)).toString() === 'new', 'Stale generation cannot cancel replacement');
  api.stop();
  console.log('PASS cancellation abort signal late body cleanup and stale generation isolation');
}

{
  const api = make();
  const held = deferred();
  let canceled = 0;
  await api.start(
    7,
    1,
    descriptor,
    () =>
      new Response(
        new ReadableStream(
          {
            pull() {
              return held.promise;
            },

            cancel() {
              canceled++;
            }
          },
          { highWaterMark: 0 }
        )
      )
  );
  const reading = api.pull(7, 1);
  await rejects(api.pull(7, 1), 'ERR_FIREFLY_PROTOCOL_DATA');
  api.cancel(7, 1);
  await rejects(reading, 'ERR_FIREFLY_PROTOCOL_CANCELED');
  check(canceled === 1, 'Outstanding reader canceled once');
  held.resolve();
  await api.start(
    8,
    1,
    descriptor,
    () => new Response(Uint8Array.of(1), { headers: { 'content-length': '2' } })
  );
  await api.pull(8, 1);
  await rejects(api.pull(8, 1), 'ERR_FIREFLY_PROTOCOL_DATA');
  await api.start(
    9,
    1,
    descriptor,
    () => new Response('too long', { headers: { 'content-length': '1' } })
  );
  await rejects(api.pull(9, 1), 'ERR_FIREFLY_PROTOCOL_DATA');
  api.stop();
  console.log(
    'PASS pending reader cancellation concurrent pull rejection and content length integrity'
  );
}

{
  const api = make();
  for (const headers of [
    { 'transfer-encoding': 'chunked' },
    { connection: 'close' },
    { 'content-length': '68719476737' },
    Object.fromEntries(Array.from({ length: 65 }, (_, i) => ['x-' + i, 'v']))
  ]) {
    await rejects(
      api.start(50, 1, descriptor, () => new Response('x', { headers })),
      Object.keys(headers).length > 64 ? 'ERR_FIREFLY_PROTOCOL_LIMIT' : 'ERR_FIREFLY_PROTOCOL_DATA'
    );
  }
  await rejects(
    api.start(
      51,
      1,
      descriptor,
      () => new Response('x', { headers: { 'x-large': '\\'.repeat(17000) } })
    ),
    'ERR_FIREFLY_PROTOCOL_LIMIT'
  );
  await api.start(
    10,
    1,
    descriptor,
    () =>
      new Response(
        new ReadableStream(
          {
            start(controller) {
              controller.enqueue(new Uint8Array(1048577));
            }
          },
          { highWaterMark: 0 }
        )
      )
  );
  await rejects(api.pull(10, 1), 'ERR_FIREFLY_PROTOCOL_LIMIT');
  await api.start(
    11,
    1,
    descriptor,
    () =>
      new Response(
        new ReadableStream(
          {
            start(controller) {
              controller.enqueue('not bytes');
            }
          },
          { highWaterMark: 0 }
        )
      )
  );
  await rejects(api.pull(11, 1), 'ERR_FIREFLY_PROTOCOL_DATA');
  await rejects(
    api.start(12, 1, { ...descriptor, method: 'POST' }, () => new Response()),
    'ERR_FIREFLY_UNSUPPORTED_API'
  );
  const queued = [];
  for (let id = 100; id < 164; id++) {
    const started = api.start(id, 1, descriptor, () => new Promise(() => {}));
    queued.push(rejects(started, 'ERR_FIREFLY_PROTOCOL_CANCELED'));
  }
  await rejects(
    api.start(164, 1, descriptor, () => new Response()),
    'ERR_FIREFLY_PROTOCOL_LIMIT'
  );
  api.stop();
  api.stop();
  await Promise.all(queued);
  await rejects(
    api.start(999, 1, descriptor, () => new Response()),
    'ERR_FIREFLY_PROTOCOL_CANCELED'
  );
  console.log('PASS source chunk type size quota unsupported uploads and permanent shutdown');
}
console.log('DONE private protocol responses');
