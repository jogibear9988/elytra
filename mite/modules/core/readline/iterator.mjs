/** Pull line delivery with bounded queue pressure and deterministic close retirement. */
export default function createIterator(rl, pause, resume) {
  let queue = [];
  let waiters = [];
  let done = false;
  let error;
  let failed = false;
  let paused = false;
  let size = 0;

  const settle = () => {
    while (waiters.length) {
      const waiter = waiters.shift();
      failed ? waiter.reject(error) : waiter.resolve({ done: true, value: undefined });
    }
  };

  function drained() {
    if (paused && queue.length < 512 && size < 524288) {
      paused = false;
      resume();
    }
  }

  const iterator = {
    next() {
      if (queue.length) {
        const value = queue.shift();
        size -= value.length;
        drained();
        return Promise.resolve({ done: false, value });
      }
      if (failed) {
        return Promise.reject(error);
      }
      if (done) {
        return Promise.resolve({ done: true, value: undefined });
      }
      return new Promise((resolve, reject) => waiters.push({ resolve, reject }));
    },

    return() {
      queue = [];
      size = 0;
      done = true;
      error = undefined;
      failed = false;
      settle();
      rl.close();
      return Promise.resolve({ done: true, value: undefined });
    },

    throw(value) {
      queue = [];
      size = 0;
      done = true;
      error = value;
      failed = true;
      settle();
      rl.close();
      return Promise.reject(value);
    },

    [Symbol.asyncIterator]() {
      return this;
    }
  };
  return {
    iterator,

    push(value) {
      if (done) {
        return false;
      }
      if (waiters.length) {
        waiters.shift().resolve({ done: false, value });
      } else {
        queue.push(value);
        size += value.length;
      }
      if (queue.length >= 1024 || size >= 1048576) {
        paused = true;
        pause();
      }
      return !paused;
    },

    finish(...args) {
      done = true;
      if (args.length) {
        const value = args[0];
        error = value;
        failed = true;
        queue = [];
        size = 0;
      }
      settle();
    }
  };
}
