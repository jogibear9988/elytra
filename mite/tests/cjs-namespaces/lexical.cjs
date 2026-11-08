/** Exercise source-aware CJS names without executing comments, literals or computed exports. */
exports.default = 'inner-default';
exports['module.exports'] = 'inner-marker';
exports.dot = 1;
module.exports.direct = 2;
exports['bracket'] = 3;
exports['hyphen-name'] = 4;
exports['\u0065scaped'] = 5;
Object.defineProperty(exports, 'defined', { value: 6, enumerable: true });
Object.defineProperty(exports, 'getterOnly', {
  enumerable: true,

  get() {
    globalThis.getterOnlyReads = (globalThis.getterOnlyReads || 0) + 1;
    throw Error('getter only');
  }
});
exports.throwing = undefined;
Object.defineProperty(exports, 'throwing', {
  enumerable: true,

  get() {
    globalThis.throwingReads = (globalThis.throwingReads || 0) + 1;
    throw Error('throwing');
  }
});
const computed = 'computed';
exports[computed] = 7;
exports['dynamic' + 'Name'] = 8;
// exports.comment=1;
/* module.exports.blockComment=1; */
const text = 'exports.string=1';
const template = `exports.template=1`;
const regex = /exports.regex *=/;
