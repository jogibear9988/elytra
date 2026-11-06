/** Compare parser-required Buffer codec slices, including index coercion and bounds. */
const bytes = Buffer.from([97, 195, 169, 0, 255, 66]);
const cases = [];
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
    try {
      cases.push([method, 'value', bytes[method](start, end)]);
    } catch (error) {
      cases.push([method, error.name, error.code]);
    }
  }
}
console.log(JSON.stringify(cases));
console.log('DONE codec slices 48');
