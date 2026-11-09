/** Node's formatter guard bypasses nested hooks while preserving genuine default frames. */
import assert from 'node:assert/strict';
import { assertOrder, stackCaptureTop } from './helpers.mjs';

const original = Object.getOwnPropertyDescriptor(Error, 'prepareStackTrace');
try {
  let calls = 0;

  Error.prepareStackTrace = function (target) {
    assert.equal(this, Error);
    ++calls;
    assert.equal(calls, 1, 'Same-target recursion must not reenter the application formatter');
    return target.stack;
  };

  const self = stackCaptureTop({ name: 'Self', message: 'default fallback' });
  const selfValue = self.stack;
  assert.equal(selfValue.split('\n')[0], 'Self: default fallback');
  assertOrder(selfValue, ['stackCaptureLeaf', 'stackCaptureMiddle', 'stackCaptureTop']);
  assert.equal(self.stack, selfValue);
  assert.equal(calls, 1);
  calls = 0;

  Error.prepareStackTrace = function nestedFormatter(target) {
    assert.equal(this, Error);
    ++calls;
    assert.equal(calls, 1, 'Another capture record must not reenter the active realm formatter');
    const other = stackCaptureTop({ name: 'Other', message: 'default fallback' });
    return { target, other: other.stack };
  };

  const outer = stackCaptureTop({});
  const result = outer.stack;
  assert.equal(result.target, outer);
  assert.equal(result.other.split('\n')[0], 'Other: default fallback');
  assertOrder(result.other, [
    'stackCaptureLeaf',
    'stackCaptureMiddle',
    'stackCaptureTop',
    'nestedFormatter'
  ]);
  assert.equal(outer.stack, result);
  assert.equal(calls, 1);
  if (process.versions.mite) {
    delete Error.prepareStackTrace;
    const recursiveHeader = {
      get name() {
        return recursiveHeader.stack;
      },

      message: 'bounded recursive header'
    };
    Error.captureStackTrace(recursiveHeader);

    assert.throws(
      () => recursiveHeader.stack,
      (error) => error.code === 'ERR_MITE_UNSUPPORTED_API'
    );

    Object.defineProperty(recursiveHeader, 'name', { value: 'Recovered', configurable: true });
    assert.equal(recursiveHeader.stack.split('\n')[0], 'Recovered: bounded recursive header');
  }
  console.log(
    'PASS captureStackTrace same and other target formatter reentry uses genuine default frames once'
  );
} finally {
  if (original) {
    Object.defineProperty(Error, 'prepareStackTrace', original);
  } else {
    delete Error.prepareStackTrace;
  }
}
