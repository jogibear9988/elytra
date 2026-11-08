/** Read-only managed policy adapter for the explicitly selected @vscode/policy-watcher bridge. */
export default function createPolicyWatcher(native) {
  const active = new Set();
  const enqueue = queueMicrotask;
  const byteLength = Buffer.byteLength.bind(Buffer);

  const error = (code, message) => Object.assign(Error(message), { code });

  const DATA = 'ERR_MITE_POLICY_DATA';
  const LIMIT = 'ERR_MITE_POLICY_LIMIT';

  function text(value, label, maximum, allowNul = false) {
    if (typeof value !== 'string') {
      throw error('ERR_INVALID_ARG_TYPE', label + ' must be a string');
    }
    for (let i = 0; i < value.length; i++) {
      const unit = value.charCodeAt(i);
      if (unit >= 0xd800 && unit <= 0xdbff) {
        const low = value.charCodeAt(++i);
        if (!(low >= 0xdc00 && low <= 0xdfff)) {
          throw error('ERR_INVALID_ARG_VALUE', 'Unpaired surrogate in ' + label);
        }
      } else if (unit >= 0xdc00 && unit <= 0xdfff) {
        throw error('ERR_INVALID_ARG_VALUE', 'Unpaired surrogate in ' + label);
      }
    }
    if ((!allowNul && value.includes('\0')) || byteLength(value, 'utf8') > maximum) {
      throw error('ERR_INVALID_ARG_VALUE', label + ' exceeds its text budget or contains NUL');
    }
    return value;
  }

  function record(value, label) {
    if (!value || typeof value !== 'object' || Array.isArray(value)) {
      throw error('ERR_INVALID_ARG_TYPE', label + ' must be an object');
    }
  }

  function definitions(source) {
    record(source, 'Policy definitions');
    const names = Reflect.ownKeys(source);
    if (names.length > 256) {
      throw error(LIMIT, 'At most 256 policy definitions');
    }
    const snapshot = Object.create(null);
    const types = new Map();
    for (const name of names) {
      text(name, 'Policy key', 4096);
      if (!name) {
        throw error('ERR_INVALID_ARG_VALUE', 'Policy key must not be empty');
      }
      const definition = source[name];
      record(definition, 'Policy definition');
      // Code-OSS nativePolicyService passes additional metadata; the upstream addon
      // reads only type. Never forward that metadata to the native provider.
      const raw = definition.type;
      const array = Array.isArray(raw);
      const length = array ? raw.length : 1;
      // Bound the native-style indexed snapshot before reading any member. An
      // application iterator must not turn a short union into unbounded work.
      if (!Number.isInteger(length) || length < 1 || length > 256) {
        throw error('ERR_INVALID_ARG_VALUE', 'Policy type union must contain 1 to 256 entries');
      }
      const union = [];
      for (let index = 0; index < length; index++) {
        union.push(array ? raw[index] : raw);
      }
      if (union.some((type) => !['string', 'number', 'boolean'].includes(type))) {
        throw error('ERR_INVALID_ARG_VALUE', 'Policy type must contain string, number or boolean');
      }
      const unique = [...new Set(union)];
      snapshot[name] = { type: array ? unique : unique[0] };
      types.set(name, unique);
    }
    const json = JSON.stringify(snapshot);
    if (byteLength(json, 'utf8') > 1048576) {
      throw error(LIMIT, 'Policy definitions exceed 1 MiB');
    }
    return { json, types };
  }

  function retire(entry) {
    if (!entry.active) {
      return;
    }
    entry.active = false;
    active.delete(entry);
    entry.callback = null;
    entry.queue.length = 0;
    entry.queuedBytes = 0;
    entry.previous.clear();
    if (entry.id !== null) {
      native.stop(entry.id);
    }
  }

  function snapshot(entry, json) {
    if (typeof json !== 'string' || byteLength(json, 'utf8') > 1048576) {
      throw error(DATA, 'Invalid policy snapshot byte budget');
    }
    let packet;
    try {
      packet = JSON.parse(json);
    } catch {
      throw error(DATA, 'Invalid policy snapshot JSON');
    }
    try {
      record(packet, 'Policy snapshot');
      if (Object.keys(packet).length !== 1 || !Object.hasOwn(packet, 'values')) {
        throw Error('Invalid policy envelope');
      }
      record(packet.values, 'Policy values');
      const values = new Map();
      for (const key of Object.keys(packet.values)) {
        const types = entry.types.get(key);
        const value = packet.values[key];
        if (!types || !types.includes(typeof value)) {
          throw Error('Unexpected policy key or type');
        }
        if (typeof value === 'number' && !Number.isFinite(value)) {
          throw Error('Non-finite policy value');
        }
        if (typeof value === 'string') {
          text(value, 'Policy value', 65536, true);
        }
        values.set(key, value);
      }
      return values;
    } catch {
      throw error(DATA, 'Invalid native policy values');
    }
  }

  function schedule(entry) {
    if (entry.scheduled) {
      return;
    }
    entry.scheduled = true;

    enqueue(() => {
      entry.scheduled = false;
      if (!entry.active || !entry.admitted) {
        return;
      }
      const next = entry.queue.shift();
      if (!next) {
        return;
      }
      entry.queuedBytes -= next.bytes;
      if (entry.queue.length) {
        schedule(entry);
      }
      const values = next.values;
      let changed = !entry.delivered || values.size !== entry.previous.size;
      if (!changed) {
        for (const [key, value] of values) {
          if (!entry.previous.has(key) || !Object.is(value, entry.previous.get(key))) {
            changed = true;
            break;
          }
        }
      }
      if (!changed) {
        return;
      }
      const update = Object.create(null);
      for (const [key, value] of values) {
        update[key] = value;
      }
      for (const key of entry.previous.keys()) {
        if (!values.has(key)) {
          update[key] = undefined;
        }
      }
      entry.previous = values;
      entry.delivered = true;
      const callback = entry.callback;
      callback(update);
    });
  }

  function receiver(entry) {
    return (json) => {
      if (!entry.active) {
        return;
      }
      try {
        const values = snapshot(entry, json);
        const bytes = byteLength(json, 'utf8');
        if (entry.queue.length >= 64 || entry.queuedBytes + bytes > 1048576) {
          throw error(LIMIT, 'Policy callback queue exhausted');
        }
        entry.queue.push({ values, bytes });
        entry.queuedBytes += bytes;
        if (entry.admitted) {
          schedule(entry);
        }
      } catch (failure) {
        if (!entry.admitted) {
          entry.failure = failure;
          return;
        }
        try {
          retire(entry);
        } catch {
          /* Preserve the malformed-provider error. */
        }
        throw failure;
      }
    };
  }

  function disposable(entry) {
    return {
      dispose() {
        retire(entry);
      }
    };
  }

  function createWatcher(productName, source, callback, options) {
    const product = text(productName, 'Product name', 4096);
    if (!product) {
      throw error('ERR_INVALID_ARG_VALUE', 'Product name must not be empty');
    }
    if (typeof callback !== 'function') {
      throw error('ERR_INVALID_ARG_TYPE', 'Policy callback must be a function');
    }
    if (options !== undefined) {
      record(options, 'Watcher options');
      for (const key of Reflect.ownKeys(options)) {
        if (key !== 'registryPath') {
          throw error('ERR_MITE_UNSUPPORTED_API', 'Unsupported policy watcher option');
        }
      }
      const path = options.registryPath;
      if (path !== undefined && text(path, 'registryPath', 4096) !== '') {
        throw error('ERR_MITE_UNSUPPORTED_API', 'Custom registryPath is not implemented');
      }
    }
    const prepared = definitions(source);
    if (active.size >= 16) {
      throw error(LIMIT, 'Policy watcher quota exhausted');
    }
    const start = native.start;
    if (typeof start !== 'function' || typeof native.stop !== 'function') {
      throw error('ERR_MITE_UNSUPPORTED_API', 'Native policy watcher provider unavailable');
    }
    // Native property accessors may reenter construction before a slot is reserved.
    if (active.size >= 16) {
      throw error(LIMIT, 'Policy watcher quota exhausted');
    }
    const entry = {
      id: null,
      active: true,
      admitted: false,
      callback,
      types: prepared.types,
      previous: new Map(),
      delivered: false,
      queue: [],
      queuedBytes: 0,
      scheduled: false
    };
    active.add(entry);
    try {
      const id = start.call(native, product, prepared.json, receiver(entry));
      if (!Number.isInteger(id) || id <= 0 || id > 2147483647) {
        throw error(DATA, 'Invalid native policy watcher ID');
      }
      entry.id = id;
      if (!entry.active) {
        native.stop(id);
        throw error(DATA, 'Policy watcher retired during admission');
      }
      if (entry.failure) {
        throw entry.failure;
      }
      entry.admitted = true;
      if (entry.queue.length) {
        schedule(entry);
      }
      return disposable(entry);
    } catch (failure) {
      try {
        retire(entry);
      } catch {
        /* Preserve actual admission failure. */
      }
      throw failure;
    }
  }

  return { createWatcher };
}
