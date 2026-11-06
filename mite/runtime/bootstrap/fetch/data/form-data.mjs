/** Ordered FormData lists hold scalar strings or immutable Files; no DOM is required. */
export default function createFormData(blobs) {
  const { File, scalar, isBlob, isFile } = blobs;
  const states = new WeakMap();

  function entries(value) {
    const list = states.get(value);
    if (!list) {
      throw new TypeError('Expected FormData receiver');
    }
    return list;
  }

  function entry(args) {
    if (args.length < 2) {
      throw new TypeError('FormData requires name and value');
    }
    const name = scalar(args[0]);
    const value = args[1];
    if (!isBlob(value)) {
      if (args.length >= 3) {
        throw new TypeError('A filename requires a Blob value');
      }
      return [name, scalar(value)];
    }
    if (args[2] === undefined && isFile(value)) {
      return [name, value];
    }
    const filename = args[2] === undefined ? 'blob' : scalar(args[2]);
    return [
      name,
      new File([value], filename, {
        type: blobs.type(value),
        lastModified: isFile(value) ? value.lastModified : Date.now()
      })
    ];
  }

  class FormData {
    constructor(form = undefined) {
      if (form !== undefined) {
        throw new TypeError('HTML form constructors are unavailable');
      }
      states.set(this, []);
    }

    append(...args) {
      entries(this).push(entry(args));
    }

    set(...args) {
      const list = entries(this);
      const pair = entry(args);
      const index = list.findIndex(([key]) => key === pair[0]);
      const remaining = list.filter(([key]) => key !== pair[0]);
      remaining.splice(index < 0 ? remaining.length : index, 0, pair);
      states.set(this, remaining);
    }

    delete(name) {
      if (!arguments.length) {
        throw new TypeError('delete requires a name');
      }
      name = scalar(name);
      states.set(
        this,
        entries(this).filter(([key]) => key !== name)
      );
    }

    get(name) {
      if (!arguments.length) {
        throw new TypeError('get requires a name');
      }
      name = scalar(name);
      return entries(this).find(([key]) => key === name)?.[1] ?? null;
    }

    getAll(name) {
      if (!arguments.length) {
        throw new TypeError('getAll requires a name');
      }
      name = scalar(name);
      return entries(this)
        .filter(([key]) => key === name)
        .map(([, value]) => value);
    }

    has(name) {
      if (!arguments.length) {
        throw new TypeError('has requires a name');
      }
      name = scalar(name);
      return entries(this).some(([key]) => key === name);
    }

    *entries() {
      for (let index = 0; index < entries(this).length; index++) {
        yield [...entries(this)[index]];
      }
    }

    *keys() {
      for (const [name] of this.entries()) {
        yield name;
      }
    }

    *values() {
      for (const [, value] of this.entries()) {
        yield value;
      }
    }

    forEach(callback, thisArg = undefined) {
      entries(this);
      if (typeof callback !== 'function') {
        throw new TypeError('callback must be a function');
      }
      for (const [name, value] of this.entries()) {
        callback.call(thisArg, value, name, this);
      }
    }
  }
  FormData.prototype[Symbol.iterator] = FormData.prototype.entries;
  Object.defineProperty(FormData.prototype, Symbol.toStringTag, {
    value: 'FormData',
    configurable: true
  });
  return { FormData, isFormData: (value) => states.has(value) };
}
