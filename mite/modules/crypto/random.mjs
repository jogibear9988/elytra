/** OS entropy APIs, callback ordering and unbiased bounded integer selection. */
import { fail, wrapBytes } from './bytes.mjs';

export default function createRandomApi(host) {
  const native = host.native;
  const Buffer = host.Buffer;

  function buffer(bytes) {
    return wrapBytes(Buffer, bytes);
  }

  /** Nonempty callback requests use native worker entropy; zero-length results complete synchronously. */
  function randomBytes(size, callback) {
    if (typeof size !== 'number') {
      fail('size', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    if (Number.isNaN(size) || size < 0 || size > 2147483647) {
      fail('size', RangeError, 'ERR_OUT_OF_RANGE');
    }
    size = size >>> 0;
    if (callback !== undefined && typeof callback !== 'function') {
      fail('callback', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    if (callback === undefined) {
      return buffer(native.random(size));
    }
    if (size === 0) {
      callback(null, buffer(native.random(0)));
      return;
    }

    native.random(size, (error, bytes) => {
      if (error) {
        callback(error);
      } else {
        callback(null, buffer(bytes));
      }
    });
  }

  function randomUUID(options) {
    if (
      options !== undefined &&
      (options === null || typeof options !== 'object' || Array.isArray(options))
    ) {
      fail('options', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    const disableEntropyCache = options?.disableEntropyCache;
    if (disableEntropyCache !== undefined && typeof disableEntropyCache !== 'boolean') {
      fail('options.disableEntropyCache', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    // This implementation asks the OS for fresh entropy on every call.
    return native.uuid();
  }

  /** Use rejection sampling over fresh entropy so non-power-of-two ranges remain unbiased. */
  function randomInt(min, max, callback) {
    if (max === undefined || typeof max === 'function') {
      callback = max;
      max = min;
      min = 0;
    }
    if (callback !== undefined && typeof callback !== 'function') {
      fail('callback', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    if (!Number.isSafeInteger(min)) {
      fail('min', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    if (!Number.isSafeInteger(max)) {
      fail('max', TypeError, 'ERR_INVALID_ARG_TYPE');
    }
    const range = max - min;
    const space = 281474976710656; // Exactly 2**48.
    if (max <= min || range >= space) {
      fail('max - min', RangeError, 'ERR_OUT_OF_RANGE');
    }
    const limit = space - (space % range);

    function select(bytes) {
      let value = 0;
      for (let i = 0; i < 6; i++) {
        value = value * 256 + bytes[i];
      }
      return value < limit ? min + (value % range) : undefined;
    }

    if (callback === undefined) {
      for (;;) {
        const result = select(native.random(6));
        if (result !== undefined) {
          return result;
        }
      }
    }

    function attempt() {
      native.random(6, (error, bytes) => {
        if (error) {
          callback(error);
          return;
        }
        const result = select(bytes);
        if (result === undefined) {
          attempt();
          return;
        }
        callback(undefined, result);
      });
    }
    // Fresh native worker entropy per attempt, without an integer cache.
    attempt();
  }

  return { randomBytes, randomUUID, randomInt };
}
