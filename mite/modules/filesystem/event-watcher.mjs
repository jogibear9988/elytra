/** Individually owned filesystem event handles, filename snapshots and private abort cleanup. */
export default function createEventWatchers(host, helpers) {
  const { native, Buffer, EventEmitter, abort } = host;
  const { path, nativePath } = helpers;
  const tick = host.process.nextTick;
  const states = new WeakMap();
  const token = {};

  function error(code, message, Type = TypeError) {
    return Object.assign(new Type(message), { code });
  }

  function state(value) {
    const current = states.get(value);
    if (!current) {
      throw error('ERR_INVALID_ARG_TYPE', 'Illegal FSWatcher receiver');
    }
    return current;
  }

  function dispose(watcher, notify) {
    const current = state(watcher);
    if (current.closed) {
      return;
    }
    current.closed = true;
    current.early.length = 0;
    current.bytes = 0;
    const unsubscribe = current.unsubscribe;
    current.unsubscribe = null;
    unsubscribe?.();
    const id = current.id;
    current.id = null;
    if (id !== null) {
      native.fsWatchStop(id);
    }
    if (notify) {
      tick(() => watcher.emit('close'));
    }
  }

  class FSWatcher extends EventEmitter {
    constructor(key, name, encoding) {
      super();
      if (key !== token) {
        throw error('ERR_ILLEGAL_CONSTRUCTOR', 'FSWatcher cannot be constructed directly');
      }
      states.set(this, {
        name,
        encoding,
        id: null,
        closed: false,
        admitted: false,
        early: [],
        bytes: 0,
        unsubscribe: null
      });
    }

    close() {
      dispose(this, true);
    }

    ref() {
      const current = state(this);
      if (current.id !== null) {
        native.fsWatchRef(current.id, true);
      }
      return this;
    }

    unref() {
      const current = state(this);
      if (current.id !== null) {
        native.fsWatchRef(current.id, false);
      }
      return this;
    }

    [Symbol.dispose]() {
      this.close();
    }
  }

  function deliver(watcher, record) {
    const current = state(watcher);
    if (current.closed) {
      return;
    }
    if (record.status < 0) {
      const failure = native.fsWatchError(record.status, current.name);
      // Error termination revokes the handle without the ordinary close event, as Node does.
      dispose(watcher, false);
      watcher.emit('error', failure);
    } else {
      watcher.emit('change', record.mask & 1 ? 'rename' : 'change', record.filename);
    }
  }

  function receive(watcher, status, mask, value) {
    const current = state(watcher);
    if (current.closed) {
      return;
    }
    if (
      !Number.isInteger(status) ||
      status < -2147483648 ||
      status > 2147483647 ||
      !Number.isInteger(mask) ||
      mask < 0 ||
      mask > 3
    ) {
      dispose(watcher, false);
      throw error('ERR_MITE_FS_WATCH_DATA', 'Invalid native filesystem event status', Error);
    }
    let filename = null;
    if (value !== null) {
      if (!(value instanceof Uint8Array) || value.byteLength > 1048576) {
        dispose(watcher, false);
        throw error('ERR_MITE_FS_WATCH_DATA', 'Invalid native event filename', Error);
      }
      const bytes = Buffer.from(value);
      filename = current.encoding === 'buffer' ? bytes : bytes.toString(current.encoding);
    }
    const record = { status, mask, filename };
    if (!current.admitted) {
      const bytes = value?.byteLength ?? 0;
      if (current.early.length >= 64 || current.bytes + bytes > 4194304) {
        dispose(watcher, false);
        throw error('ERR_MITE_FS_WATCH_LIMIT', 'Filesystem event admission queue exhausted', Error);
      }
      current.bytes += bytes;
      current.early.push(record);
    } else {
      deliver(watcher, record);
    }
  }

  function schedule(watcher) {
    tick(() => {
      const current = state(watcher);
      while (!current.closed && current.early.length) {
        deliver(watcher, current.early.shift());
      }
      current.bytes = 0;
    });
  }

  function attachAbort(watcher, signal) {
    const current = state(watcher);
    let attaching = true;
    const unsubscribe = abort.subscribe(signal, () => {
      if (attaching) {
        // A pre-aborted signal still allows native startup errors to surface first.
        tick(() => watcher.close());
      } else {
        watcher.close();
      }
    });
    attaching = false;
    if (current.closed) {
      unsubscribe();
    } else {
      current.unsubscribe = unsubscribe;
    }
  }

  function watch(filename, options, listener) {
    if (typeof options === 'function') {
      listener = options;
      options = undefined;
    }
    if (typeof options === 'string') {
      options = { encoding: options };
    } else if (options === undefined || options === null) {
      options = {};
    } else if (typeof options !== 'object') {
      throw error('ERR_INVALID_ARG_TYPE', 'Filesystem watch options must be an object or string');
    }
    const selected = { ...options };
    const persistent = selected.persistent === undefined ? true : !!selected.persistent;
    const recursive = selected.recursive === undefined ? false : !!selected.recursive;
    const encoding = selected.encoding || 'utf8';
    const signal = selected.signal;
    if (encoding !== 'buffer' && (typeof encoding !== 'string' || !Buffer.isEncoding(encoding))) {
      throw error('ERR_INVALID_ARG_VALUE', 'Invalid filesystem watch encoding');
    }
    if (listener && typeof listener !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'Filesystem watch listener must be a function');
    }
    if (signal !== undefined && (!abort || !abort.isSignal(signal))) {
      throw error('ERR_INVALID_ARG_TYPE', 'Filesystem watch signal must be an AbortSignal');
    }
    if (selected.ignore !== undefined && selected.ignore !== null) {
      throw error(
        'ERR_MITE_UNSUPPORTED_API',
        'Filesystem watch ignore filters are not implemented',
        Error
      );
    }
    if (recursive && host.platform !== 'darwin' && host.platform !== 'win32') {
      throw error(
        'ERR_MITE_UNSUPPORTED_API',
        'Recursive filesystem events are unavailable on this platform',
        Error
      );
    }
    const name = path(filename);
    // Snapshot byte paths before the application can mutate its original Buffer.
    const snapshot = typeof name === 'string' ? name : Buffer.from(name);
    const watcher = new FSWatcher(token, snapshot, encoding);
    const current = state(watcher);
    try {
      const result = native.fsWatchStart(
        nativePath(snapshot),
        persistent,
        recursive,
        (status, mask, value) => receive(watcher, status, mask, value)
      );
      if (result.status) {
        throw native.fsWatchError(result.status, snapshot);
      }
      if (current.closed) {
        native.fsWatchStop(result.id);
        throw error(
          'ERR_MITE_FS_WATCH_CLOSED',
          'Filesystem watcher stopped during admission',
          Error
        );
      }
      current.id = result.id;
      if (listener) {
        watcher.on('change', listener);
      }
      if (current.closed) {
        throw error(
          'ERR_MITE_FS_WATCH_CLOSED',
          'Filesystem watcher closed while adding listener',
          Error
        );
      }
      if (signal !== undefined) {
        attachAbort(watcher, signal);
      }
      current.admitted = true;
      if (current.early.length) {
        schedule(watcher);
      }
      return watcher;
    } catch (failure) {
      dispose(watcher, false);
      throw failure;
    }
  }

  return { watch };
}
