/** Exercise observable stream behavior against actual Node, without sockets or a GUI. */
import * as web from 'node:stream/web';

function check(condition, message) {
  if (!condition) {
    throw new Error(message);
  }
}

async function rejects(promise, expected) {
  try {
    await promise;
  } catch (error) {
    check(
      typeof expected === 'function' ? error instanceof expected : error === expected,
      'wrong rejection'
    );
    return;
  }
  throw new Error('missing rejection');
}

async function run(name, action) {
  await action();
  console.log('PASS', name);
}

await run('constructor identities and writable globals', async () => {
  const names = [
    'ByteLengthQueuingStrategy',
    'CountQueuingStrategy',
    'ReadableByteStreamController',
    'ReadableStream',
    'ReadableStreamBYOBReader',
    'ReadableStreamBYOBRequest',
    'ReadableStreamDefaultController',
    'ReadableStreamDefaultReader',
    'TransformStream',
    'TransformStreamDefaultController',
    'WritableStream',
    'WritableStreamDefaultController',
    'WritableStreamDefaultWriter'
  ];
  for (const name of names) {
    check(web[name] === globalThis[name], name);
    const descriptor = Object.getOwnPropertyDescriptor(globalThis, name);
    check(
      descriptor.writable && descriptor.configurable && !descriptor.enumerable,
      name + ' descriptor'
    );
  }
  const original = globalThis.ReadableStream;
  globalThis.ReadableStream = null;
  check((await import('node:stream/web')).ReadableStream === original, 'module snapshot');
  globalThis.ReadableStream = original;
});

await run('readable pressure locks and close', async () => {
  let control;
  const stream = new ReadableStream(
    {
      start(controller) {
        control = controller;
      }
    },
    new CountQueuingStrategy({ highWaterMark: 2 })
  );
  check(control.desiredSize === 2, 'initial pressure');
  control.enqueue('one');
  control.enqueue('two');
  check(control.desiredSize === 0, 'queued pressure');
  const reader = stream.getReader();
  check(stream.locked && reader instanceof web.ReadableStreamDefaultReader, 'locked reader');
  await rejects(stream.cancel(), TypeError);
  check((await reader.read()).value === 'one' && control.desiredSize === 1, 'read pressure');
  control.close();
  check((await reader.read()).value === 'two', 'drain before eof');
  check((await reader.read()).done, 'eof');
  await reader.closed;
  reader.releaseLock();
  check(!stream.locked, 'release');
});

await run('pull async iteration and iterator cleanup', async () => {
  const values = [];
  let next = 0;
  for await (const value of new ReadableStream({
    pull(controller) {
      if (next < 4) {
        controller.enqueue(next++);
      } else {
        controller.close();
      }
    }
  })) {
    values.push(value);
  }
  check(values.join() === '0,1,2,3', 'pull order');
  let cancelled = false;
  const stream = new ReadableStream({
    start(controller) {
      controller.enqueue(1);
    },

    cancel() {
      cancelled = true;
    }
  });
  for await (const value of stream.values({ preventCancel: true })) {
    check(value === 1, 'iteration');
    break;
  }
  check(!cancelled && !stream.locked, 'preventCancel');
  await stream.cancel();
  check(cancelled, 'explicit cancel');
  let returned = false;

  function* source() {
    try {
      yield 1;
      yield 2;
    } finally {
      returned = true;
    }
  }

  for await (const value of ReadableStream.from(source())) {
    check(value === 1, 'from');
    break;
  }
  check(returned, 'iterator return');
});

await run('readable error and cancel identity', async () => {
  const reason = { reason: 'source' };
  const failed = new ReadableStream({
    start(controller) {
      controller.error(reason);
    }
  }).getReader();
  await rejects(failed.closed, reason);
  await rejects(failed.read(), reason);
  let observed;
  const reader = new ReadableStream({
    cancel(value) {
      observed = value;
    }
  }).getReader();
  const pending = reader.read();
  await reader.cancel(reason);
  check(observed === reason && (await pending).done, 'cancel pending read');
});

await run('writable pressure ready serialization and close', async () => {
  let release;
  const writes = [];
  const stream = new WritableStream(
    {
      write(value) {
        writes.push(value);
        return new Promise((resolve) => {
          release = resolve;
        });
      },

      close() {
        writes.push('closed');
      }
    },
    new CountQueuingStrategy({ highWaterMark: 1 })
  );
  const writer = stream.getWriter();
  await writer.ready;
  const first = writer.write('a');
  check(writer.desiredSize === 0, 'write pressure');
  let ready = false;
  const readiness = writer.ready.then(() => {
    ready = true;
  });
  await Promise.resolve();
  check(!ready && writes.join() === 'a', 'pending sink');
  const second = writer.write('b');
  release();
  await first;
  check(writes.join() === 'a,b', 'serialized writes');
  release();
  await second;
  await readiness;
  await writer.close();
  await writer.closed;
  check(writes.join() === 'a,b,closed', 'sink close');
  writer.releaseLock();
  check(!stream.locked, 'writable release');
});

await run('writable abort signal and reason', async () => {
  let controller;
  let observed;
  const reason = { reason: 'sink' };
  const writer = new WritableStream({
    start(value) {
      controller = value;
    },

    abort(value) {
      observed = value;
    }
  }).getWriter();
  check(controller.signal instanceof AbortSignal && !controller.signal.aborted, 'sink signal');
  const closed = writer.closed.catch((error) => error);
  await writer.abort(reason);
  check(
    controller.signal.aborted && controller.signal.reason === reason && observed === reason,
    'sink abort'
  );
  check((await closed) === reason, 'closed rejection');
});

await run('transform pipeline flush and sizes', async () => {
  const values = [];
  const transform = new TransformStream({
    transform(value, controller) {
      controller.enqueue(value * 2);
    },

    flush(controller) {
      controller.enqueue(99);
    }
  });
  await ReadableStream.from([1, 2, 3])
    .pipeThrough(transform)
    .pipeTo(
      new WritableStream({
        write(value) {
          values.push(value);
        }
      })
    );
  check(values.join() === '2,4,6,99', 'transform order');
  const strategy = new ByteLengthQueuingStrategy({ highWaterMark: 8 });
  check(strategy.highWaterMark === 8 && strategy.size(new Uint8Array(3)) === 3, 'byte size');
});

await run('pipe source error aborts destination', async () => {
  const reason = { reason: 'pipe source' };
  let aborted;
  await rejects(
    new ReadableStream({
      start(controller) {
        controller.error(reason);
      }
    }).pipeTo(
      new WritableStream({
        abort(value) {
          aborted = value;
        }
      })
    ),
    reason
  );
  check(aborted === reason, 'propagate source error');
});

await run('pipe destination error cancels source', async () => {
  const reason = { reason: 'pipe sink' };
  let cancelled;
  await rejects(
    new ReadableStream({
      start(controller) {
        controller.enqueue(1);
      },

      cancel(value) {
        cancelled = value;
      }
    }).pipeTo(
      new WritableStream({
        write() {
          throw reason;
        }
      })
    ),
    reason
  );
  check(cancelled === reason, 'propagate sink error');
});

await run('pipe signal abort releases locks', async () => {
  const control = new AbortController();
  const reason = { reason: 'pipe abort' };
  let cancelled;
  let aborted;
  const source = new ReadableStream({
    cancel(value) {
      cancelled = value;
    }
  });
  const target = new WritableStream({
    abort(value) {
      aborted = value;
    }
  });
  const piping = source.pipeTo(target, { signal: control.signal });
  control.abort(reason);
  await rejects(piping, reason);
  check(
    cancelled === reason && aborted === reason && !source.locked && !target.locked,
    'signal cleanup'
  );
});

await run('pipe prevention flags preserve endpoints', async () => {
  const control = new AbortController();
  const reason = { reason: 'prevent' };
  control.abort(reason);
  let touched = false;
  const source = new ReadableStream({
    cancel() {
      touched = true;
    }
  });
  const target = new WritableStream({
    abort() {
      touched = true;
    },

    close() {
      touched = true;
    }
  });
  await rejects(
    source.pipeTo(target, { signal: control.signal, preventAbort: true, preventCancel: true }),
    reason
  );
  check(!touched && !source.locked && !target.locked, 'preserved endpoints');
  await source.cancel();
  await target.close();
});

await run('BYOB native buffer transfer and offset', async () => {
  let control;
  const stream = new ReadableStream({
    type: 'bytes',

    start(controller) {
      control = controller;
    },

    pull(controller) {
      const request = controller.byobRequest;
      check(request instanceof ReadableStreamBYOBRequest, 'byob request');
      request.view[0] = 42;
      request.view[1] = 43;
      request.respond(2);
      controller.close();
    }
  });
  check(control instanceof ReadableByteStreamController, 'byte controller');
  const reader = stream.getReader({ mode: 'byob' });
  const buffer = new ArrayBuffer(8);
  const pending = reader.read(new Uint8Array(buffer, 2, 4));
  check(buffer.byteLength === 0, 'input buffer detached');
  const { value, done } = await pending;
  check(
    !done && value.byteOffset === 2 && value.length === 2 && value.join() === '42,43',
    'BYOB bytes'
  );
  check((await reader.read(new Uint8Array(4))).done, 'BYOB eof');
  reader.releaseLock();
});

await run('byte enqueue transfers and tee isolates buffers', async () => {
  const bytes = new Uint8Array([1, 2, 3]);
  const stream = new ReadableStream({
    type: 'bytes',

    start(controller) {
      controller.enqueue(bytes);
      controller.close();
    }
  });
  check(bytes.byteLength === 0, 'enqueue detaches');
  const [left, right] = stream.tee();
  const a = await left.getReader().read();
  const b = await right.getReader().read();
  a.value[0] = 9;
  check(b.value[0] === 1 && a.value.buffer !== b.value.buffer, 'tee byte isolation');
});

await run('tee cancellation aggregates reasons', async () => {
  let observed;
  const [left, right] = new ReadableStream({
    cancel(value) {
      observed = value;
    }
  }).tee();
  const first = left.cancel('left');
  await Promise.resolve();
  check(observed === undefined, 'wait for both branches');
  await Promise.all([first, right.cancel('right')]);
  check(observed.join() === 'left,right', 'tee reasons');
});

await run('transform errors reach both ends', async () => {
  const reason = { reason: 'transform' };
  const transform = new TransformStream({
    transform() {
      throw reason;
    }
  });
  const reader = transform.readable.getReader();
  const writer = transform.writable.getWriter();
  const read = rejects(reader.read(), reason);
  const readClosed = rejects(reader.closed, reason);
  const writeClosed = rejects(writer.closed, reason);
  await rejects(writer.write(1), reason);
  await Promise.all([read, readClosed, writeClosed]);
});

console.log('DONE web streams');
