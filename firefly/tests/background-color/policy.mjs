/** Controlled color policy only; real pixels are checked by the native component fixture. */
import assert from 'node:assert/strict';
import create from '../../core/window/background.mjs';

const error = (code, message) => Object.assign(new Error(message), { code });

const unsupported = (name) => {
  throw error('ERR_FIREFLY_UNSUPPORTED_API', name);
};

let current = 0xffffff;
let writes = 0;
const native = {
  lookupColor(name) {
    return { red: 0xff0000, blueviolet: 0x8a2be2 }[name] ?? null;
  },

  contentsBackground() {
    return current;
  },

  setContentsBackground(id, color) {
    assert.equal(id, 7);
    ++writes;
    current = color;
  }
};
const colors = create(native, error, unsupported);

for (const [value, expected] of [
  ['#abc', 0xaabbcc],
  ['#F123', 0x112233],
  ['#123456', 0x123456],
  ['#FF2468ac', 0x2468ac]
]) {
  assert.equal(colors.parse(value), expected);
}
console.log('PASS background opaque hex and Electron ARGB spelling');

for (const [value, expected] of [
  ['rgb(18, 52, 86)', 0x123456],
  ['rgba(18,52,86,1.0)', 0x123456],
  ['hsl(0,100%,50%)', 0xff0000],
  ['hsl(-120,100%,50%)', 0x0000ff],
  ['hsla(480,100%,50%,1)', 0x00ff00],
  ['red', 0xff0000],
  ['blueviolet', 0x8a2be2]
]) {
  assert.equal(colors.parse(value), expected, value);
}
console.log('PASS background opaque functional colors and native named lookup');

for (const value of ['', 'transparent', 'invalid-color', '#gg0000', 'rgb(no)', 'RED']) {
  assert.equal(colors.parse(value), 0xffffff, value);
}
for (const value of [null, undefined, 42, true, {}]) {
  assert.throws(() => colors.parse(value), { code: 'ERR_INVALID_ARG_TYPE' });
}
for (const value of ['#8123', '#80123456', 'rgba(1,2,3,0.5)', 'hsla(0,100%,50%,0)']) {
  assert.throws(() => colors.parse(value), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });
}
console.log('PASS background invalid strings use white and nonopaque colors fail explicitly');

colors.set(7, '#123456');
assert.equal(writes, 1);
assert.equal(colors.get(7), '#123456');
current = 0xabcdef;
assert.equal(colors.get(7), '#ABCDEF', 'Getter reads native state instead of a JS cache');

assert.throws(() => colors.set(7, '#80112233'), { code: 'ERR_FIREFLY_UNSUPPORTED_API' });

assert.equal(writes, 1, 'Invalid decisions never mutate the native color');
for (current of [-1, 0x1000000, 1.5, undefined]) {
  assert.throws(() => colors.get(7), { code: 'ERR_FIREFLY_COLOR_DATA' });
}
console.log('PASS background live native reads bounded metadata and mutation admission');
console.log('DONE background policy');
