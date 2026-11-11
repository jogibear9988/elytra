/** Stream-backed Console basics; bounded formatting is deliberately smaller than util.inspect. */
export default function createConsoleModule(host) {
  const states = new WeakMap();
  const apply = Reflect.apply;
  const descriptors = Object.getOwnPropertyDescriptors;
  const keys = Object.keys;
  const methods = ['log', 'info', 'debug', 'warn', 'error', 'dir', 'assert'];

  function failure(Type, code, message) {
    return Object.assign(new Type(message), { code });
  }

  function unsupported(name) {
    throw failure(Error, 'ERR_MITE_UNSUPPORTED_API', 'Console.' + name + ' is not implemented');
  }

  function state(value) {
    const found = states.get(value);
    if (!found) {
      throw failure(TypeError, 'ERR_INVALID_THIS', 'Invalid Console receiver');
    }
    return found;
  }

  function inspect(value, depth = 2, seen = new Set()) {
    if (typeof value === 'string') {
      return JSON.stringify(value);
    }
    if (typeof value === 'bigint') {
      return String(value) + 'n';
    }
    if (typeof value === 'function') {
      return '[Function]';
    }
    if (value === null || typeof value !== 'object') {
      return String(value);
    }
    if (host.types?.isProxy(value)) {
      unsupported('Proxy inspection');
    }
    if (seen.has(value)) {
      return '[Circular]';
    }
    if (depth < 0) {
      return Array.isArray(value) ? '[Array]' : '[Object]';
    }
    seen.add(value);
    try {
      const entries = descriptors(value);
      const names = keys(entries).filter((name) => entries[name].enumerable);
      if (names.length > 100) {
        throw failure(
          RangeError,
          'ERR_MITE_CONSOLE_LIMIT',
          'Console object exceeds 100 enumerable properties'
        );
      }
      const values = names.map((name) => {
        const entry = entries[name];
        const rendered =
          'value' in entry ? inspect(entry.value, depth - 1, seen) : '[Getter/Setter]';
        return Array.isArray(value) ? rendered : name + ': ' + rendered;
      });
      return Array.isArray(value)
        ? '[ ' + values.join(', ') + ' ]'
        : '{ ' + values.join(', ') + ' }';
    } finally {
      seen.delete(value);
    }
  }

  function format(args) {
    if (!args.length) {
      return '';
    }
    if (typeof args[0] !== 'string') {
      return args.map((value) => (typeof value === 'string' ? value : inspect(value))).join(' ');
    }
    let index = 1;
    const head = args[0].replace(/%[%sdifjoO]/g, (token) => {
      if (token === '%%') {
        return '%';
      }
      if (index >= args.length) {
        return token;
      }
      const value = args[index++];
      if (token === '%s') {
        return typeof value === 'object' && value !== null ? inspect(value) : String(value);
      }
      if (token === '%d') {
        return typeof value === 'bigint' ? String(value) + 'n' : String(Number(value));
      }
      if (token === '%i') {
        return String(parseInt(value, 10));
      }
      if (token === '%f') {
        return String(parseFloat(value));
      }
      if (token === '%j') {
        return JSON.stringify(value);
      }
      return inspect(value);
    });
    return [
      head,
      ...args.slice(index).map((value) => (typeof value === 'string' ? value : inspect(value)))
    ].join(' ');
  }

  function write(receiver, error, text) {
    const record = state(receiver);
    if (text.length > 1024 * 1024) {
      throw failure(RangeError, 'ERR_MITE_CONSOLE_LIMIT', 'Console output exceeds 1 MiB');
    }
    const stream = error ? record.stderr : record.stdout;
    if (record.ignoreErrors) {
      const ignore = () => {};

      let listening = false;
      try {
        if (typeof stream.once === 'function') {
          stream.once('error', ignore);
          listening = true;
        }
        apply(stream.write, stream, [
          text + '\n',
          (failure) => {
            // Writable calls back before its asynchronous error event. A success
            // retains no listener, even if a custom stream omits its callback.
            if (
              failure &&
              typeof stream.once === 'function' &&
              (typeof stream.listenerCount !== 'function' || stream.listenerCount('error') === 0)
            ) {
              stream.once('error', ignore);
            }
          }
        ]);
      } catch {
        // The documented default ignores actual stream write failures only.
      } finally {
        if (listening && typeof stream.removeListener === 'function') {
          stream.removeListener('error', ignore);
        }
      }
    } else {
      apply(stream.write, stream, [text + '\n']);
    }
  }

  function Console(stdout, stderr, ignoreErrors = true) {
    if (!new.target) {
      return new Console(stdout, stderr, ignoreErrors);
    }
    if (stdout && typeof stdout === 'object' && typeof stdout.write !== 'function') {
      const options = stdout;
      for (const name of Reflect.ownKeys(options)) {
        if (!['stdout', 'stderr', 'ignoreErrors', 'inspectOptions', 'colorMode'].includes(name)) {
          unsupported('option ' + String(name));
        }
      }
      stdout = options.stdout;
      stderr = options.stderr;
      ignoreErrors = options.ignoreErrors ?? true;
      const inspectOptions = options.inspectOptions;
      if (inspectOptions !== undefined) {
        if (
          !inspectOptions ||
          typeof inspectOptions !== 'object' ||
          Array.isArray(inspectOptions)
        ) {
          throw failure(TypeError, 'ERR_INVALID_ARG_TYPE', 'inspectOptions must be an object');
        }
        for (const name of Reflect.ownKeys(inspectOptions)) {
          if (name !== 'colors' || inspectOptions[name] !== false) {
            unsupported('inspectOptions');
          }
        }
      }
      if (options.colorMode !== undefined && options.colorMode !== false) {
        unsupported('colorMode');
      }
    }
    if (
      !stdout ||
      typeof stdout.write !== 'function' ||
      (stderr !== undefined && (!stderr || typeof stderr.write !== 'function'))
    ) {
      throw failure(
        TypeError,
        'ERR_CONSOLE_WRITABLE_STREAM',
        'Console requires writable stdout/stderr'
      );
    }
    if (typeof ignoreErrors !== 'boolean') {
      throw failure(TypeError, 'ERR_INVALID_ARG_TYPE', 'ignoreErrors must be boolean');
    }
    states.set(this, { stdout, stderr: stderr ?? stdout, ignoreErrors });
    for (const method of methods) {
      this[method] = Console.prototype[method].bind(this);
    }
  }

  for (const name of ['log', 'info', 'debug']) {
    Console.prototype[name] = function (...args) {
      write(this, false, format(args));
    };
  }
  for (const name of ['warn', 'error']) {
    Console.prototype[name] = function (...args) {
      write(this, true, format(args));
    };
  }

  Console.prototype.dir = function (value, options) {
    if (options !== undefined) {
      unsupported('dir options');
    }
    write(this, false, inspect(value));
  };

  Console.prototype.assert = function (value, ...args) {
    state(this);
    if (!value) {
      write(this, true, 'Assertion failed' + (args.length ? ': ' + format(args) : ''));
    }
  };

  for (const name of [
    'table',
    'time',
    'timeEnd',
    'timeLog',
    'count',
    'countReset',
    'group',
    'groupCollapsed',
    'groupEnd',
    'clear',
    'trace',
    'dirxml',
    'profile',
    'profileEnd',
    'timeStamp'
  ]) {
    Console.prototype[name] = function () {
      state(this);
      unsupported(name);
    };
  }
  const instance = new Console(host.process.stdout, host.process.stderr);
  const output = host.console ?? {};
  for (const name of [
    ...methods,
    'table',
    'time',
    'timeEnd',
    'timeLog',
    'count',
    'countReset',
    'group',
    'groupCollapsed',
    'groupEnd',
    'clear',
    'trace',
    'dirxml',
    'profile',
    'profileEnd',
    'timeStamp'
  ]) {
    // Preserve the captured global object's existing methods and accessor
    // wrappers, including CodeOSS's parent-pipe logging hooks. Presence checks
    // do not evaluate an existing getter merely to decide whether to replace it.
    if (!(name in output)) {
      output[name] = instance[name].bind(instance);
    }
  }
  output.Console = Console;
  return output;
}
