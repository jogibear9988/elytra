/** Bounded constraint admission; native limits and frame conversions stay authoritative. */
export default function createConstraints(native, error, unsupported) {
  function dimension(value, name) {
    if (typeof value !== 'number' || !Number.isInteger(value)) {
      throw Object.assign(new TypeError(`${name} must be an integer`), {
        code: 'ERR_INVALID_ARG_TYPE'
      });
    }
    if (value < 0 || value > 16384) {
      throw Object.assign(new RangeError(`${name} must be between 0 and 16384`), {
        code: 'ERR_OUT_OF_RANGE'
      });
    }
    return value;
  }

  function initial(options) {
    const result = {};
    for (const name of ['minWidth', 'minHeight', 'maxWidth', 'maxHeight']) {
      const value = options[name];
      result[name] = value === undefined ? 0 : dimension(value, name);
    }
    return result;
  }

  function get(id, minimum) {
    const method = minimum ? 'minimumSize' : 'maximumSize';
    if (typeof native[method] !== 'function') {
      unsupported('Native window size constraints');
    }
    const wire = native[method](id);
    if (typeof wire !== 'string' || wire.length > 128) {
      throw error('ERR_FIREFLY_GEOMETRY_DATA', 'Invalid native window size constraints');
    }
    let value;
    try {
      value = JSON.parse(wire);
    } catch {
      throw error('ERR_FIREFLY_GEOMETRY_DATA', 'Invalid native window size constraints');
    }
    // Constructor content units can add real frame margins, and Electron can
    // report an unbounded constructor axis as INT_MAX. Do not reinterpret them.
    if (
      !Array.isArray(value) ||
      value.length !== 2 ||
      value.some((item) => !Number.isInteger(item) || item < 0 || item > 2147483647)
    ) {
      throw error('ERR_FIREFLY_GEOMETRY_DATA', 'Invalid native window size constraints');
    }
    return [...value];
  }

  function set(id, width, height, minimum) {
    width = dimension(width, 'width');
    height = dimension(height, 'height');
    const method = minimum ? 'setMinimumSize' : 'setMaximumSize';
    if (typeof native[method] !== 'function') {
      unsupported('Native window size constraints');
    }
    native[method](id, width, height);
  }

  return { initial, get, set };
}
