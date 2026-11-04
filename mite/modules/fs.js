(function (host) {
    'use strict';
    const native = host.native;
    const Buffer = host.Buffer;
    const constants = native.constants;
    function fail(code, message, Type = TypeError) { const error = new Type(message); error.code = code; throw error; }
    function unsupported(feature) { fail('ERR_MITE_UNSUPPORTED_API', `${feature} is not implemented`); }
    function path(value) {
        if (typeof value !== 'string' && !(value instanceof Uint8Array)) fail('ERR_INVALID_ARG_TYPE', 'path must be a string or Buffer');
        if (typeof value === 'string' ? value.includes('\0') : value.includes(0)) fail('ERR_INVALID_ARG_VALUE', 'path must not contain null bytes');
        return value;
    }
    function options(value) {
        if (value === undefined || value === null) return {};
        if (typeof value === 'string') return { encoding: value };
        if (typeof value !== 'object') fail('ERR_INVALID_ARG_TYPE', 'options must be a string or object');
        if (value.signal !== undefined) unsupported('AbortSignal filesystem cancellation');
        if (value.flush) unsupported('Filesystem flush option');
        return value;
    }
    function codec(value, buffers = false) {
        if (!value || (buffers && value === 'buffer')) return value;
        if (typeof value !== 'string') fail('ERR_INVALID_ARG_VALUE', 'Invalid encoding');
        if (value && !Buffer.isEncoding(value)) fail('ERR_INVALID_ARG_VALUE', `Invalid encoding: ${value}`);
        return value;
    }
    function integer(value, minimum, maximum, name) {
        if (typeof value !== 'number') fail('ERR_INVALID_ARG_TYPE', `${name} must be a number`);
        if (!Number.isInteger(value) || value < minimum || value > maximum) fail('ERR_OUT_OF_RANGE', `${name} is out of range`, RangeError);
        return value;
    }
    function mode(value, fallback) {
        if (value == null) return fallback;
        if (typeof value === 'string') {
            if (!/^[0-7]+$/.test(value)) fail('ERR_INVALID_ARG_VALUE', 'Invalid file mode');
            value = parseInt(value, 8);
        }
        return integer(value, 0, 0xffffffff, 'mode');
    }
    function flags(value, fallback) {
        if (value == null) value = fallback;
        if (typeof value === 'number') return integer(value, -0x80000000, 0x7fffffff, 'flags');
        if (typeof value !== 'string') fail('ERR_INVALID_ARG_VALUE', 'flags must be a string or number');
        const c = constants;
        const table = {
            r: c.O_RDONLY, 'r+': c.O_RDWR,
            w: c.O_WRONLY | c.O_CREAT | c.O_TRUNC, wx: c.O_WRONLY | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
            xw: c.O_WRONLY | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
            'w+': c.O_RDWR | c.O_CREAT | c.O_TRUNC, 'wx+': c.O_RDWR | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
            'xw+': c.O_RDWR | c.O_CREAT | c.O_TRUNC | c.O_EXCL,
            a: c.O_WRONLY | c.O_CREAT | c.O_APPEND, ax: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_EXCL,
            xa: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_EXCL,
            'a+': c.O_RDWR | c.O_CREAT | c.O_APPEND, 'ax+': c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_EXCL,
            'xa+': c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_EXCL,
            rs: c.O_RDONLY | c.O_SYNC, sr: c.O_RDONLY | c.O_SYNC,
            'rs+': c.O_RDWR | c.O_SYNC, 'sr+': c.O_RDWR | c.O_SYNC,
            as: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_SYNC, sa: c.O_WRONLY | c.O_CREAT | c.O_APPEND | c.O_SYNC,
            'as+': c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_SYNC, 'sa+': c.O_RDWR | c.O_CREAT | c.O_APPEND | c.O_SYNC
        };
        if (!Object.hasOwn(table, value)) fail('ERR_INVALID_ARG_VALUE', `Invalid file flag: ${value}`);
        return table[value];
    }
    class StatsBase {
        isFile() { return (Number(this.mode) & constants.S_IFMT) === constants.S_IFREG; }
        isDirectory() { return (Number(this.mode) & constants.S_IFMT) === constants.S_IFDIR; }
        isSymbolicLink() { return (Number(this.mode) & constants.S_IFMT) === constants.S_IFLNK; }
        isBlockDevice() { return (Number(this.mode) & constants.S_IFMT) === constants.S_IFBLK; }
        isCharacterDevice() { return (Number(this.mode) & constants.S_IFMT) === constants.S_IFCHR; }
        isFIFO() { return (Number(this.mode) & constants.S_IFMT) === constants.S_IFIFO; }
        isSocket() { return (Number(this.mode) & constants.S_IFMT) === constants.S_IFSOCK; }
    }
    class Stats extends StatsBase {}
    class BigIntStats extends StatsBase {}
    function stats(raw, bigint) {
        const result = new (bigint ? BigIntStats : Stats)();
        for (const name of ['dev','mode','nlink','uid','gid','rdev','blksize','ino','size','blocks']) result[name] = bigint ? BigInt(raw[name]) : Number(raw[name]);
        for (const name of ['atime','mtime','ctime','birthtime']) {
            const value = raw[name];
            const nanos = BigInt(value.seconds) * 1000000000n + BigInt(value.nanos);
            result[name + 'Ms'] = bigint ? nanos / 1000000n : Number(value.seconds) * 1000 + value.nanos / 1000000;
            if (bigint) result[name + 'Ns'] = nanos;
            result[name] = new Date(Number(nanos / 1000000n));
        }
        return result;
    }
    const kType = Symbol('type');
    class Dirent {
        constructor(name, parentPath, type) { this.name = name; this.parentPath = parentPath; this[kType] = type; }
        isFile() { return this[kType] === 1; }
        isDirectory() { return this[kType] === 2; }
        isSymbolicLink() { return this[kType] === 3; }
        isFIFO() { return this[kType] === 4; }
        isSocket() { return this[kType] === 5; }
        isCharacterDevice() { return this[kType] === 6; }
        isBlockDevice() { return this[kType] === 7; }
    }
    function prepare(op, pathname, argument, data) {
        const spec = { op, path: (op === 'readFile' || op === 'writeFile') && typeof pathname === 'number' ? integer(pathname, 0, 0x7fffffff, 'fd') : path(pathname) };
        let convert = () => undefined;
        let recover;
        if (op === 'readFile') {
            const opts = options(argument); const encoding = codec(opts.encoding);
            spec.flags = flags(opts.flag, 'r');
            convert = raw => { const buffer = Buffer.from(raw.buffer, raw.byteOffset, raw.byteLength); return encoding ? buffer.toString(encoding) : buffer; };
        } else if (op === 'writeFile') {
            const opts = options(argument); const encoding = codec(opts.encoding) || 'utf8';
            if (typeof data === 'string') spec.data = Buffer.from(data, encoding);
            else if (ArrayBuffer.isView(data)) spec.data = new Uint8Array(data.buffer, data.byteOffset, data.byteLength);
            else fail('ERR_INVALID_ARG_TYPE', 'data must be a string, Buffer, TypedArray or DataView');
            spec.mode = mode(opts.mode, 0o666); spec.flags = flags(opts.flag || 'w', 'w');
        } else if (op === 'access') spec.mode = argument === undefined ? constants.F_OK : integer(argument, -0x80000000, 0x7fffffff, 'mode');
        else if (op === 'stat' || op === 'lstat') {
            const opts = options(argument);
            convert = raw => stats(raw, !!opts.bigint);
            if (opts.throwIfNoEntry === false) recover = error => error.code === 'ENOENT' || error.code === 'ENOTDIR';
        } else if (op === 'mkdir') {
            const opts = typeof argument === 'number' ? { mode: argument } : options(argument);
            if (opts.recursive !== undefined && typeof opts.recursive !== 'boolean') fail('ERR_INVALID_ARG_TYPE', 'recursive must be a boolean');
            spec.mode = mode(opts.mode, 0o777); spec.recursive = !!opts.recursive;
            convert = value => value;
        } else if (op === 'rename') spec.dest = path(argument);
        else if (op === 'rmdir') { if (options(argument).recursive) unsupported('Recursive rmdir'); }
        else if (op === 'readdir') {
            const opts = options(argument); const encoding = codec(opts.encoding === undefined ? 'utf8' : opts.encoding, true);
            if (opts.recursive) unsupported('Recursive readdir');
            spec.withFileTypes = !!opts.withFileTypes;
            convert = entries => entries.map(entry => {
                const buffer = Buffer.from(entry.name);
                const name = encoding === 'buffer' ? buffer : buffer.toString(encoding || 'utf8');
                return opts.withFileTypes ? new Dirent(name, pathname, entry.type) : name;
            });
        }
        return { spec, convert, recover };
    }
    function sync(op, pathname, argument, data) {
        const {spec, convert, recover} = prepare(op, pathname, argument, data);
        try { return convert(native.run(spec)); }
        catch (error) { if (recover && recover(error)) return undefined; throw error; }
    }
    function callbackOperation(op, pathname, argument, data, callback) {
        if (typeof callback !== 'function') fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
        const {spec, convert} = prepare(op, pathname, argument, data);
        native.run(spec, (error, value) => {
            if (error) { callback(error); return; }
            // Conversion errors belong to this operation; callback exceptions escape.
            let converted;
            try { converted = convert(value); } catch (error) { callback(error); return; }
            if (op === 'readFile' || op === 'stat' || op === 'lstat' || op === 'readdir' || op === 'mkdir') callback(null, converted);
            else callback(null);
        });
    }
    const fs = { constants, Stats, Dirent };
    for (const op of ['readFile','writeFile','access','stat','lstat','mkdir','rmdir','unlink','rename','readdir']) {
        if (op === 'writeFile') {
            fs.writeFileSync = (path, data, opts) => sync(op, path, opts, data);
            fs.writeFile = function (path, data, opts, callback) {
                if (typeof opts === 'function') { callback = opts; opts = undefined; }
                callbackOperation(op, path, opts, data, callback);
            };
        } else {
            fs[op + 'Sync'] = (path, opts) => sync(op, path, opts);
            fs[op] = function (path, opts, callback) {
                if (typeof opts === 'function') { callback = opts; opts = undefined; }
                callbackOperation(op, path, opts, undefined, callback);
            };
        }
    }
    fs.appendFileSync = function (path, data, opts) { opts = options(opts); return fs.writeFileSync(path, data, { ...opts, flag: opts.flag === undefined ? 'a' : opts.flag }); };
    fs.appendFile = function (path, data, opts, callback) {
        if (typeof opts === 'function') { callback = opts; opts = undefined; }
        opts = options(opts); return fs.writeFile(path, data, { ...opts, flag: opts.flag === undefined ? 'a' : opts.flag }, callback);
    };
    fs.existsSync = function (path) { try { fs.accessSync(path); return true; } catch { return false; } };
    fs.exists = function (path, callback) {
        if (typeof callback !== 'function') fail('ERR_INVALID_ARG_TYPE', 'callback must be a function');
        try { fs.access(path, error => callback(!error)); }
        catch { queueMicrotask(() => callback(false)); }
    };
    const promises = { constants };
    for (const op of ['readFile','writeFile','appendFile','access','stat','lstat','mkdir','rmdir','unlink','rename','readdir'])
        promises[op] = function (...args) {
            return new Promise((resolve, reject) => {
                if (typeof args[0] === 'number') fail('ERR_INVALID_ARG_TYPE', 'Promise filesystem APIs require a path or FileHandle');
                fs[op](...args, (error, value) => error ? reject(error) : resolve(value));
            });
        };
    fs.promises = promises;
    return fs;
})
