const privateValue = 42;
console.log(exports === module.exports, this === exports, require.main === module);
console.log(module.id, module.loaded, module.parent === null);
console.log(__filename.endsWith('scope.cjs'), __dirname.endsWith('commonjs'));
console.log(globalThis.privateValue === undefined);
