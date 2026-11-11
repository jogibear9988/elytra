/** Bounded own-data inspection and real custom formatters, independent of global console hooks. */
export default function createInspection(host) {
  const custom = Symbol.for('nodejs.util.inspect.custom');
  const descriptor = Object.getOwnPropertyDescriptor;
  const prototype = Object.getPrototypeOf;
  const ownKeys = Reflect.ownKeys;
  const apply = Reflect.apply;
  const mapEntries = Map.prototype.entries;
  const mapSize = descriptor(Map.prototype, 'size').get;
  const mapNext = prototype(apply(mapEntries, new Map(), [])).next;
  const setValues = Set.prototype.values;
  const setSize = descriptor(Set.prototype, 'size').get;
  const setNext = prototype(apply(setValues, new Set(), [])).next;
  const dateTime = Date.prototype.getTime;
  const dateISO = Date.prototype.toISOString;
  const regexpPrototype = RegExp.prototype;
  const regexpSource = descriptor(regexpPrototype, 'source').get;
  const regexpFlags = [
    'hasIndices',
    'global',
    'ignoreCase',
    'multiline',
    'dotAll',
    'unicode',
    'unicodeSets',
    'sticky'
  ].map((name) => descriptor(regexpPrototype, name)?.get);
  const defaults = {
    depth: 2,
    colors: false,
    customInspect: true,
    showHidden: false,
    showProxy: false,
    getters: false,
    sorted: false,
    maxArrayLength: 100,
    maxStringLength: 10000,
    breakLength: 80,
    compact: 3,
    numericSeparator: false
  };
  let current;

  const identity = (text) => text;

  function fail(code, message, Type = Error) {
    throw Object.assign(new Type(message), { code });
  }

  function unsupported(name) {
    fail('ERR_MITE_UNSUPPORTED_API', 'util.inspect ' + name + ' is not implemented');
  }

  function options(input, depth, colors) {
    if (typeof input === 'boolean') {
      input = {
        showHidden: input,
        ...(depth === undefined ? {} : { depth }),
        ...(colors === undefined ? {} : { colors })
      };
    }
    if (input == null) {
      input = {};
    }
    if (typeof input !== 'object' || Array.isArray(input)) {
      fail('ERR_INVALID_ARG_TYPE', 'inspect options must be an object', TypeError);
    }
    const result = { ...defaults, stylize: identity };
    for (const key of ownKeys(input)) {
      if (typeof key !== 'string' || (!(key in defaults) && key !== 'stylize')) {
        unsupported('option');
      }
      result[key] = input[key];
    }
    if (
      result.colors ||
      result.showHidden ||
      result.showProxy ||
      result.getters ||
      result.numericSeparator
    ) {
      unsupported('colors/hidden/proxy/getter/numeric formatting');
    }
    if (
      result.depth !== null &&
      (!Number.isInteger(result.depth) || result.depth < -64 || result.depth > 64)
    ) {
      fail(
        'ERR_OUT_OF_RANGE',
        'inspect depth must be null or an integer from -64 to 64',
        RangeError
      );
    }
    for (const key of ['maxArrayLength', 'maxStringLength']) {
      if (
        result[key] !== null &&
        (!Number.isInteger(result[key]) || result[key] < 0 || result[key] > 1024 * 1024)
      ) {
        fail('ERR_OUT_OF_RANGE', 'Invalid inspect length limit', RangeError);
      }
    }
    if (
      typeof result.customInspect !== 'boolean' ||
      typeof result.sorted !== 'boolean' ||
      typeof result.stylize !== 'function'
    ) {
      fail('ERR_INVALID_ARG_TYPE', 'Invalid inspect option', TypeError);
    }
    if (
      !(result.breakLength >= 0) ||
      !(result.compact === true || result.compact === false || result.compact === 3)
    ) {
      unsupported('layout options');
    }
    return result;
  }

  function bounded(text, context) {
    if (typeof text !== 'string') {
      text = String(text);
    }
    if (text.length > 1024 * 1024) {
      fail('ERR_MITE_INSPECT_LIMIT', 'Inspection output exceeds 1 MiB', RangeError);
    }
    context.output += text.length;
    if (context.output > 4 * 1024 * 1024) {
      fail('ERR_MITE_INSPECT_LIMIT', 'Inspection work exceeds its budget', RangeError);
    }
    return text;
  }

  function quote(value, opts) {
    const length =
      opts.maxStringLength === null ? value.length : Math.min(value.length, opts.maxStringLength);
    if (length > 1024 * 1024) {
      fail('ERR_MITE_INSPECT_LIMIT', 'Inspection string exceeds 1 MiB', RangeError);
    }
    const text = value.slice(0, length);
    const delimiter = !text.includes("'")
      ? "'"
      : !text.includes('"')
        ? '"'
        : !text.includes('`') && !text.includes('${')
          ? '`'
          : "'";
    let output = delimiter;
    for (const char of text) {
      const code = char.charCodeAt(0);
      if (char === delimiter || char === '\\') {
        output += '\\' + char;
      } else if (char === '\n') {
        output += '\\n';
      } else if (char === '\r') {
        output += '\\r';
      } else if (char === '\t') {
        output += '\\t';
      } else if (code < 32 || code === 127) {
        output += '\\x' + code.toString(16).padStart(2, '0');
      } else {
        output += char;
      }
    }
    output += delimiter;
    if (length < value.length) {
      output += `... ${value.length - length} more characters`;
    }
    return output;
  }

  function data(value, key) {
    for (let count = 0; value !== null; count++, value = prototype(value)) {
      if (count >= 64) {
        fail('ERR_MITE_INSPECT_LIMIT', 'Inspection prototype chain exceeds budget', RangeError);
      }
      if (host.types?.isProxy(value)) {
        unsupported('Proxy inspection');
      }
      const entry = descriptor(value, key);
      if (entry) {
        return 'value' in entry ? entry.value : undefined;
      }
    }
  }

  function container(prefix, open, close, entries, opts) {
    if (!entries.length) {
      return prefix + open + close;
    }
    const line = prefix + open + ' ' + entries.join(', ') + ' ' + close;
    if (opts.compact !== false && line.length <= opts.breakLength) {
      return line;
    }
    return (
      prefix +
      open +
      '\n  ' +
      entries.join(',\n  ').replaceAll('\n', '\n  ').replaceAll('\n    ', '\n  ') +
      '\n' +
      close
    );
  }

  function render(value, depth, opts, context) {
    if (++context.nodes > 4096 || ++context.calls > 64) {
      fail('ERR_MITE_INSPECT_LIMIT', 'Inspection graph exceeds budget', RangeError);
    }
    try {
      if (typeof value === 'string') {
        return bounded(opts.stylize(quote(value, opts), 'string'), context);
      }
      if (value === null) {
        return opts.stylize('null', 'null');
      }
      if (typeof value === 'bigint') {
        return opts.stylize(String(value) + 'n', 'bigint');
      }
      if (typeof value === 'number') {
        return opts.stylize(Object.is(value, -0) ? '-0' : String(value), 'number');
      }
      if (typeof value === 'boolean' || typeof value === 'undefined' || typeof value === 'symbol') {
        return opts.stylize(String(value), typeof value);
      }
      if (!host.types || typeof host.types.isProxy !== 'function') {
        unsupported('object inspection without native brands');
      }
      if (host.types.isProxy(value)) {
        unsupported('Proxy inspection');
      }
      const previous = context.active.get(value);
      if (previous) {
        previous.reference ??= context.next++;
        return `[Circular *${previous.reference}]`;
      }
      const record = {};
      context.active.set(value, record);
      try {
        if (opts.customInspect) {
          const hook = data(value, custom);
          if (typeof hook === 'function' && hook !== inspect) {
            if (++context.hooks > 256) {
              fail('ERR_MITE_INSPECT_LIMIT', 'Custom inspection exceeds budget', RangeError);
            }
            const result = apply(hook, value, [depth, { ...opts }, inspect]);
            if (result !== value) {
              const output =
                typeof result === 'string' ? result : render(result, depth, opts, context);
              return bounded(
                (record.reference ? `<ref *${record.reference}> ` : '') + output,
                context
              );
            }
          }
        }
        let output;
        let size;
        let map = false;
        let set = false;
        try {
          size = apply(mapSize, value, []);
          map = true;
        } catch {}
        if (!map) {
          try {
            size = apply(setSize, value, []);
            set = true;
          } catch {}
        }
        if (depth !== null && depth < 0) {
          output = opts.stylize(
            map ? '[Map]' : set ? '[Set]' : Array.isArray(value) ? '[Array]' : '[Object]',
            'special'
          );
        } else if (map || set) {
          const entries = [];
          const iterator = apply(map ? mapEntries : setValues, value, []);
          const limit = opts.maxArrayLength === null ? size : Math.min(size, opts.maxArrayLength);
          if (limit > 16384) {
            fail('ERR_MITE_INSPECT_LIMIT', 'Collection exceeds inspection budget', RangeError);
          }
          for (let index = 0; index < limit; index++) {
            const entry = apply(map ? mapNext : setNext, iterator, []).value;
            entries.push(
              map
                ? render(entry[0], depth === null ? null : depth - 1, opts, context) +
                    ' => ' +
                    render(entry[1], depth === null ? null : depth - 1, opts, context)
                : render(entry, depth === null ? null : depth - 1, opts, context)
            );
          }
          if (size > limit) {
            entries.push(`... ${size - limit} more items`);
          }
          output = container((map ? 'Map' : 'Set') + '(' + size + ') ', '{', '}', entries, opts);
        } else {
          let date;
          let regexp;
          try {
            date = apply(dateTime, value, []);
          } catch {}
          if (date !== undefined) {
            return bounded(
              Number.isNaN(date) ? 'Invalid Date' : apply(dateISO, value, []),
              context
            );
          }
          if (value !== regexpPrototype) {
            try {
              regexp = apply(regexpSource, value, []);
            } catch {}
          }
          if (regexp !== undefined) {
            let flags = '';
            const letters = 'dgimsuvy';
            for (let index = 0; index < regexpFlags.length; index++) {
              if (regexpFlags[index] && apply(regexpFlags[index], value, [])) {
                flags += letters[index];
              }
            }
            return bounded('/' + regexp + '/' + flags, context);
          }
          if (host.types.isPromise(value)) {
            unsupported('Promise state inspection');
          }
          if (host.types.isAnyArrayBuffer(value) || host.types.isArrayBufferView(value)) {
            unsupported('binary inspection');
          }
          const names = ownKeys(value);
          context.properties += names.length;
          if (context.properties > 16384) {
            fail('ERR_MITE_INSPECT_LIMIT', 'Inspection property budget exceeded', RangeError);
          }
          const array = Array.isArray(value);
          const entries = [];

          const renderProperty = (key) => {
            const entry = descriptor(value, key);
            if (!entry || !entry.enumerable) {
              return;
            }
            const label =
              typeof key === 'symbol'
                ? '[' + String(key) + ']'
                : /^[A-Za-z_$][\w$]*$/.test(key)
                  ? key
                  : quote(key, opts);
            entries.push(
              label +
                ': ' +
                ('value' in entry
                  ? render(entry.value, depth === null ? null : depth - 1, opts, context)
                  : entry.get && entry.set
                    ? '[Getter/Setter]'
                    : entry.get
                      ? '[Getter]'
                      : '[Setter]')
            );
          };

          if (array) {
            const length = descriptor(value, 'length').value;
            const limit =
              opts.maxArrayLength === null ? length : Math.min(length, opts.maxArrayLength);
            if (limit > 16384) {
              fail('ERR_MITE_INSPECT_LIMIT', 'Inspection array exceeds budget', RangeError);
            }
            for (let index = 0; index < limit; index++) {
              const entry = descriptor(value, String(index));
              if (!entry) {
                let empty = 1;
                while (index + 1 < limit && !descriptor(value, String(index + 1))) {
                  empty++;
                  index++;
                }
                entries.push(`<${empty} empty item${empty === 1 ? '' : 's'}>`);
              } else {
                entries.push(
                  'value' in entry
                    ? render(entry.value, depth === null ? null : depth - 1, opts, context)
                    : '[Getter]'
                );
              }
            }
            if (length > limit) {
              entries.push(`... ${length - limit} more items`);
            }
            for (const key of names) {
              if (
                typeof key === 'symbol' ||
                (key !== 'length' && !/^(?:0|[1-9][0-9]*)$/.test(key))
              ) {
                renderProperty(key);
              }
            }
            output = container('', '[', ']', entries, opts);
          } else {
            const selected = opts.sorted
              ? names.sort((a, b) => String(a).localeCompare(String(b)))
              : names;
            for (const key of selected) {
              renderProperty(key);
            }
            let prefix = '';
            const parent = prototype(value);
            if (parent === null) {
              prefix = '[Object: null prototype] ';
            } else {
              const ctor = data(parent, 'constructor');
              const name = typeof ctor === 'function' ? data(ctor, 'name') : undefined;
              if (typeof value === 'function') {
                const functionName = data(value, 'name');
                prefix = functionName
                  ? '[Function: ' + functionName + ']'
                  : '[Function (anonymous)]';
                if (entries.length) {
                  prefix += ' ';
                }
              } else if (typeof name === 'string' && name !== 'Object') {
                if (
                  [
                    'Error',
                    'TypeError',
                    'RangeError',
                    'ReferenceError',
                    'SyntaxError',
                    'AggregateError',
                    'EvalError',
                    'URIError'
                  ].includes(name)
                ) {
                  unsupported('Error stack inspection');
                }
                if (
                  [
                    'Boolean',
                    'Number',
                    'String',
                    'BigInt',
                    'Symbol',
                    'WeakMap',
                    'WeakSet',
                    'WeakRef',
                    'FinalizationRegistry'
                  ].includes(name)
                ) {
                  unsupported('boxed/weak object inspection');
                }
                if (name.length > 1024) {
                  fail(
                    'ERR_MITE_INSPECT_LIMIT',
                    'Inspection constructor label exceeds budget',
                    RangeError
                  );
                }
                prefix = name + ' ';
              }
            }
            output =
              typeof value === 'function' && !entries.length
                ? prefix
                : container(prefix, '{', '}', entries, opts);
          }
        }
        return bounded((record.reference ? `<ref *${record.reference}> ` : '') + output, context);
      } finally {
        context.active.delete(value);
      }
    } finally {
      context.calls--;
    }
  }

  function inspect(value, input, depth, colors) {
    const opts = options(input, depth, colors);
    const outer = current;
    const context = current ?? {
      nodes: 0,
      properties: 0,
      hooks: 0,
      calls: 0,
      output: 0,
      active: new Map(),
      next: 1
    };
    current = context;
    try {
      return render(value, opts.depth, opts, context);
    } finally {
      current = outer;
    }
  }

  Object.defineProperty(inspect, 'custom', {
    value: custom,
    enumerable: true,
    configurable: true,
    writable: true
  });

  function formatWithOptions(input, ...args) {
    const opts = options(input);
    if (!args.length) {
      return '';
    }
    let index = 1;
    if (typeof args[0] !== 'string') {
      return args
        .map((value) => (typeof value === 'string' ? value : inspect(value, opts)))
        .join(' ');
    }
    const head = args[0].replace(/%[%sdifjoOc]/g, (token) => {
      if (token === '%%') {
        return args.length > 1 ? '%' : '%%';
      }
      if (index >= args.length) {
        return token;
      }
      const value = args[index++];
      if (token === '%c') {
        return '';
      }
      if (token === '%s') {
        return value !== null && typeof value === 'object'
          ? inspect(value, { ...opts, depth: 0 })
          : String(value);
      }
      if (token === '%d') {
        return typeof value === 'bigint' ? String(value) + 'n' : String(Number(value));
      }
      if (token === '%i') {
        return typeof value === 'bigint' ? String(value) + 'n' : String(parseInt(value, 10));
      }
      if (token === '%f') {
        return String(parseFloat(value));
      }
      if (token === '%j') {
        const cycle = {};
        const ancestors = [];
        let nodes = 0;
        try {
          return JSON.stringify(value, function (key, item) {
            if (++nodes > 16384 || (typeof item === 'string' && item.length > 1024 * 1024)) {
              fail('ERR_MITE_INSPECT_LIMIT', 'JSON formatting exceeds budget', RangeError);
            }
            if (item && typeof item === 'object') {
              while (ancestors.length && ancestors.at(-1) !== this) {
                ancestors.pop();
              }
              if (ancestors.includes(item)) {
                throw cycle;
              }
              if (ancestors.length >= 64) {
                fail('ERR_MITE_INSPECT_LIMIT', 'JSON formatting depth exceeds budget', RangeError);
              }
              ancestors.push(item);
            }
            return item;
          });
        } catch (error) {
          if (error === cycle) {
            return '[Circular]';
          }
          throw error;
        }
      }
      return inspect(value, opts);
    });
    const result = [
      head,
      ...args
        .slice(index)
        .map((value) => (typeof value === 'string' ? value : inspect(value, opts)))
    ].join(' ');
    if (result.length > 1024 * 1024) {
      fail('ERR_MITE_INSPECT_LIMIT', 'Formatted output exceeds 1 MiB', RangeError);
    }
    return result;
  }

  return { inspect, formatWithOptions, format: (...args) => formatWithOptions({}, ...args) };
}
