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
            schedule(() => deliver(emitter, error, type, args));
          }
        ]);
      }
    } catch (error) {
      // Reading/calling a hostile thenable fails synchronously, as emit does.
      emitter.emit('error', error);
    }
  };
}
