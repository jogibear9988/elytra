/** Bounded recursive copying through genuine filesystem operations and directory streams. */
export default function createCopyTree(fs, host, validatePath) {
  const path = host.path;
  const p = fs.promises;

  function fail(code, message, Type = Error) {
    const error = new Type(message);
    error.code = code;
    throw error;
  }

  function name(value) {
    value = validatePath(value);
    // Recursive traversal joins textual names. Refuse invalid byte paths instead
    // of decoding replacement characters and copying a different native path.
    if (typeof value !== 'string') {
      const bytes = host.Buffer.from(value);
      value = bytes.toString('utf8');
      if (!host.Buffer.from(value).equals(bytes)) {
        fail('ERR_MITE_UNSUPPORTED_API', 'Non-UTF8 recursive copy paths are not implemented');
      }
    }
    return path.resolve(value);
  }

  async function maybeStat(value) {
    try {
      return await p.lstat(value);
    } catch (error) {
      if (error.code === 'ENOENT') {
        return undefined;
      }
      throw error;
    }
  }

  async function canonicalDestination(value) {
    const suffix = [];
    for (;;) {
      try {
        return path.resolve(await p.realpath(value), ...suffix);
      } catch (error) {
        if (error.code !== 'ENOENT') {
          throw error;
        }
        const parent = path.dirname(value);
        if (parent === value) {
          throw error;
        }
        suffix.unshift(path.basename(value));
        value = parent;
      }
    }
  }

  function options(value) {
    if (value === undefined || value === null) {
      value = {};
    }
    if (typeof value !== 'object') {
      fail('ERR_INVALID_ARG_TYPE', 'cp options must be an object', TypeError);
    }
    const result = {
      recursive: false,
      force: true,
      errorOnExist: false,
      dereference: false,
      preserveTimestamps: false,
      verbatimSymlinks: false,
      mode: 0
    };
    for (const key of Object.keys(value)) {
      if (!Object.hasOwn(result, key) && key !== 'filter') {
        fail('ERR_MITE_UNSUPPORTED_API', `cp option ${key} is not implemented`);
      }
      if (key === 'filter') {
        if (typeof value[key] !== 'function') {
          fail('ERR_INVALID_ARG_TYPE', 'cp filter must be a function', TypeError);
        }
      } else if (key === 'mode') {
        if (typeof value.mode !== 'number') {
          fail('ERR_INVALID_ARG_TYPE', 'cp mode must be a number', TypeError);
        }
        if (!Number.isInteger(value.mode) || value.mode < 0 || value.mode > 7) {
          fail('ERR_OUT_OF_RANGE', 'Invalid cp mode', RangeError);
        }
      } else if (typeof value[key] !== 'boolean') {
        fail('ERR_INVALID_ARG_TYPE', `cp ${key} must be a boolean`, TypeError);
      }
      result[key] = value[key];
    }
    return result;
  }

  async function copy(source, destination, opts) {
    const from = name(source);
    const to = name(destination);
    const settings = options(opts);
    const active = new Set();
    let visited = 0;

    async function walk(src, dest, depth) {
      if (depth > 128 || ++visited > 100000) {
        fail('ERR_MITE_FS_LIMIT', 'Recursive copy traversal budget exceeded');
      }
      if (settings.filter && !(await settings.filter(src, dest))) {
        return;
      }
      const stat = await (settings.dereference ? p.stat(src) : p.lstat(src));
      const existing = await maybeStat(dest);
      if (existing && existing.dev === stat.dev && existing.ino === stat.ino) {
        fail('ERR_FS_CP_EINVAL', 'Source and destination are the same file');
      }
      if (stat.isDirectory()) {
        if (!settings.recursive) {
          fail('ERR_FS_EISDIR', 'Directory copying requires recursive:true');
        }
        if (existing && !existing.isDirectory()) {
          fail('ERR_FS_CP_DIR_TO_NON_DIR', 'Cannot copy directory onto non-directory');
        }
        const identity = `${stat.dev}:${stat.ino}`;
        if (active.has(identity)) {
          fail('ELOOP', 'Recursive copy encountered a directory cycle');
        }
        const canonical = await p.realpath(src);
        const target = await canonicalDestination(dest);
        const relative = path.relative(canonical, target);
        if (
          relative === '' ||
          (!relative.startsWith(`..${path.sep}`) && relative !== '..' && !path.isAbsolute(relative))
        ) {
          fail('ERR_FS_CP_EINVAL', 'Cannot copy a directory into itself');
        }
        active.add(identity);
        try {
          if (!existing) {
            await p.mkdir(dest, { recursive: true, mode: Number(stat.mode) & 0o777 });
          }
          const dir = await p.opendir(src, { encoding: 'buffer' });
          for await (const entry of dir) {
            const child = entry.name.toString('utf8');
            if (!host.Buffer.from(child).equals(entry.name)) {
              fail(
                'ERR_MITE_UNSUPPORTED_API',
                'Non-UTF8 recursive directory names are not implemented'
              );
            }
            await walk(path.join(src, child), path.join(dest, child), depth + 1);
          }
        } finally {
          active.delete(identity);
        }
      } else {
        if (existing) {
          if (existing.isDirectory()) {
            fail('ERR_FS_CP_NON_DIR_TO_DIR', 'Cannot copy non-directory onto directory');
          }
          if (!settings.force) {
            if (settings.errorOnExist) {
              fail('EEXIST', 'Copy destination exists');
            }
            return;
          }
        }
        await p.mkdir(path.dirname(dest), { recursive: true });
        if (stat.isSymbolicLink() && !settings.dereference) {
          const rawTarget = await p.readlink(src, { encoding: 'buffer' });
          let target = rawTarget.toString('utf8');
          if (!host.Buffer.from(target).equals(rawTarget)) {
            fail('ERR_MITE_UNSUPPORTED_API', 'Non-UTF8 symbolic link targets are not implemented');
          }
          if (!settings.verbatimSymlinks) {
            target = path.resolve(path.dirname(src), target);
          }
          if (existing) {
            await p.unlink(dest);
          }
          let type = 'file';
          try {
            if ((await p.stat(src)).isDirectory()) {
              type = 'dir';
            }
          } catch (error) {
            if (error.code !== 'ENOENT') {
              throw error;
            }
          }
          await p.symlink(target, dest, type);
        } else if (stat.isFile()) {
          if (existing?.isSymbolicLink()) {
            await p.unlink(dest);
          }
          try {
            await p.copyFile(
              src,
              dest,
              settings.mode | (!settings.force ? fs.constants.COPYFILE_EXCL : 0)
            );
          } catch (error) {
            if (!(error.code === 'EEXIST' && !settings.force && !settings.errorOnExist)) {
              throw error;
            }
          }
          if (settings.preserveTimestamps) {
            await p.utimes(dest, stat.atime, stat.mtime);
          }
        } else {
          fail('ERR_MITE_UNSUPPORTED_API', 'Copying special filesystem objects is not implemented');
        }
      }
    }

    await walk(from, to, 0);
  }

  function cp(src, dest, opts, callback) {
    if (typeof opts === 'function') {
      callback = opts;
      opts = undefined;
    }
    if (typeof callback !== 'function') {
      fail('ERR_INVALID_ARG_TYPE', 'cp callback must be a function', TypeError);
    }

    copy(src, dest, opts).then(() => callback(null), callback);
  }

  return { cp, promiseCopy: copy };
}
