/** Compare observable assertion outcomes with Node, using independent test checks. */
import assert, { AssertionError, strictEqual } from 'node:assert';
import strict from 'node:assert/strict';
import { createRequire } from 'node:module';
const require = createRequire(import.meta.url);

const check = (value, message) => {
  if (!value) {
    throw Error(message);
  }
};

check(require('assert') === assert && require('assert/strict') === strict, 'shared aliases');
check(
  assert.strict === strict && strict.strict === strict && assert.ok === assert,
  'callable identities'
);
check(
  strict.equal === strictEqual && strict.deepEqual === assert.deepStrictEqual,
  'strict aliases'
);
const results = [];

function outcome(name, action) {
  try {
    action();
    results.push([name, 'pass']);
  } catch (error) {
    results.push([name, error.code ?? error.name, error.operator ?? null]);
  }
}

for (const value of [true, 1, 'x', {}, [], Symbol('x')]) {
  assert(value);
}
for (const value of [false, 0, '', null, undefined, NaN]) {
  let thrown;
  try {
    assert(value, 'explicit');
  } catch (error) {
    thrown = error;
  }
  check(
    (thrown instanceof AssertionError &&
      thrown.code === 'ERR_ASSERTION' &&
      thrown.actual === value) ||
      (Number.isNaN(value) && Number.isNaN(thrown?.actual)),
    'truthy error'
  );
  check(
    thrown.expected === true && thrown.message === 'explicit' && thrown.generatedMessage === false,
    'error metadata'
  );
}
const identity = new Error('identity');
let thrown;
try {
  assert(false, identity);
} catch (error) {
  thrown = error;
}
check(thrown === identity, 'message identity');

outcome('missing-ok', () => assert());

outcome('missing-equal', () => assert.equal(1));

outcome('coercive', () => assert.equal(1, '1'));

outcome('strict-coercion', () => assert.strictEqual(1, '1'));

outcome('nan', () => assert.strictEqual(NaN, NaN));

outcome('negative-zero', () => assert.strictEqual(0, -0));

outcome('strict-alias', () => strict.equal(1, '1'));

outcome('deep-loose', () => assert.deepEqual({ a: [1] }, { a: ['1'] }));

outcome('deep-strict', () => assert.deepStrictEqual({ a: [1] }, { a: ['1'] }));

outcome('sparse', () => assert.deepStrictEqual([,], [undefined]));

const a = {};
a.self = a;
const b = {};
b.self = b;

outcome('cycles', () => assert.deepStrictEqual(a, b));

const symbol = Symbol('key');

outcome('symbols', () => assert.deepStrictEqual({ [symbol]: 1 }, { [symbol]: 2 }));

outcome('maps', () =>
  assert.deepStrictEqual(new Map([[{ x: 1 }, 'a']]), new Map([[{ x: 1 }, 'a']]))
);

outcome('sets', () => assert.deepStrictEqual(new Set([1, 2]), new Set([2, 1])));

outcome('dates', () => assert.deepStrictEqual(new Date(100), new Date(101)));

outcome('buffers', () => assert.deepStrictEqual(Buffer.from([1, 2]), Buffer.from([1, 3])));

outcome('prototype', () =>
  assert.deepStrictEqual(Object.assign(Object.create(null), { x: 1 }), { x: 1 })
);

outcome('boxed', () => assert.deepStrictEqual(Object(1), Object(2)));

outcome('not-deep', () => assert.notDeepStrictEqual({ x: 1 }, { x: 1 }));

outcome('regexp', () => assert.deepStrictEqual(/a/gi, /a/g));

outcome('match', () => assert.match('hello', /ell/));

outcome('bad-match', () => assert.match('hello', /zzz/));

outcome('does-not-match', () => assert.doesNotMatch('hello', /zzz/));

outcome('bad-does-not-match', () => assert.doesNotMatch('hello', /ell/));

outcome('if-error', () => assert.ifError(null));

outcome('if-error-false', () => assert.ifError(false));

outcome('fail', () => assert.fail('explicit failure'));

const captured = new AssertionError({
  actual: 1,
  expected: 2,
  message: 'explicit',
  operator: 'strictEqual'
});
check(
  captured.code === 'ERR_ASSERTION' &&
    captured.name === 'AssertionError' &&
    captured.actual === 1 &&
    captured.expected === 2 &&
    captured.operator === 'strictEqual',
  'constructed metadata'
);

assert.throws(() => {
  throw new TypeError('bad input');
}, TypeError);

assert.throws(
  () => {
    throw Object.assign(new Error('bad input'), { code: 'BAD' });
  },
  { code: 'BAD', message: /bad/ }
);

assert.throws(
  () => {
    throw new Error('bad input');
  },
  (error) => error.message === 'bad input'
);

outcome('missing-throw', () => assert.throws(() => {}));

outcome('mismatch-throw', () =>
  assert.throws(() => {
    throw new Error('bad');
  }, /other/)
);

try {
  assert.throws(() => {
    throw identity;
  }, TypeError);
} catch (error) {
  check(error.code === 'ERR_ASSERTION' && error.actual === identity, 'mismatch assertion');
}

assert.doesNotThrow(() => 1);

await assert.rejects(Promise.reject(new TypeError('async bad')), TypeError);
await assert.rejects(async () => {
  throw identity;
}, /identity/);
await assert.doesNotReject(async () => 42);
let rejection;
try {
  await assert.rejects(async () => 42);
} catch (error) {
  rejection = error;
}
check(rejection?.code === 'ERR_ASSERTION' && rejection.operator === 'rejects', 'missing rejection');
for (const test of [
  () =>
    assert.throws(
      () => {
        throw identity;
      },
      () => {
        throw identity;
      }
    ),
  () =>
    assert.throws(
      () => {
        throw identity;
      },
      () => {
        throw new RangeError('validator');
      }
    )
]) {
  let caught;
  try {
    test();
  } catch (error) {
    caught = error;
  }
  check(caught === identity || caught instanceof RangeError, 'validator exception propagation');
}
try {
  await assert.rejects(Promise.reject(identity), () => {
    throw identity;
  });
} catch (error) {
  check(error === identity, 'async validator exception identity');
}
try {
  await assert.rejects(() => {
    throw identity;
  }, Error);
} catch (error) {
  check(error === identity, 'sync throw in rejects bypasses matcher');
}
for (const name of ['deepEqual', 'notDeepEqual', 'deepStrictEqual', 'notDeepStrictEqual']) {
  let error;
  try {
    assert[name](1);
  } catch (caught) {
    error = caught;
  }
  check(error?.code === 'ERR_MISSING_ARGS', 'missing deep argument ' + name);
}
const gc = typeof collectHostGarbage === 'function' ? collectHostGarbage : globalThis.gc;
gc?.();
assert.deepStrictEqual(a, b);
await assert.rejects(Promise.reject(identity), /identity/);
gc?.();

outcome('unwanted-throw', () =>
  assert.doesNotThrow(() => {
    throw identity;
  })
);

let unwanted;
try {
  await assert.doesNotReject(Promise.reject(identity));
} catch (error) {
  unwanted = error;
}
check(
  unwanted?.code === 'ERR_ASSERTION' &&
    unwanted.operator === 'doesNotReject' &&
    unwanted.actual === identity,
  'unwanted rejection metadata'
);
try {
  await assert.doesNotReject(Promise.reject(identity), TypeError);
} catch (error) {
  check(error === identity, 'unmatched no-rejection filter identity');
}

for (const entry of [assert, assert.ok, strict, strict.ok]) {
  for (const args of [[], [false], [false, undefined], [false, null]]) {
    let error;
    try {
      entry(...args);
    } catch (value) {
      error = value;
    }
    check(
      error?.code === 'ERR_ASSERTION' && error.generatedMessage === true,
      'automatic message metadata'
    );
  }
}
for (const name of [
  'equal',
  'notEqual',
  'deepEqual',
  'notDeepEqual',
  'deepStrictEqual',
  'notDeepStrictEqual'
]) {
  outcome(name + '-NaN', () => assert[name](NaN, NaN));

  if (name.includes('eep')) {
    outcome(name + '-nested-NaN', () => assert[name]({ value: NaN }, { value: NaN }));
  }
}
console.log(JSON.stringify(results));
