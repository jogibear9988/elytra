/** Observe listener thenables without Promise assimilation or repeated getter reads. */
export default function createRejectionObserver(capture, rejectionSymbol, schedule) {
  const apply = Reflect.apply;

  function deliver(emitter, error, type, args) {
    const handler = emitter[rejectionSymbol];
    if (typeof handler === 'function') {
      apply(handler, emitter, [error, type, ...args]);
      return;
    }
    // An async error listener must not recursively recapture its own rejection.
    const previous = emitter[capture];
    emitter[capture] = false;
    try {
      emitter.emit('error', error);
    } finally {
      emitter[capture] = previous;
    }
  }

  function enqueue(record) {
    // A completed callback can remain conservatively reachable from a native
    // stack slot. Give it a separate scope and clear its private delivery data,
    // so that reachability does not extend emitter/argument ownership.
    schedule(() => {
      try {
        deliver(record.emitter, record.error, record.type, record.args);
      } finally {
        record.emitter = undefined;
        record.error = undefined;
        record.type = undefined;
        record.args = undefined;
      }
    });
  }

  return function observe(emitter, result, type, args) {
    if (!emitter[capture] || result === undefined || result === null) {
      return;
    }
    try {
      const then = result.then;
      if (typeof then === 'function') {
        apply(then, result, [
          undefined,
          (error) => {
            // The native nextTick queue owns delivery outside the Promise reaction.
            enqueue({ emitter, error, type, args });
          }
        ]);
      }
    } catch (error) {
      // Reading/calling a hostile thenable fails synchronously, as emit does.
      emitter.emit('error', error);
    }
  };
}
