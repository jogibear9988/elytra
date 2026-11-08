/** Bind the pinned portable provider; preserve honest limits and modern error classification. */
import assert from 'assert/';
import { checkDeepValues, unavailable } from '../../mite/modules/core/assert/policy.mjs';

export default function createAssertModule() {
  for (const name of ['deepEqual', 'notDeepEqual', 'deepStrictEqual', 'notDeepStrictEqual']) {
    const original = assert[name];

    assert[name] = function (...args) {
      checkDeepValues(args[0], args[1]);
      return original(...args);
    };
  }
  // These aliases must follow our guarded methods, not the provider's old references.
  assert.strict.deepEqual = assert.deepStrictEqual;
  assert.strict.notDeepEqual = assert.notDeepStrictEqual;
  assert.strict.deepStrictEqual = assert.deepStrictEqual;
  assert.strict.notDeepStrictEqual = assert.notDeepStrictEqual;
  const throws = assert.throws;
  const rejects = assert.rejects;

  function mismatch(actual, expected, message, operator) {
    if (message instanceof Error) {
      throw message;
    }
    throw new assert.AssertionError({ actual, expected, message, operator });
  }

  // A validator's own exception must propagate, even if it is the same object
  // as the exception being tested. Proxy forwarding preserves constructor checks.
  function validator(expected, record) {
    if (typeof expected !== 'function') {
      return expected;
    }
    return new Proxy(expected, {
      apply(target, receiver, args) {
        try {
          return Reflect.apply(target, receiver, args);
        } catch (error) {
          record.validatorThrew = true;
          throw error;
        }
      }
    });
  }

  function normalize(error, expected, forwarded) {
    if (error?.code === 'ERR_ASSERTION' && error.expected === forwarded) {
      error.expected = expected;
    }
    return error;
  }

  function pattern(actual, expected) {
    if (!expected || typeof expected !== 'object' || expected instanceof RegExp) {
      return;
    }
    checkDeepValues(expected, undefined);
    if (actual === null || typeof actual !== 'object') {
      return;
    }
    for (const key of Object.keys(expected)) {
      checkDeepValues(actual[key], expected[key]);
    }
  }

  assert.throws = function (block, expected, ...rest) {
    if (typeof block !== 'function') {
      return throws(block, expected, ...rest);
    }
    let seen = false;
    let actual;
    const record = { validatorThrew: false };
    const forwarded = validator(expected, record);
    try {
      const result = throws(
        () => {
          try {
            return block();
          } catch (error) {
            seen = true;
            actual = error;
            throw error;
          }
        },
        forwarded,
        ...rest
      );
      if (seen) {
        pattern(actual, expected);
      }
      return result;
    } catch (error) {
      if (seen && error === actual && expected && !record.validatorThrew) {
        mismatch(actual, expected, rest[0], 'throws');
      }
      throw normalize(error, expected, forwarded);
    }
  };

  assert.rejects = function (input, expected, ...rest) {
    let seen = false;
    let actual;
    const record = { validatorThrew: false };
    const forwarded = validator(expected, record);

    const observe = (promise) => {
      if (
        promise &&
        typeof promise === 'object' &&
        !(promise instanceof Promise) &&
        typeof promise.then === 'function'
      ) {
        unavailable('non-native thenable rejection validation');
      }
      return promise instanceof Promise
        ? promise.then(
            (value) => value,
            (error) => {
              seen = true;
              actual = error;
              throw error;
            }
          )
        : promise;
    };

    let observed;
    try {
      observed = typeof input === 'function' ? () => observe(input()) : observe(input);
    } catch (error) {
      return Promise.reject(error);
    }
    return rejects(observed, forwarded, ...rest)
      .then((result) => {
        if (seen) {
          pattern(actual, expected);
        }
        return result;
      })
      .catch((error) => {
        if (seen && error === actual && expected && !record.validatorThrew) {
          mismatch(actual, expected, rest[0], 'rejects');
        }
        throw normalize(error, expected, forwarded);
      });
  };

  assert.strict.throws = assert.throws;
  assert.strict.rejects = assert.rejects;
  const fail = assert.fail;

  assert.fail = function (...args) {
    if (args.length > 1) {
      unavailable('legacy fail overload');
    }
    return fail(...args);
  };

  assert.strict.fail = assert.fail;
  for (const name of ['Assert', 'CallTracker', 'partialDeepStrictEqual']) {
    assert[name] = function () {
      unavailable(name);
    };

    assert.strict[name] = assert[name];
  }
  // The older provider overwrites this field even when it generated the message.
  // Wrap only callable entry points; keep every named method/class shared.
  function callable(target) {
    const wrapped = function (...args) {
      try {
        return Reflect.apply(target, this, args);
      } catch (error) {
        if (error instanceof assert.AssertionError && args[1] == null) {
          error.generatedMessage = true;
        }
        throw error;
      }
    };

    Object.defineProperty(wrapped, 'name', { value: target.name });
    return Object.assign(wrapped, target);
  }

  const result = callable(assert);
  result.ok = result;
  result.strict = callable(assert.strict);
  result.strict.ok = result;
  result.strict.strict = result.strict;
  return result;
}
