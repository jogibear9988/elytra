/** Compare incremental bridges with actual Node, including terminal completion. */
import { Readable, Writable, Duplex, Transform, finished } from 'node:stream';
import { pipeline } from 'node:stream/promises';
import { ReadableStream, WritableStream, TransformStream } from 'node:stream/web';
import { setTimeout as sleep } from 'node:timers/promises';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

async function rejected(promise, expected) {
  try {
    await promise;
  } catch (error) {
    check(typeof expected === 'function' ? expected(error) : error === expected, 'wrong rejection');
    return;
  }
  throw new Error('missing rejection');
}

function completion(node) {
  return new Promise((resolve, reject) => {
    const cleanup = finished(node, (error) => {
      cleanup();
      error ? reject(error) : resolve();
    });
  });
}

async function run(name, action) {
  await action();
  console.log('PASS', name);
}

await run('fromWeb bytes encoding and retained lock', async () => {
  const source = ReadableStream.from([new Uint8Array([104, 195]), new Uint8Array([169])]);
  const node = Readable.fromWeb(source, { encoding: 'utf8', highWaterMark: 1 });
  let text = '';
  for await (const chunk of node) {
    text += chunk;
  }
  check(text === 'hé' && source.locked && node.destroyed, 'decoded source');
});

await run('fromWeb objects and pipeline', async () => {
  const objects = [{ value: 1 }, { value: 2 }];
  const seen = [];
  await pipeline(
    Readable.fromWeb(ReadableStream.from(objects), { objectMode: true }),
    new Writable({
      objectMode: true,

      write(value, encoding, callback) {
        seen.push(value);
        callback();
      }
    })
  );
  check(seen[0] === objects[0] && seen[1] === objects[1], 'object identity');
});

await run('fromWeb readable pressure and destroy reason', async () => {
  let pulls = 0;
  let cancelled;
  const source = new ReadableStream(
    {
      pull(controller) {
        pulls++;
        controller.enqueue(new Uint8Array([1]));
      },

      cancel(reason) {
        cancelled = reason;
      }
    },
    { highWaterMark: 0 }
  );
  const node = Readable.fromWeb(source, { highWaterMark: 1 });
  await sleep(5);
  check(pulls === 0, 'no eager node read');
  const reason = new Error('cancel read');
  const done = rejected(completion(node), reason);
  node.destroy(reason);
  await done;
  check(cancelled === reason && source.locked, 'cancel identity');
});

await run('fromWeb readable errors without consumption', async () => {
  let control;
  const source = new ReadableStream({
    start(controller) {
      control = controller;
    }
  });
  const node = Readable.fromWeb(source);
  const reason = new Error('source failed');
  const done = rejected(completion(node), reason);
  control.error(reason);
  await done;
  check(node.destroyed, 'error destroys node');
});

await run('fromWeb writable pressure encoding and close', async () => {
  const seen = [];
  const target = new WritableStream({
    async write(value) {
      await sleep(1);
      seen.push(Buffer.from(value).toString());
    },

    close() {
      seen.push('closed');
    }
  });
  const node = Writable.fromWeb(target, { highWaterMark: 1 });
  const done = completion(node);
  check(!node.write('a'), 'node backpressure');
  node.end('b');
  await done;
  check(seen.join() === 'a,b,closed' && target.locked, 'sink ordering and ownership');
});

await run('fromWeb writable decodeStrings false', async () => {
  let value;
  const node = Writable.fromWeb(
    new WritableStream({
      write(chunk) {
        value = chunk;
      }
    }),
    { decodeStrings: false }
  );
  const done = completion(node);
  node.end('text');
  await done;
  check(value === 'text', 'string sink');
});

await run('fromWeb writable error and abort', async () => {
  const reason = new Error('sink failed');
  const node = Writable.fromWeb(
    new WritableStream({
      write() {
        throw reason;
      }
    })
  );
  const done = rejected(completion(node), reason);
  node.end('a');
  await done;
  let aborted;
  const other = Writable.fromWeb(
    new WritableStream({
      abort(value) {
        aborted = value;
      }
    })
  );
  const closed = rejected(completion(other), reason);
  other.destroy(reason);
  await closed;
  check(aborted === reason, 'sink abort identity');
});

await run('fromWeb live and pre-aborted signals', async () => {
  for (const early of [false, true]) {
    const control = new AbortController();
    const reason = new Error('stop');

    control.signal.addEventListener('abort', (event) => event.stopImmediatePropagation());

    if (early) {
      control.abort(reason);
    }
    let cancelled;
    const node = Readable.fromWeb(
      new ReadableStream({
        cancel(value) {
          cancelled = value;
        }
      }),
      { signal: control.signal }
    );
    const done = rejected(
      completion(node),
      (error) => error.name === 'AbortError' && error.code === 'ABORT_ERR' && error.cause === reason
    );
    if (!early) {
      control.abort(reason);
    }
    await done;
    check(cancelled?.cause === reason, 'signal cancels reader');
  }
});

await run('toWeb readable byte snapshots and object identity', async () => {
  const bytes = Buffer.from('abc');
  const reader = Readable.toWeb(Readable.from([bytes], { objectMode: false })).getReader();
  const value = (await reader.read()).value;
  check(value instanceof Uint8Array && !Buffer.isBuffer(value), 'web byte type');
  bytes[0] = 120;
  check(value[0] === 97 && (await reader.read()).done, 'independent snapshot');
  const object = { object: true };
  const objects = Readable.toWeb(Readable.from([object])).getReader();
  check(
    (await objects.read()).value === object && (await objects.read()).done,
    'web object identity'
  );
});

await run('toWeb readable pressure and cancel', async () => {
  let pulls = 0;
  let destroyed;
  const node = new Readable({
    objectMode: true,
    highWaterMark: 1,

    read() {
      this.push(++pulls);
    },

    destroy(reason, callback) {
      destroyed = reason;
      callback(reason);
    }
  });
  const source = Readable.toWeb(node, {
    strategy: {
      highWaterMark: 1,

      size() {
        return 1;
      }
    }
  });
  await sleep(10);
  check(pulls > 0 && pulls <= 3, 'bounded prefetch');
  const reason = new Error('web cancel');
  const closed = rejected(completion(node), reason);
  await source.cancel(reason);
  await closed;
  check(node.destroyed && destroyed === reason, 'resource close');
});

await run('toWeb readable errors and premature close', async () => {
  const reason = new Error('node failed');
  await rejected(
    Readable.toWeb(
      new Readable({
        read() {
          this.destroy(reason);
        }
      })
    )
      .getReader()
      .read(),
    reason
  );
  await rejected(
    Readable.toWeb(
      new Readable({
        read() {
          this.destroy();
        }
      })
    )
      .getReader()
      .read(),
    (error) =>
      error.name === 'AbortError' &&
      error.code === 'ABORT_ERR' &&
      error.cause?.code === 'ERR_STREAM_PREMATURE_CLOSE'
  );
});

await run('toWeb writable drain and final', async () => {
  let release;
  const values = [];
  const node = new Writable({
    highWaterMark: 1,

    write(chunk, encoding, callback) {
      values.push(chunk.toString());
      release = callback;
    },

    final(callback) {
      values.push('final');
      callback();
    }
  });
  const writer = Writable.toWeb(node).getWriter();
  let done = false;
  const writing = writer.write(new Uint8Array([97])).then(() => {
    done = true;
  });
  await sleep(5);
  check(!done && values.join() === 'a', 'wait for drain');
  release();
  await writing;
  await writer.close();
  check(values.join() === 'a,final' && node.writableFinished, 'final completion');
});

await run('toWeb accepted write resolves before callback', async () => {
  let release;
  let complete = false;
  const node = new Writable({
    write(chunk, encoding, callback) {
      release = () => {
        complete = true;
        callback();
      };
    }
  });
  const writer = Writable.toWeb(node).getWriter();
  await writer.write(Buffer.from('a'));
  check(!complete, 'accepted write timing');
  release();
  await writer.close();
});

await run('toWeb writable errors abort and destroyed destinations', async () => {
  const reason = new Error('node sink');
  const writer = Writable.toWeb(
    new Writable({
      highWaterMark: 1,

      write(chunk, encoding, callback) {
        callback(reason);
      }
    })
  ).getWriter();
  await rejected(writer.write('a'), reason);
  await rejected(writer.closed, reason);
  let destroyed;
  const target = new Writable({
    write(chunk, encoding, callback) {
      callback();
    },

    destroy(value, callback) {
      destroyed = value;
      callback(value);
    }
  });
  await Writable.toWeb(target).abort(reason);
  check(destroyed === reason, 'abort reason');
  const closed = new Writable({
    write(chunk, encoding, callback) {
      callback();
    }
  });
  closed.destroy();
  const ended = Writable.toWeb(closed).getWriter();
  await ended.closed;
  await rejected(ended.write('a'), (error) => error instanceof TypeError);
});

await run('Duplex fromWeb transform pair', async () => {
  const transform = new TransformStream({
    transform(chunk, controller) {
      controller.enqueue(Buffer.from(chunk.toString().toUpperCase()));
    }
  });
  const node = Duplex.fromWeb(transform, { highWaterMark: 2 });
  check(!node.allowHalfOpen && !node.readableObjectMode, 'duplex defaults');
  const values = [];
  const reading = (async () => {
    for await (const chunk of node) {
      values.push(chunk.toString());
    }
  })();
  node.write('a');
  node.end('b');
  await reading;
  check(
    values.join('') === 'AB' && transform.readable.locked && transform.writable.locked,
    'duplex pair'
  );
});

await run('Duplex toWeb transform and roundtrip', async () => {
  const pair = Duplex.toWeb(
    new Transform({
      transform(chunk, encoding, callback) {
        callback(null, Buffer.from(chunk.toString().toUpperCase()));
      }
    })
  );
  const seen = [];
  const reading = pair.readable.pipeTo(
    new WritableStream({
      write(value) {
        seen.push(Buffer.from(value).toString());
      }
    })
  );
  const writer = pair.writable.getWriter();
  await writer.write(Buffer.from('a'));
  await writer.close();
  await reading;
  check(seen.join('') === 'A', 'duplex to web');
  const roundtrip = Duplex.fromWeb(
    Duplex.toWeb(
      new Transform({
        transform(chunk, encoding, callback) {
          callback(null, chunk);
        }
      })
    )
  );
  const received = [];
  const done = (async () => {
    for await (const chunk of roundtrip) {
      received.push(chunk);
    }
  })();
  roundtrip.end('roundtrip');
  await done;
  check(Buffer.concat(received).toString() === 'roundtrip', 'roundtrip');
});

await run('validation and pair lock rollback', async () => {
  for (const factory of [
    () => Readable.fromWeb({}),
    () => Writable.fromWeb({}),
    () => Readable.toWeb({}),
    () => Writable.toWeb({}),
    () => Duplex.fromWeb({}),
    () => Duplex.toWeb({})
  ]) {
    let code;
    try {
      factory();
    } catch (error) {
      code = error.code;
    }
    check(code === 'ERR_INVALID_ARG_TYPE', 'brand rejection');
  }
  const source = new ReadableStream();
  const target = new WritableStream();
  const lock = target.getWriter();
  let failed = false;
  try {
    Duplex.fromWeb({ readable: source, writable: target });
  } catch {
    failed = true;
  }
  check(failed && !source.locked, 'atomic pair validation');
  lock.releaseLock();
  await source.cancel();
  await target.close();
});

console.log('DONE web adapters');
