/** Real LibJS/Node frame snapshots with function-identity constructor trimming. */
import assert from 'node:assert/strict';
import {
  assertOrder,
  frames,
  stackCaptureLeaf,
  stackCaptureMiddle,
  stackCaptureTop
} from './helpers.mjs';

const all = stackCaptureTop({});
const original = all.stack;
const lines = assertOrder(original, ['stackCaptureLeaf', 'stackCaptureMiddle', 'stackCaptureTop']);
assert.ok(lines[0].includes('stackCaptureLeaf'));
assert.ok(lines.some((line) => /helpers\.mjs:\d+(?::\d+)?/.test(line)));
assert.equal(
  lines.some((line) => /\bcaptureStackTrace\b/.test(line)),
  false
);
const trimmed = stackCaptureTop({}, stackCaptureMiddle).stack;
assert.ok(frames(trimmed)[0].includes('stackCaptureTop'));
assert.equal(
  frames(trimmed).some((line) => /\bstackCaptureLeaf\b|\bstackCaptureMiddle\b/.test(line)),
  false
);

const sameNameButAbsent = function stackCaptureMiddle() {};

assert.deepEqual(frames(stackCaptureTop({}, sameNameButAbsent).stack), []);
for (const ignored of [undefined, null, false, 1, 'not a constructor']) {
  assertOrder(stackCaptureTop({}, ignored).stack, [
    'stackCaptureLeaf',
    'stackCaptureMiddle',
    'stackCaptureTop'
  ]);
}
assert.ok(frames(stackCaptureTop({}, stackCaptureLeaf).stack)[0].includes('stackCaptureMiddle'));
console.log(
  'PASS captureStackTrace actual named source frames and exact constructor function identity trimming'
);

class CapturedError extends Error {
  constructor() {
    super('custom error');
    this.name = 'CapturedError';
    Error.captureStackTrace(this, CapturedError);
  }
}

function customConstructorCaller() {
  return new CapturedError();
}

const custom = customConstructorCaller();
assert.ok(custom instanceof CapturedError);
assert.equal(custom.stack.split('\n')[0], 'CapturedError: custom error');
assert.ok(frames(custom.stack)[0].includes('customConstructorCaller'));
assert.equal(
  frames(custom.stack).some((line) => /\bCapturedError\b/.test(line)),
  false
);

function recursiveCapture(level, target) {
  if (level > 0) {
    return recursiveCapture(level - 1, target);
  }
  Error.captureStackTrace(target, recursiveCapture);
  return target;
}

const recursive = recursiveCapture(3, {}).stack;
assert.equal(frames(recursive).filter((line) => /\brecursiveCapture\b/.test(line)).length, 3);

function recaptureAtNewSite(target) {
  Error.captureStackTrace(target);
}

recaptureAtNewSite(all);
assert.ok(frames(all.stack)[0].includes('recaptureAtNewSite'));
assert.notEqual(all.stack, original);
console.log(
  'PASS captureStackTrace custom Error constructor recursion trimming and genuine recapture site'
);
