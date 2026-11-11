/** Native threads own independent LibJS VMs; this facade owns realm-local events and snapshots. */
import createClone from './clone.mjs';

export default function createWorkers(host) {
  const { native, events, types, path, fileURLToPath, URL, process } = host;
  const { EventEmitter } = events;
  const tick = process.nextTick.bind(process);
  const clone = createClone(types);
  const workers = new WeakMap();
  const ports = new WeakMap();
  const secret = {};

  function unsupported(name) {
    throw Object.assign(new Error(`${name} is not implemented`), {
      code: 'ERR_MITE_UNSUPPORTED_API'
    });
  }

  function invalid(message) {
    throw Object.assign(new TypeError(message), { code: 'ERR_INVALID_ARG_TYPE' });
  }

  function transfer(value) {
    if (value === undefined) {
      return;
    }
    if (!Array.isArray(value)) {
      invalid('transferList must be an array');
    }
    if (value.length) {
      unsupported('Worker message transfer lists');
    }
  }

  function status(result) {
    if (result !== 0) {
      throw Object.assign(new Error('Worker channel cannot admit this message'), {
        code: result === native.queueFull ? 'ERR_WORKER_MESSAGING_FAILED' : 'ERR_WORKER_NOT_RUNNING'
      });
    }
  }

  function workerState(value) {
    const state = workers.get(value);
    if (!state) {
      invalid('Invalid Worker receiver');
    }
    return state;
  }

  function portState(value) {
    const state = ports.get(value);
    if (!state) {
      invalid('Invalid MessagePort receiver');
    }
    return state;
  }

  class MessagePort extends EventEmitter {
    constructor(token) {
      super();
      if (token !== secret) {
        throw new TypeError('Illegal constructor');
      }
      const state = {
        closed: false,
        queue: [],
        bytes: 0,
        started: false,
        scheduled: false,
        explicitRef: null
      };
      ports.set(this, state);

      const updateRef = () => {
        if (!state.closed) {
          native.parentRef(state.explicitRef ?? this.listenerCount('message') > 0);
        }
      };

      const pump = () => {
        if (state.scheduled || !state.started) {
          return;
        }
        state.scheduled = true;

        tick(() => {
          state.scheduled = false;
          while (!state.closed && state.queue.length && this.listenerCount('message')) {
            const wire = state.queue.shift();
            state.bytes -= wire.byteLength;
            this.emit('message', clone.decode(wire));
          }
        });
      };

      this.on('newListener', (event) => {
        if (event === 'message') {
          state.started = true;

          tick(() => {
            updateRef();
            pump();
          });
        }
      });

      this.on('removeListener', (event) => {
        if (event === 'message') {
          updateRef();
        }
      });

      state.pump = pump;
      state.updateRef = updateRef;

      native.parentSubscribe((kind, wire) => {
        if (state.closed) {
          return;
        }
        if (kind === 'message') {
          if (state.queue.length >= 64 || state.bytes + wire.byteLength > 16 * 1024 * 1024) {
            this.close();
            this.emit(
              'messageerror',
              Object.assign(new Error('Worker receive queue is full'), {
                code: 'ERR_WORKER_MESSAGING_FAILED'
              })
            );
            return;
          }
          state.queue.push(wire);
          state.bytes += wire.byteLength;
          pump();
        } else if (kind === 'close') {
          state.closed = true;
          state.queue.length = 0;
          state.bytes = 0;
          this.emit('close');
        }
      });

      native.parentRef(false);
    }

    postMessage(value, transferList) {
      const state = portState(this);
      if (state.closed) {
        return;
      }
      transfer(transferList);
      status(native.parentPost(clone.encode(value)));
    }

    start() {
      const state = portState(this);
      state.started = true;
      state.pump();
    }

    ref() {
      const state = portState(this);
      state.explicitRef = true;
      state.updateRef();
      return this;
    }

    unref() {
      const state = portState(this);
      state.explicitRef = false;
      state.updateRef();
      return this;
    }

    hasRef() {
      const state = portState(this);
      return !state.closed && (state.explicitRef ?? this.listenerCount('message') > 0);
    }

    close() {
      const state = portState(this);
      if (state.closed) {
        return;
      }
      state.closed = true;
      state.queue.length = 0;
      state.bytes = 0;
      native.parentClose();

      tick(() => this.emit('close'));
    }
  }
  class Worker extends EventEmitter {
    constructor(filename, options = {}) {
      super();
      if (typeof filename !== 'string') {
        if (filename instanceof URL) {
          filename = fileURLToPath(filename);
        } else {
          invalid('Worker filename must be a path or file URL');
        }
      }
      if (
        !filename ||
        filename.includes('\0') ||
        /[\uD800-\uDFFF]/u.test(filename.replace(/[\uD800-\uDBFF][\uDC00-\uDFFF]/gu, ''))
      ) {
        invalid('Worker filename must be valid Unicode without NUL');
      }
      if (!options || typeof options !== 'object' || Array.isArray(options)) {
        invalid('Worker options must be an object');
      }
      let name = '';
      for (const key of Object.keys(options)) {
        const value = options[key];
        if (key === 'name') {
          if (typeof value !== 'string') {
            invalid('Worker name must be a string');
          }
          if (value.includes('\0') || value.length > 256) {
            invalid('Invalid Worker name');
          }
          name = value;
        } else if (key === 'execArgv' && Array.isArray(value) && value.length === 0) {
          // No engine flags are requested.
        } else {
          unsupported(`Worker option ${key}`);
        }
      }
      if (!path.isAbsolute(filename) && !filename.startsWith('./') && !filename.startsWith('../')) {
        throw Object.assign(
          new TypeError('Worker filename must be absolute or start with ./ or ../'),
          { code: 'ERR_WORKER_PATH' }
        );
      }
      const state = {
        id: 0,
        exited: false,
        code: undefined,
        resolve: null,
        termination: null,
        name
      };
      workers.set(this, state);
      state.id = native.create(path.resolve(filename), name, (kind, value) => {
        if (state.exited) {
          return;
        }
        if (kind === 'message') {
          this.emit('message', clone.decode(value));
        } else if (kind === 'error') {
          const error = new Error(value.message);
          if (typeof value.name === 'string') {
            error.name = value.name;
          }
          if (typeof value.code === 'string') {
            error.code = value.code;
          }
          if (typeof value.stack === 'string') {
            error.stack = value.stack;
          }
          this.emit('error', error);
        } else if (kind === 'online') {
          this.emit('online');
        } else if (kind === 'exit') {
          state.exited = true;
          state.code = value;
          if (state.resolve) {
            state.resolve(value);
          }
          this.emit('exit', value);
        }
      });
    }

    postMessage(value, transferList) {
      const state = workerState(this);
      if (state.exited) {
        return;
      }
      transfer(transferList);
      status(native.post(state.id, clone.encode(value)));
    }

    ref() {
      const state = workerState(this);
      if (!state.exited) {
        native.ref(state.id, true);
      }
      return this;
    }

    unref() {
      const state = workerState(this);
      if (!state.exited) {
        native.ref(state.id, false);
      }
      return this;
    }

    terminate() {
      const state = workerState(this);
      if (state.exited) {
        return Promise.resolve(state.code);
      }
      // Node keeps termination observable even after explicit unref. Repeat
      // calls re-reference too, while retaining our existing completion promise.
      native.ref(state.id, true);
      if (!state.termination) {
        state.termination = new Promise((resolve) => {
          state.resolve = resolve;
        });
        native.terminate(state.id);
      }
      return state.termination;
    }

    get threadId() {
      const state = workerState(this);
      return state.exited ? -1 : state.id;
    }

    get threadName() {
      const state = workerState(this);
      return state.exited ? null : state.name;
    }
  }
  for (const name of ['stdin', 'stdout', 'stderr', 'performance', 'resourceLimits']) {
    Object.defineProperty(Worker.prototype, name, {
      get() {
        workerState(this);
        return unsupported(`Worker.${name}`);
      }
    });
  }
  const parentPort = native.isMainThread ? null : new MessagePort(secret);
  return {
    Worker,
    MessagePort,
    isMainThread: native.isMainThread,
    isInternalThread: false,
    parentPort,
    workerData: native.isMainThread ? null : undefined,
    threadId: native.threadId,
    threadName: native.isMainThread ? 'main' : native.name
  };
}
