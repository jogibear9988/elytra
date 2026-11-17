/** Compare parser-required Buffer codec slices, including index coercion and bounds. */
const bytes = Buffer.from([97, 195, 169, 0, 255, 66]);
const cases = [];
const legacy = process.versions.node === '24.15.0';

function check(value, message) {
  if (!value) {
    throw new Error(message);
  }
}

for (const method of ['utf8Slice', 'latin1Slice', 'ucs2Slice', 'base64Slice']) {
  for (const [start, end] of [
    [undefined, undefined],
    [0, 6],
    [1, 4],
    [2.8, 5.2],
    ['1', '3'],
    [NaN, 3],
    [-1, 3],
    [1, 99],
    [Infinity, 3],
    [3, -1],
    [4, 2],
    [6, 6]
  ]) {
    let observed;
    try {
      observed = [method, 'value', bytes[method](start, end)];
    } catch (error) {
      observed = [method, error.name, error.code];
    }
    if (start === Infinity) {
      // Node 24.15.0 validates its adjusted end before returning empty; modern
      // Node and Mite return empty first. Keep both exact outcomes observable.
      check(
        JSON.stringify(observed) ===
          JSON.stringify(
            legacy ? [method, 'RangeError', 'ERR_OUT_OF_RANGE'] : [method, 'value', '']
          ),
        `${method} exact provider Infinity-start outcome`
      );
    }
    cases.push(observed);
  }
  for (const [start, end] of [
    [99, 3],
    [99, 99],
    [Infinity, Infinity]
  ]) {
    let thrown;
    let value;
    try {
      value = bytes[method](start, end);
    } catch (error) {
      thrown = error;
    }
    check(
      legacy
        ? thrown instanceof RangeError && thrown.code === 'ERR_OUT_OF_RANGE'
        : !thrown && value === '',
      `${method} empty range precedes upper bounds`
    );
  }
  check(
    Buffer.alloc(0)[method](Symbol('unused'), -1) === '',
    `${method} empty skips index coercion`
  );
}
console.log(JSON.stringify({ nodeVersion: process.versions.node ?? null, cases }));
console.log('DONE codec slices 48');
