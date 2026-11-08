import namedPromisify from '../core/promisify-results.mjs';
import createReadStream from './read-stream.mjs';

/**
 * Filesystem overloads, descriptor ownership and FileHandle lifetime. Native operations
 * return snapshots; promise/callback adapters normalize errors and close ordering.
 *
 * Native dependencies are supplied by the embedding loader. One factory call
 * constructs the shared builtin instance for one runtime/realm.
 */
export default function createFileSystemModule(host) {
  const native = host.native;
  const Buffer = host.Buffer;
  const constants = native.constants;
  const windows = host.platform === 'win32';
  const toNamespacedPath = host.path.toNamespacedPath;

  /** Apply Windows namespace conversion only after the public path argument is validated. */
  function nativePath(value) {
    if (!windows || typeof value === 'number') {
      return value;
    }
    if (value instanceof Uint8Array) {
      value = Buffer.from(value.buffer, value.byteOffset, value.byteLength).toString('utf8');
    }
    return toNamespacedPath(value);
  }

  function fail(code, message, Type = TypeError) {
    const error = new Type(message);
    error.code = code;
    throw error;
  }

  function unsupported(feature) {
    fail('ERR_MITE_UNSUPPORTED_API', `${feature} is not implemented`);
  }

  /** Accept supported file URL/string/byte paths and reject embedded NUL before native I/O. */
  function path(value) {
    if (value instanceof host.URL) {
      value = host.fileURLToPath(value);
    }
    if (typeof value !== 'string' && !(value instanceof Uint8Array)) {
      fail('ERR_INVALID_ARG_TYPE', 'path must be a string or Buffer');
    }
    if (typeof value === 'string' ? value.includes('\0') : value.includes(0)) {
      fail('ERR_INVALID_ARG_VALUE', 'path must not contain null bytes');
    }
    return value;
  }

  function options(value) {
    if (value === undefined || value === null) {
      return {};
    }
    if (typeof value === 'string') {
      return { encoding: value };
    }
    if (typeof value !== 'object') {
      fail('ERR_INVALID_ARG_TYPE', 'options must be a string or object');
    }
    if (value.signal !== undefined) {
      unsupported('AbortSignal filesystem cancellation');
    }
    if (value.flush) {
      unsupported('Filesystem flush option');
    }
    return value;
  }

  function codec(value, buffers = false) {
    if (!value || (buffers && value === 'buffer')) {
      return value;
    }
    if (typeof value !== 'string') {
      fail('ERR_INVALID_ARG_VALUE', 'Invalid encoding');
    }
    if (value && !Buffer.isEncoding(value)) {
      fail('ERR_INVALID_ARG_VALUE', `Invalid encoding: ${value}`);
    }
    return value;
  }

  function integer(value, minimum, maximum, name) {
    if (typeof value !== 'number') {
      fail('ERR_INVALID_ARG_TYPE', `${name} must be a number`);
    }
    if (!Number.isInteger(value) || value < minimum || value > maximum) {
      fail('ERR_OUT_OF_RANGE', `${name} is out of range`, RangeError);
    }
    return value;
  }

  function mode(value, fallback) {
    if (value == null) {
      return fallback;
    }
    if (typeof value === 'string') {
      if (!/^[0-7]+$/.test(value)) {
        fail('ERR_INVALID_ARG_VALUE', 'Invalid file mode');
      }
      value = parseInt(value, 8);
    }
    return integer(value, 0, 0xffffffff, 'mode');
  }

  function flags(value, fallback) {
    if (value == null) {
      value = fallback;
    }
    if (typeof value === 'number') {
      return integer(value, -0x80000000, 0x7fffffff, 'flags');
    }
    if (typeof value !== 'string') {
      fail('ERR_INVALID_ARG_VALUE', 'flags must be a string or number');
    }
    const c = constants;
    const table = {
      r: c.O_RDONLY,
      'r+': c.O_RDWR,
      w: c.O_WRONLY | c.O_CREAT | c.O_TRUNC,
      wx: c.O_WRONLY | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
      xw: c.O_WRONLY | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
      'w+': c.O_RDWR | c.O_CREAT | c.O_TRUNC,
      'wx+': c.O_RDWR | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
      'xw+': c.O_RDWR | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
      a: c.O_WRONLY | c.O_CREAT | c.O_APPEND,
      ax: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_EXCL,
      xa: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_EXCL,
      'a+': c.O_RDWR | c.O_CREAT | c.O_APPEND,
      'ax+': c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_EXCL,
      'xa+': c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_EXCL,
      rs: c.O_RDONLY | c.O_SYNC,
      sr: c.O_RDONLY | c.O_SYNC,
      'rs+': c.O_RDWR | c.O_SYNC,
      'sr+': c.O_RDWR | c.O_SYNC,
      as: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_SYNC,
      sa: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_SYNC,
      'as+': c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_SYNC,
      'sa+': c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_SYNC
    };
    if (!Object.hasOwn(table, value)) {
      fail('ERR_INVALID_ARG_VALUE', `Invalid file flag: ${value}`);
    }
    return table[value];
  }

  class StatsBase {
    isFile() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFREG;
    }

    isDirectory() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFDIR;
    }

    isSymbolicLink() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFLNK;
    }

    isBlockDevice() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFBLK;
    }

    isCharacterDevice() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFCHR;
    }

    isFIFO() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFIFO;
    }

    isSocket() {
      return (Number(this.mode) & constants.S_IFMT) === constants.S_IFSOCK;
    }
  }
  class Stats extends StatsBase {}
  class BigIntStats extends StatsBase {}

  function stats(raw, bigint) {
    const result = new (bigint ? BigIntStats : Stats)();
    for (const name of [
      'dev',
      'mode',
      'nlink',
      'uid',
      'gid',
      'rdev',
      'blksize',
      'ino',
      'size',
      'blocks'
    ]) {
      result[name] = bigint ? BigInt(raw[name]) : Number(raw[name]);
    }
    for (const name of ['atime', 'mtime', 'ctime', 'birthtime']) {
      const value = raw[name];
      const nanos = BigInt(value.seconds) * 1000000000n + BigInt(value.nanos);
      result[name + 'Ms'] = bigint
        ? nanos / 1000000n
        : Number(value.seconds) * 1000 + value.nanos / 1000000;
      if (bigint) {
        result[name + 'Ns'] = nanos;
      }
      result[name] = new Date(Number(nanos / 1000000n));
    }
    return result;
  }

  const kType = Symbol('type');
  class Dirent {
    constructor(name, parentPath, type) {
      this.name = name;
      this.parentPath = parentPath;
      this[kType] = type;
    }

    isFile() {
      return this[kType] === 1;
    }

    isDirectory() {
      return this[kType] === 2;
    }

    isSymbolicLink() {
      return this[kType] === 3;
    }

    isFIFO() {
      return this[kType] === 4;
    }

    isSocket() {
      return this[kType] === 5;
    }

    isCharacterDevice() {
      return this[kType] === 6;
    }

    isBlockDevice() {
      return this[kType] === 7;
    }
  }

  function prepare(op, pathname, argument, data) {
    const descriptor = ['close', 'fstat', 'ftruncate', 'fsync', 'fdatasync'].includes(op);
    const spec = {
      op,
      path:
        descriptor || ((op === 'readFile' || op === 'writeFile') && typeof pathname === 'number')
          ? integer(pathname, 0, 0x7fffffff, 'fd')
          : path(pathname)
    };
    const parentPath = spec.path;
    spec.path = nativePath(spec.path);
    if (
      windows &&
      op === 'readdir' &&
      /[\\/]$/.test(String(parentPath)) &&
      !spec.path.endsWith('\\')
    ) {
      spec.path += '\\';
    }

    let convert = () => undefined;

    let recover;
    if (op === 'open') {
      spec.flags = flags(argument.flags, 'r');
      spec.mode = mode(argument.mode, 0o666);

      convert = (value) => value;
    } else if (op === 'ftruncate') {
      const length =
        argument === undefined
          ? 0
          : integer(argument, -Number.MAX_SAFE_INTEGER, Number.MAX_SAFE_INTEGER, 'len');
      spec.position = String(Math.max(0, length));
    } else if (op === 'readFile') {
      const opts = options(argument);
      const encoding = codec(opts.encoding);
      spec.flags = flags(opts.flag, 'r');

      convert = (raw) => {
        const buffer = Buffer.from(raw.buffer, raw.byteOffset, raw.byteLength);
        return encoding ? buffer.toString(encoding) : buffer;
      };
    } else if (op === 'writeFile') {
      const opts = options(argument);
      const encoding = codec(opts.encoding) || 'utf8';
      if (typeof data === 'string') {
        spec.data = Buffer.from(data, encoding);
      } else if (ArrayBuffer.isView(data)) {
        spec.data = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
      } else {
        fail('ERR_INVALID_ARG_TYPE', 'data must be a string, Buffer, TypedArray or DataView');
      }
      spec.mode = mode(opts.mode, 0o666);
      spec.flags = flags(opts.flag || 'w', 'w');
    } else if (op === 'access') {
      spec.mode =
        argument === undefined
          ? constants.F_OK
          : integer(argument, -0x80000000, 0x7fffffff, 'mode');
    } else if (op === 'stat' || op === 'lstat' || op === 'fstat') {
      const opts = options(argument);

      convert = (raw) => stats(raw, !!opts.bigint);

      if (opts.throwIfNoEntry === false) {
        recover = (error) => error.code === 'ENOENT' || error.code === 'ENOTDIR';
      }
    } else if (op === 'mkdir') {
      const opts = typeof argument === 'number' ? { mode: argument } : options(argument);
      if (opts.recursive !== undefined && typeof opts.recursive !== 'boolean') {
        fail('ERR_INVALID_ARG_TYPE', 'recursive must be a boolean');
      }
      spec.mode = mode(opts.mode, 0o777);
      spec.recursive = !!opts.recursive;

      convert = (value) => value;
    } else if (op === 'rename') {
      spec.dest = nativePath(path(argument));
    } else if (op === 'rmdir') {
      if (options(argument).recursive) {
        unsupported('Recursive rmdir');
      }
    } else if (op === 'readdir') {
      const opts = options(argument);
      const encoding = codec(opts.encoding === undefined ? 'utf8' : opts.encoding, true);
      if (opts.recursive) {
        unsupported('Recursive readdir');
      }
      spec.withFileTypes = !!opts.withFileTypes;

      convert = (entries) =>
        entries.map((entry) => {
          const buffer = Buffer.from(entry.name);
          const name = encoding === 'buffer' ? buffer : buffer.toString(encoding || 'utf8');
          return opts.withFileTypes ? new Dirent(name, parentPath, entry.type) : name;
        });
    }
    return { spec, convert, recover };
  }

  function sync(op, pathname, argument, data) {
    const { spec, convert, recover } = prepare(op, pathname, argument, data);
    try {
      return convert(native.run(spec));
    } catch (error) {
      if (recover && recover(error)) {
        return undefined;
      }
      throw error;
    }
  }

  function callbackOperation(op, pathname, argument, data, callback) {
    if (typeof callback !== 'function') {
      fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
    }
    const { spec, convert } = prepare(op, pathname, argument, data);

    native.run(spec, (error, value) => {
      if (error) {
        callback(error);
        return;
      }
      // Conversion errors belong to this operation; callback exceptions escape.
      let converted;
      try {
        converted = convert(value);
      } catch (error) {
        callback(error);
        return;
      }
      if (
        op === 'readFile' ||
        op === 'stat' ||
        op === 'lstat' ||
        op === 'readdir' ||
        op === 'mkdir' ||
        op === 'open' ||
        op === 'fstat'
      ) {
        callback(null, converted);
      } else {
        callback(null);
      }
    });
  }

  const fs = { constants, Stats, Dirent };
  for (const op of [
    'readFile',
    'writeFile',
    'access',
    'stat',
    'lstat',
    'mkdir',
    'rmdir',
    'unlink',
    'rename',
    'readdir',
    'fstat',
    'ftruncate',
    'fsync',
    'fdatasync'
  ]) {
    if (op === 'writeFile') {
      fs.writeFileSync = (path, data, opts) => sync(op, path, opts, data);

      fs.writeFile = function (path, data, opts, callback) {
        if (typeof opts === 'function') {
          callback = opts;
          opts = undefined;
        }
        callbackOperation(op, path, opts, data, callback);
      };
    } else {
      fs[op + 'Sync'] = (path, opts) => sync(op, path, opts);

      fs[op] = function (path, opts, callback) {
        if (typeof opts === 'function') {
          callback = opts;
          opts = undefined;
        }
        callbackOperation(op, path, opts, undefined, callback);
      };
    }
  }

  fs.openSync = (path, flag, fileMode) => sync('open', path, { flags: flag, mode: fileMode });

  fs.open = function (path, flag, fileMode, callback) {
    if (typeof flag === 'function') {
      callback = flag;
      flag = undefined;
      fileMode = undefined;
    } else if (typeof fileMode === 'function') {
      callback = fileMode;
      fileMode = undefined;
    }
    callbackOperation('open', path, { flags: flag, mode: fileMode }, undefined, callback);
  };

  fs.closeSync = (fd) => sync('close', fd);

  fs.close = function (
    fd,
    callback = (error) => {
      if (error) {
        throw error;
      }
    }
  ) {
    callbackOperation('close', fd, undefined, undefined, callback);
  };

  function byteView(value) {
    if (!ArrayBuffer.isView(value)) {
      fail('ERR_INVALID_ARG_TYPE', 'buffer must be a Buffer, TypedArray or DataView');
    }
    return new Uint8Array(value.buffer, value.byteOffset, value.byteLength);
  }

  function readPosition(value) {
    if (value == null) {
      return '-1';
    }
    if (typeof value === 'bigint') {
      if (value < -1n || value > 0x7fffffffffffffffn) {
        fail('ERR_OUT_OF_RANGE', 'position is out of range', RangeError);
      }
      return String(value);
    }
    return String(integer(value, -1, Number.MAX_SAFE_INTEGER, 'position'));
  }

  function writePosition(value) {
    // Node's write overloads use the current position for non-number input.
    // Keep safe negative integers distinct: libuv's native interpretation
    // need not be identical to the -1 cursor sentinel on every platform.
    if (typeof value !== 'number') {
      return '-1';
    }
    if (!Number.isInteger(value)) {
      return '-1';
    }
    if (value < -Number.MAX_SAFE_INTEGER) {
      return '-1';
    }
    if (value > Number.MAX_SAFE_INTEGER) {
      unsupported('Write positions above the safe integer range');
    }
    return String(Math.trunc(value));
  }

  function prepareRead(fd, buffer, offset, length, position) {
    integer(fd, 0, 0x7fffffff, 'fd');
    let opts;
    if (!ArrayBuffer.isView(buffer)) {
      opts = buffer == null ? {} : buffer;
      if (typeof opts !== 'object') {
        fail('ERR_INVALID_ARG_TYPE', 'options must be an object');
      }
      buffer = opts.buffer === undefined ? Buffer.alloc(16384) : opts.buffer;
    } else if (typeof offset === 'object') {
      opts = offset || {};
    }
    if (opts) {
      offset = opts.offset;
      length = opts.length;
      position = opts.position;
    }
    const view = byteView(buffer);
    offset = integer(offset === undefined ? 0 : offset, 0, view.byteLength, 'offset');
    length = integer(
      length === undefined ? view.byteLength - offset : length,
      0,
      view.byteLength - offset,
      'length'
    );
    return {
      spec: { op: 'read', path: fd, length, position: readPosition(position) },
      buffer,

      convert(raw) {
        byteView(buffer).set(raw, offset);
        return raw.byteLength;
      }
    };
  }

  function prepareWrite(fd, buffer, offset, length, position) {
    integer(fd, 0, 0x7fffffff, 'fd');
    const original = buffer;
    if (typeof buffer === 'string') {
      const encoding = typeof offset === 'string' ? offset : length;
      position = typeof offset === 'string' ? null : offset;
      buffer = Buffer.from(
        buffer,
        typeof encoding === 'string' && Buffer.isEncoding(encoding) ? encoding : 'utf8'
      );
      offset = 0;
      length = buffer.byteLength;
    } else if (offset !== null && typeof offset === 'object') {
      const opts = offset;
      offset = opts.offset;
      length = opts.length;
      position = opts.position;
    }
    const view = byteView(buffer);
    offset = integer(offset === undefined ? 0 : offset, 0, view.byteLength, 'offset');
    length = integer(
      typeof length !== 'number' ? view.byteLength - offset : length,
      0,
      view.byteLength - offset,
      'length'
    );
    return {
      spec: {
        op: 'write',
        path: fd,
        data: view.subarray(offset, offset + length),
        position: writePosition(position)
      },
      buffer: original
    };
  }

  fs.readSync = function (fd, ...args) {
    byteView(args[0]);
    const operation = prepareRead(fd, ...args);
    return operation.convert(native.run(operation.spec));
  };

  fs.writeSync = function (fd, ...args) {
    return native.run(prepareWrite(fd, ...args).spec);
  };

  fs.read = function (fd, ...args) {
    const callback = args.pop();
    if (typeof callback !== 'function') {
      fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
    }
    const operation = prepareRead(fd, ...args);

    native.run(operation.spec, (error, raw) => {
      if (error) {
        callback(error, 0, operation.buffer);
        return;
      }
      let count;
      try {
        count = operation.convert(raw);
      } catch (error) {
        callback(error, 0, operation.buffer);
        return;
      }
      callback(null, count, operation.buffer);
    });
  };

  fs.write = function (fd, ...args) {
    const callback = args.pop();
    if (typeof callback !== 'function') {
      fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
    }
    const operation = prepareWrite(fd, ...args);

    native.run(operation.spec, (error, count) =>
      callback(error, error ? 0 : count, operation.buffer)
    );
  };

  fs.appendFileSync = function (path, data, opts) {
    opts = options(opts);
    return fs.writeFileSync(path, data, {
      ...opts,
      flag: opts.flag === undefined ? 'a' : opts.flag
    });
  };

  fs.appendFile = function (path, data, opts, callback) {
    if (typeof opts === 'function') {
      callback = opts;
      opts = undefined;
    }
    opts = options(opts);
    return fs.writeFile(
      path,
      data,
      { ...opts, flag: opts.flag === undefined ? 'a' : opts.flag },
      callback
    );
  };

  fs.existsSync = function (path) {
    try {
      fs.accessSync(path);
      return true;
    } catch {
      return false;
    }
  };

  fs.exists = function (path, callback) {
    if (typeof callback !== 'function') {
      fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
    }
    try {
      fs.access(path, (error) => callback(!error));
    } catch {
      queueMicrotask(() => callback(false));
    }
  };

  function call(operation, ...args) {
    return new Promise((resolve, reject) =>
      fs[operation](...args, (error, value) => (error ? reject(error) : resolve(value)))
    );
  }

  const handleToken = Symbol('FileHandle constructor');
  /** Keep descriptors alive through in-flight operations; close waits for outstanding references. */
  class FileHandle extends host.EventEmitter {
    #fd;
    #owner;
    #active = 0;
    #closing = false;
    #closePromise;
    #resolveClose;
    #rejectClose;

    constructor(token, fd) {
      super();
      if (token !== handleToken) {
        fail('ERR_ILLEGAL_CONSTRUCTOR', 'Illegal FileHandle constructor');
      }
      this.#fd = fd;
      this.#owner = native.own(fd);
    }

    get fd() {
      return this.#fd;
    }

    async #run(syscall, operation) {
      if (this.#fd < 0) {
        const error = new Error('file closed');
        error.code = 'EBADF';
        error.syscall = syscall;
        throw error;
      }
      ++this.#active;
      try {
        return await operation(this.#fd);
      } finally {
        --this.#active;
        if (this.#closing && this.#active === 0) {
          this.#startClose();
        }
      }
    }

    #startClose() {
      const fd = this.#owner();
      this.#fd = -1;
      try {
        fs.close(fd, (error) => (error ? this.#rejectClose(error) : this.#resolveClose()));
      } catch (error) {
        // Submission failed before a worker took ownership of the fd.
        try {
          fs.closeSync(fd);
        } catch {}
        this.#rejectClose(error);
      }
    }

    close() {
      if (this.#fd < 0) {
        return Promise.resolve();
      }
      if (this.#closePromise) {
        return this.#closePromise;
      }
      this.#closing = true;
      this.#closePromise = new Promise((resolve, reject) => {
        this.#resolveClose = resolve;
        this.#rejectClose = reject;
      });
      if (this.#active === 0) {
        this.#startClose();
      }
      this.emit('close');
      return this.#closePromise;
    }

    createReadStream(options) {
      // Streams must await a native close already in progress, without changing
      // the public close() Promise identity once fd retirement has begun.
      return createReadStream(this, options, host, () => this.#closePromise ?? this.close());
    }

    read(...args) {
      return this.#run(
        'read',
        (fd) =>
          new Promise((resolve, reject) => {
            fs.read(fd, ...args, (error, bytesRead, buffer) =>
              error ? reject(error) : resolve({ __proto__: null, bytesRead, buffer })
            );
          })
      );
    }

    write(...args) {
      return this.#run(
        'write',
        (fd) =>
          new Promise((resolve, reject) => {
            fs.write(fd, ...args, (error, bytesWritten, buffer) =>
              error ? reject(error) : resolve({ __proto__: null, bytesWritten, buffer })
            );
          })
      );
    }

    stat(opts = {}) {
      return this.#run('fstat', (fd) => {
        const { bigint = false } = opts;
        return call('fstat', fd, { bigint });
      });
    }

    truncate(length = 0) {
      return this.#run('ftruncate', (fd) => call('ftruncate', fd, length));
    }

    sync() {
      return this.#run('fsync', (fd) => call('fsync', fd));
    }

    datasync() {
      return this.#run('fdatasync', (fd) => call('fdatasync', fd));
    }

    readFile(opts) {
      return this.#run('readFile', (fd) => call('readFile', fd, opts));
    }

    writeFile(data, opts) {
      return this.#run('writeFile', (fd) => call('writeFile', fd, data, opts));
    }

    appendFile(data, opts) {
      return this.#run('writeFile', (fd) => call('appendFile', fd, data, opts));
    }

    [Symbol.asyncDispose]() {
      return this.close();
    }
  }
  const promises = { constants };
  for (const op of [
    'readFile',
    'writeFile',
    'appendFile',
    'access',
    'stat',
    'lstat',
    'mkdir',
    'rmdir',
    'unlink',
    'rename',
    'readdir'
  ]) {
    promises[op] = function (...args) {
      return new Promise((resolve, reject) => {
        if (args[0] instanceof FileHandle && ['readFile', 'writeFile', 'appendFile'].includes(op)) {
          const [handle, ...rest] = args;
          resolve(handle[op](...rest));
          return;
        }
        if (typeof args[0] === 'number') {
          fail('ERR_INVALID_ARG_TYPE', 'Promise filesystem APIs require a path or FileHandle');
        }

        fs[op](...args, (error, value) => (error ? reject(error) : resolve(value)));
      });
    };
  }

  promises.open = async function (path, flag, fileMode) {
    return new FileHandle(handleToken, await call('open', path, flag, fileMode));
  };

  namedPromisify(fs.read, ['bytesRead', 'buffer'], host.promisifyState);
  namedPromisify(fs.write, ['bytesWritten', 'buffer'], host.promisifyState);
  fs.promises = promises;
  return fs;
}
