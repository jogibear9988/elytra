/** Dir reads a real native directory stream; async operations are serialized. */
export default function createDirectories(host, { path, nativePath, Dirent, codec }) {
  const Buffer = host.Buffer;
  const states = new WeakMap();

  function error(code, message, Type = Error) {
    const value = new Type(message);
    value.code = code;
    return value;
  }

  function options(value) {
    if (value === undefined || value === null) {
      value = {};
    }
    if (typeof value === 'string') {
      value = { encoding: value };
    }
    if (typeof value !== 'object') {
      throw error('ERR_INVALID_ARG_TYPE', 'Directory options must be an object', TypeError);
    }
    if (value.recursive) {
      throw error('ERR_MITE_UNSUPPORTED_API', 'Recursive opendir is not implemented');
    }
    const encoding = codec(value.encoding === undefined ? 'utf8' : value.encoding, true) || 'utf8';
    const batch = value.bufferSize === undefined ? 32 : value.bufferSize;
    if (typeof batch !== 'number') {
      throw error('ERR_INVALID_ARG_TYPE', 'bufferSize must be a number', TypeError);
    }
    if (!Number.isInteger(batch) || batch < 1 || batch > 4096) {
      throw error('ERR_OUT_OF_RANGE', 'Directory bufferSize must be 1..4096', RangeError);
    }
    return { encoding, batch };
  }

  function state(receiver) {
    if (!states.has(receiver)) {
      throw new TypeError('Invalid Dir receiver');
    }
    return states.get(receiver);
  }

  function entry(record, raw) {
    if (raw === null) {
      return null;
    }
    const bytes = Buffer.from(raw.name.buffer, raw.name.byteOffset, raw.name.byteLength);
    return new Dirent(
      record.encoding === 'buffer' ? bytes : bytes.toString(record.encoding),
      record.path,
      raw.type
    );
  }

  function enqueue(record, op, callback) {
    if (callback !== undefined && typeof callback !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'Callback must be a function', TypeError);
    }
    if (callback === undefined) {
      return new Promise((resolve, reject) =>
        enqueue(record, op, (error, value) => (error ? reject(error) : resolve(value)))
      );
    }
    if (record.queue.length >= 256) {
      throw error('ERR_MITE_FS_LIMIT', 'Directory operation queue is full');
    }
    record.queue.push({ op, callback });
    pump(record);
  }

  function pump(record) {
    if (record.running || !record.queue.length) {
      return;
    }
    const item = record.queue.shift();
    record.running = true;

    const complete = (failure, raw) => {
      record.running = false;
      if (!failure && item.op === 'close') {
        record.closed = true;
      }
      let value;
      if (!failure && item.op === 'read') {
        try {
          value = entry(record, raw);
        } catch (error) {
          failure = error;
        }
      }
      try {
        item.callback(failure, value);
      } finally {
        pump(record);
      }
    };

    try {
      record.handle[item.op](complete);
    } catch (error) {
      process.nextTick(() => complete(error));
    }
  }

  class Dir {
    constructor(token, handle, pathname, encoding) {
      if (token !== states) {
        throw error('ERR_MITE_UNSUPPORTED_API', 'Dir requires fs.opendir');
      }
      states.set(this, {
        handle,
        path: pathname,
        encoding,
        queue: [],
        running: false,
        closed: false
      });
    }

    get path() {
      return state(this).path;
    }

    read(callback) {
      return enqueue(state(this), 'read', callback);
    }

    close(callback) {
      return enqueue(state(this), 'close', callback);
    }

    readSync() {
      const record = state(this);
      if (record.running || record.queue.length) {
        throw error('ERR_DIR_CONCURRENT_OPERATION', 'Directory operation is pending');
      }
      return entry(record, record.handle.read());
    }

    closeSync() {
      const record = state(this);
      if (record.running || record.queue.length) {
        throw error('ERR_DIR_CONCURRENT_OPERATION', 'Directory operation is pending');
      }
      record.handle.close();
      record.closed = true;
    }

    async *[Symbol.asyncIterator]() {
      const record = state(this);
      try {
        for (;;) {
          const value = await this.read();
          if (value === null) {
            break;
          }
          yield value;
        }
      } finally {
        if (!record.closed) {
          await this.close();
        }
      }
    }
  }
  if (Symbol.asyncDispose) {
    Object.defineProperty(Dir.prototype, Symbol.asyncDispose, {
      value() {
        return this.close();
      }
    });
  }

  function opendir(pathname, opts, callback) {
    if (typeof opts === 'function') {
      callback = opts;
      opts = undefined;
    }
    if (typeof callback !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'Callback must be a function', TypeError);
    }
    const name = path(pathname);
    const { encoding, batch } = options(opts);

    host.native.openDirectory(nativePath(name), batch, (error, handle) =>
      callback(error, error ? undefined : new Dir(states, handle, name, encoding))
    );
  }

  function opendirSync(pathname, opts) {
    const name = path(pathname);
    const { encoding, batch } = options(opts);
    return new Dir(states, host.native.openDirectory(nativePath(name), batch), name, encoding);
  }

  return { Dir, opendir, opendirSync };
}
